#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ASHClient.h>
#include <Command.h>
#include <ConnectInfo.h>
#include <DHT.h>

ASHClient net;

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDR 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Date and time
bool dtSynced = false;
unsigned long dtBase = 0;
unsigned long dtSyncMillis = 0;
unsigned long dtLastRequest = 0;

// Date and time timer
const unsigned long DT_SYNC_INTERVAL = 60000;
const unsigned long DT_RETRY_INTERVAL = 5000;

// display timer
const unsigned long DISPLAY_INTERVAL = 250;
unsigned long displayLastDraw = 0;

const char *WEEKDAYS[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
const char *MONTHS[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

// DHT11 sensor

DHT dht(GPIO_NUM_0, DHT11);
const unsigned long DHT_READ_INTERVAL = 120000;
unsigned long dhtLastRead = 0;
float temperature = NAN;
float humidity = NAN;

void updateDHT()
{
    unsigned long now = millis();
    if (now - dhtLastRead >= DHT_READ_INTERVAL)
    {
        dhtLastRead = now;

        humidity = dht.readHumidity();
        temperature = dht.readTemperature();

        if (isnan(temperature) || isnan(humidity))
        {
            net.send(Command::log("temperature or humidity is not a number").toString());
        }
        else
        {
            net.send(Command::setValue("esp_oled_temperature", String(temperature, 1)).toString());
            net.send(Command::setValue("esp_oled_humidity", String(humidity, 1)).toString());
        }
    }
}

unsigned long currentUnixTime()
{
    return dtBase + (millis() - dtSyncMillis) / 1000;
}

void updateDT()
{
    unsigned long now = millis();
    unsigned long interval = dtSynced ? DT_SYNC_INTERVAL : DT_RETRY_INTERVAL;
    if (now - dtLastRequest >= interval)
    {
        dtLastRequest = now;
        net.send(Command::message("cur_dt").toString());
    }
}

void drawCentered(const char *text, int size, int y)
{
    int w = strlen(text) * 6 * size;
    display.setTextSize(size);
    display.setCursor((SCREEN_WIDTH - w) / 2, y);
    display.print(text);
}

static void drawWifiIcon(int x, int y)
{
    bool connected = (WiFi.status() == WL_CONNECTED);
    int bars = 0;
    if (connected)
    {
        int rssi = WiFi.RSSI();
        bars = rssi >= -55 ? 4 : rssi >= -65 ? 3
                             : rssi >= -75   ? 2
                                             : 1;
    }

    for (int i = 0; i < 4; i++)
    {
        int h = 2 + i * 2;
        int bx = x + i * 4;
        if (i < bars)
            display.fillRect(bx, y + 8 - h, 3, h, SSD1306_WHITE);
        else
            display.drawFastHLine(bx, y + 7, 3, SSD1306_WHITE);
    }

    if (!connected)
        display.drawLine(x, y + 7, x + 14, y, SSD1306_WHITE);
}

static void drawServerIcon(int x, int y, bool connected)
{
    if (connected)
    {
        display.fillRect(x, y, 10, 3, SSD1306_WHITE);
        display.fillRect(x, y + 5, 10, 3, SSD1306_WHITE);
    }
    else
    {
        display.drawRect(x, y, 10, 3, SSD1306_WHITE);
        display.drawRect(x, y + 5, 10, 3, SSD1306_WHITE);
        display.drawLine(x, y + 7, x + 9, y, SSD1306_WHITE);
    }
}

static void drawClimate(int y)
{
    char buf[16];
    display.setTextSize(1);

    display.setCursor(2, y);
    if (isnan(temperature))
    {
        display.print("T --");
    }
    else
    {
        snprintf(buf, sizeof(buf), "T %.1f", temperature);
        display.print(buf);
        display.write(247); // %
        display.print("C");
    }

    if (isnan(humidity))
        snprintf(buf, sizeof(buf), "H --");
    else
        snprintf(buf, sizeof(buf), "H %.0f%%", humidity);
    display.setCursor(display.width() - 2 - strlen(buf) * 6, y);
    display.print(buf);
}

void updateDisplay()
{
    unsigned long now = millis();
    if (now - displayLastDraw < DISPLAY_INTERVAL)
        return;
    displayLastDraw = now;

    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);

    drawWifiIcon(0, 1);
    drawServerIcon(21, 1, net.connected());

    char buf[32];

    if (dtSynced)
    {
        time_t t = (time_t)currentUnixTime();
        struct tm tm;
        gmtime_r(&t, &tm);

        snprintf(buf, sizeof(buf), "%s %d %s %d",
                 WEEKDAYS[tm.tm_wday], tm.tm_mday, MONTHS[tm.tm_mon], tm.tm_year + 1900);
        display.setTextSize(1);
        display.setCursor(display.width() - strlen(buf) * 6, 1);
        display.print(buf);

        display.drawFastHLine(0, 11, display.width(), SSD1306_WHITE);

        snprintf(buf, sizeof(buf), "%02d%c%02d",
                 tm.tm_hour, (tm.tm_sec % 2 == 0) ? ':' : ' ', tm.tm_min);
        display.setTextSize(3);
        display.setCursor(2, 16);
        display.print(buf);

        snprintf(buf, sizeof(buf), "%02d", tm.tm_sec);
        display.setTextSize(2);
        display.setCursor(102, 24);
        display.print(buf);

        drawClimate(45);

        int w = (display.width() - 4) * tm.tm_sec / 59;
        display.drawRect(0, 56, display.width(), 5, SSD1306_WHITE);
        display.fillRect(2, 58, w, 1, SSD1306_WHITE);
    }
    else
    {
        display.drawFastHLine(0, 11, display.width(), SSD1306_WHITE);
        drawCentered("Syncing...", 2, 32);
    }

    display.display();
}

void onConnect()
{
    Serial.println("connected");
    net.send("GREETING esp_oled");
    dtLastRequest = millis();
    net.send(Command::message("cur_dt").toString());
}

void onDisconnect()
{
    Serial.println("disconnected");
}

void onMessage(const String &line)
{
    Command cmd;
    ParseError err;
    if (!Command::parse(line, cmd, &err))
    {
        Serial.println("received bad command: " + err.message);
        return;
    }
    if (cmd.type == Command::Type::Ping)
    {
        net.send(Command::pong().toString());
    }
    else if (cmd.type == Command::Type::SetValue)
    {
        if (cmd.key == "cur_dt")
        {
            dtBase = strtoul(cmd.value.c_str(), NULL, 10);
            dtSyncMillis = millis();
            dtSynced = true;
        }
    }
}

void setup()
{
    Serial.begin(115200);
    Wire.begin(SDA, SCL); // SDA 8, SCL 9

    if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR))
    {
        Serial.println("SSD1306 not found");
        for (;;)
            delay(1000);
    }

    dht.begin();

    temperature = dht.readTemperature();
    humidity = dht.readHumidity();

    if (isnan(temperature) || isnan(humidity))
    {
        net.send(Command::log("temperature or humidity is not a number").toString());
    }
    else
    {
        net.send(Command::setValue("esp_oled_temperature", String(temperature, 1)).toString());
        net.send(Command::setValue("esp_oled_humidity", String(humidity, 1)).toString());
    }

    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.display();

    net.begin(WIFI_SSID, WIFI_PASS, SERVER_HOST, SERVER_PORT);

    net.onConnect(onConnect);
    net.onDisconnect(onDisconnect);
    net.onMessage(onMessage);
}

void loop()
{
    net.update();
    updateDHT();
    updateDT();
    updateDisplay();
}