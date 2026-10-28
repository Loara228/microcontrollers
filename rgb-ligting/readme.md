
# Board

Контроллер получает команды по USB и меняет цвет светодиодов. Для минамальной сборки требуется сам микроконтроллер и RGB светодиод.

<img src="./led-client/pictures/IMG_2844.jpg" width="800">

<img src="./led-client/pictures/IMG_2843.jpg" width="800">

## Настройка

Настройка микроконтроллера в [main.cpp](./board/src/main.cpp)

```cpp
const uint8_t PIN_R = 0; // ПИН для красного цвета
const uint8_t PIN_G = 2; // ПИН для зеленого цвета
const uint8_t PIN_B = 1; // ПИН для синего цвета
const bool COMMON_ANODE = false; // true - светодиод с общим анодом (+)
const uint32_t BAUD = 115200; // Скорость работы serial port. Она должна быть одинаковой с обеих сторон
const uint16_t FRAME_MS = 10; // Задержка цикла loop
```

Для идентификации порта можно запустить скрипт [линукс](../get-path-linux) [macos](../get-path-macos)

## Examples

Минимальный пример - [basic.rs](./led-client/examples/basic.rs)

### Простой пример Axum + LED

Простой пример axum сервера, который перенаправляет полученные команды в микроконтроллер

1. Настройте [http клиент](./led-client/examples/http.rs)

    ```rs
    const BAUD: u32 = 115200; // Скорость работы serial port. Она должна быть одинаковой с обеих сторон
    const PATH: &'static str = "/dev/ttyACM0"; // порт для комуникации
    const DELAY: Duration = Duration::from_millis(50); // Задержка между открытием порта и началом работы. Duration::ZERO работает через раз на ESP32
    const ADDR: &'static str = "127.0.0.1:5678"; // IP:PORT
    ```

2. Подключете микроконтроллер по USB к ПК
3. Запустите http клиент с помощью команды `cargo run --example http`
4. Отправьте команду с помощью [test.sh](./led-client/examples/test.sh) или запустите [demo.sh](./led-client/examples/demo.sh)

### Dota lighting

[**ВИДЕО НА YOUTUBE**](https://youtu.be/T69do24J16Q)

Динамическая подсветка монитора для игры Dota 2.  Реагирует на события:

#### FX-эффекты

Каждый эффект проходит три фазы: **Attack** (нарастание), **Hold** (удержание), **Release** (затухание) и повторяется заданное число раз.

| Событие | Цвет | RGB | Attack | Hold | Release | Повторы |
|---|:---:|---|--:|--:|--:|:--:|
| `Убийство` | ![#000000](https://placehold.co/14x14/000000/000000.png) | 0, 0, 0 | 60 мс | 30 мс | 60 мс | ×4 |
| `Помощь в убийстве` | ![#23FF16](https://placehold.co/14x14/23FF16/23FF16.png) | 35, 255, 22 | 200 мс | 300 мс | 400 мс | ×1 |
| `Смерть` | ![#FF0000](https://placehold.co/14x14/FF0000/FF0000.png) | 255, 0, 0 | 200 мс | 60 с | 60 с | ×99 |
| `Возраждение` | ![#23FF16](https://placehold.co/14x14/23FF16/23FF16.png) | 35, 255, 22 | 200 мс | 1 с | 3 с | ×1 |
| `Поднятие уровня` | ![#FFFF1E](https://placehold.co/14x14/FFFF1E/FFFF1E.png) | 255, 255, 30 | 100 мс | 1 с | 2 с | ×1 |
| `При использовании smoke of deceit` | ![#FF00FF](https://placehold.co/14x14/FF00FF/FF00FF.png) | 255, 0, 255 | 200 мс | 60 с | 200 мс | ×99 |
| `При развеивании smoke of deceit` | ![#FF0000](https://placehold.co/14x14/FF0000/FF0000.png) | 255, 0, 0 | 100 мс | 1 с | 500 мс | ×1 |

#### BASE-эффекты (базовая подсветка)

| Событие | Цвет | RGB | Длительность |
|---|:---:|---|--:|
| `День` | ![#7AFDC2](https://placehold.co/14x14/7AFDC2/7AFDC2.png) | 122, 253, 194 | 2 с |
| `Ночь` | ![#640FFF](https://placehold.co/14x14/640FFF/640FFF.png) | 100, 15, 255 | 2 с |

#### Запуск 

1. Настройте клиент в [dota_lighting.rs](./led-client/examples/dota_lighting.rs)

    ```rs
    const BAUD: u32 = 115200;
    const PORT: &'static str = "/dev/cu.usbmodem101";
    const TIMEOUT: Duration = Duration::from_millis(50);
    ```

2. Подключете микроконтроллер по USB к ПК
3. Запустите с помощью cargo run --example `dota_lighting`
4. Запустите игру


## Команды и аргументы

- **BASE** - Установка базового цвета
    - `R`, `G`, `B` - Аргументы для установки цвета (0-255)
    - `ms` - Длительность перехода (мс)

- **FX** - Выполнение эффекта
    - `R`, `G`, `B` - Аргументы для установки цвета (0-255)
    - `ATTACK` - Переход от базового цвета к цвету эффекта (мс)
    - `HOLD` - Длительность эффекта (мс)
    - `RELEASE` - Переход от цвета эффекта к базовому (мс)
    - `REPEATS` - Количество повторений

- **PING** - Возвращает PONG
