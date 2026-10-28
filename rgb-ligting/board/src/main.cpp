#include <Arduino.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

// Настройки
const uint8_t PIN_R = 0;
const uint8_t PIN_G = 2;
const uint8_t PIN_B = 1;
const bool COMMON_ANODE = false;
const uint32_t BAUD = 115200;
const uint16_t FRAME_MS = 10;

struct Color
{
  float r, g, b;
};

static Color mix(const Color &a, const Color &b, float t)
{
  Color c;
  c.r = a.r + (b.r - a.r) * t;
  c.g = a.g + (b.g - a.g) * t;
  c.b = a.b + (b.b - a.b) * t;
  return c;
}

Color baseFrom = {0, 0, 0};
Color baseTo = {0, 0, 0};
uint32_t baseStart = 0;
uint32_t baseDur = 0;

struct Effect
{
  bool active;
  Color from;
  Color color;
  uint32_t start;
  uint32_t attack, hold, release;
  uint32_t repeats;
} fx = {false, {0, 0, 0}, {0, 0, 0}, 0, 0, 0, 0, 1};

Color current = {0, 0, 0};

static Color baseNow(uint32_t now)
{
  uint32_t el = now - baseStart;
  if (baseDur == 0 || el >= baseDur)
    return baseTo;
  return mix(baseFrom, baseTo, (float)el / (float)baseDur);
}

static Color render(uint32_t now)
{
  Color base = baseNow(now);
  if (!fx.active)
    return base;

  uint32_t cycle = fx.attack + fx.hold + fx.release;
  uint32_t el = now - fx.start;

  // Нулевая длительность или все повторы отработаны
  if (cycle == 0 || el / cycle >= fx.repeats)
  {
    fx.active = false;
    return base;
  }

  Color from = (el < cycle) ? fx.from : base;
  el %= cycle;

  if (el < fx.attack)
    return mix(from, fx.color, (float)el / (float)fx.attack);
  el -= fx.attack;

  if (el < fx.hold)
    return fx.color;
  el -= fx.hold;

  return mix(fx.color, base, (float)el / (float)fx.release);
}

const uint8_t PINS[3] = {PIN_R, PIN_G, PIN_B};

#if defined(ARDUINO_ARCH_ESP32) && !(defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3)
#define LEGACY_ESP32_LEDC 1
#endif

static void pwmBegin()
{
  for (uint8_t i = 0; i < 3; i++)
  {
#ifdef LEGACY_ESP32_LEDC
    ledcSetup(i, 5000, 8); // канал i, 5 кГц, 8 бит
    ledcAttachPin(PINS[i], i);
#else
    pinMode(PINS[i], OUTPUT);
#endif
  }
}

static void writeChannel(uint8_t i, float v)
{
  if (v < 0)
    v = 0;
  if (v > 1)
    v = 1;
  uint8_t pwm = (uint8_t)(v * v * 255.0f + 0.5f);
  if (COMMON_ANODE)
    pwm = 255 - pwm;
#ifdef LEGACY_ESP32_LEDC
  ledcWrite(i, pwm);
#else
  analogWrite(PINS[i], pwm);
#endif
}

static void writeLed(const Color &c)
{
  writeChannel(0, c.r);
  writeChannel(1, c.g);
  writeChannel(2, c.b);
}

static void setBase(const Color &c, uint32_t ms)
{
  uint32_t now = millis();
  baseFrom = baseNow(now);
  baseTo = c;
  baseStart = now;
  baseDur = ms;
}

static void startEffect(const Color &color, uint32_t attack, uint32_t hold, uint32_t release, uint32_t repeats)
{
  fx.from = current;
  fx.color = color;
  fx.attack = attack;
  fx.hold = hold;
  fx.release = release;
  fx.repeats = repeats;
  fx.start = millis();
  fx.active = true;
}

static void ok() { Serial.println(F("OK")); }
static void err(const __FlashStringHelper *m)
{
  Serial.print(F("ERR "));
  Serial.println(m);
}

static bool toLong(const char *s, long lo, long hi, long &out)
{
  char *end;
  long v = strtol(s, &end, 10);
  if (end == s || *end != '\0' || v < lo || v > hi)
    return false;
  out = v;
  return true;
}

static bool parseColor(char **t, Color &c)
{
  long r, g, b;
  if (!toLong(t[0], 0, 255, r) || !toLong(t[1], 0, 255, g) || !toLong(t[2], 0, 255, b))
    return false;
  c.r = r / 255.0f;
  c.g = g / 255.0f;
  c.b = b / 255.0f;
  return true;
}

const uint32_t MAX_MS = 60000;
const uint8_t MAX_TOK = 10;
const uint32_t MAX_REPEATS = 10000;

static void handleLine(char *line)
{
  char *t[MAX_TOK];
  uint8_t n = 0;
  for (char *p = strtok(line, " \t,;"); p && n < MAX_TOK; p = strtok(NULL, " \t,;"))
    t[n++] = p;
  if (n == 0)
    return;
  for (char *c = t[0]; *c; ++c)
    *c = toupper(*c);

  Color col;
  long attack, hold, release, id, ms, repeats;

  if (!strcmp(t[0], "BASE"))
  {
    ms = 0;
    if ((n != 4 && n != 5) || !parseColor(&t[1], col) ||
        (n == 5 && !toLong(t[4], 0, MAX_MS, ms)))
      return err(F("usage: BASE r g b [ms]"));
    setBase(col, ms);
    ok();
  }
  else if (!strcmp(t[0], "FX"))
  {
    repeats = 1;
    if ((n != 7 && n != 8) || !parseColor(&t[1], col) ||
        !toLong(t[4], 0, MAX_MS, attack) ||
        !toLong(t[5], 0, MAX_MS, hold) ||
        !toLong(t[6], 0, MAX_MS, release) ||
        (n == 8 && !toLong(t[7], 1, MAX_REPEATS, repeats)))
      return err(F("usage: FX r g b attack hold release [repeats]"));
    startEffect(col, attack, hold, release, repeats);
    ok();
  }
  else if (!strcmp(t[0], "PING"))
  {
    Serial.println(F("PONG"));
  }
  else
  {
    err(F("unknown command"));
  }
}

char lineBuf[64];
uint8_t lineLen = 0;
bool lineOverflow = false;

static void readSerial()
{
  while (Serial.available() > 0)
  {
    char c = (char)Serial.read();
    if (c == '\r')
      continue;
    if (c == '\n')
    {
      if (lineOverflow)
      {
        err(F("line too long"));
      }
      else
      {
        lineBuf[lineLen] = '\0';
        handleLine(lineBuf);
      }
      lineLen = 0;
      lineOverflow = false;
    }
    else if (lineLen < sizeof(lineBuf) - 1)
    {
      lineBuf[lineLen++] = c;
    }
    else
    {
      lineOverflow = true;
    }
  }
}

uint32_t lastFrame = 0;

void setup()
{
  pwmBegin();
  Serial.begin(BAUD);

  Color startColor = {0.0f, 0.0f, 1.0f};
  baseFrom = baseTo = current = startColor;
  writeLed(current);
}

void loop()
{
  readSerial();

  uint32_t now = millis();
  if (now - lastFrame >= FRAME_MS)
  {
    lastFrame = now;
    current = render(now);
    writeLed(current);
  }
}
