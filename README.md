# ESP32 CRSF Auto Landing Bridge

Небольшой мост на ESP32 между **ExpressLRS/CRSF-приёмником** и **полётным контроллером**. ESP32 пропускает CRSF-трафик в обе стороны, а для кадров RC Channels Packed (`0x16`) может изменить только канал газа, реализуя простое плавное снижение газа по команде с AUX-канала.

Проект сделан как компактный Arduino-скетч без внешних библиотек помимо ESP32 Arduino Core.

## Что делает проект

Поток управления:

```text
ELRS receiver  --CRSF-->  ESP32  --CRSF-->  Flight Controller
ELRS receiver  <--telemetry-- ESP32 <--telemetry-- Flight Controller
```

ESP32:

1. принимает CRSF от ELRS-приёмника;
2. находит кадры `0x16 RC Channels Packed`;
3. распаковывает все 16 каналов;
4. проверяет AUX8;
5. при включении режима плавно уменьшает Channel 3 (throttle);
6. собирает корректный CRSF-кадр и пересчитывает CRC;
7. остальные кадры пропускает без изменения;
8. телеметрию от FC возвращает в сторону ELRS.

## Логика автопосадки

По умолчанию используются:

| Параметр | Значение |
|---|---:|
| Газ | CRSF Channel 3 (`index 2`) |
| Включение режима | CRSF Channel 8 (`index 7`) |
| AUX ON | `> 1500` |
| AUX OFF | `< 1400` |
| Минимум газа | `172` |
| Скорость снижения | `70 CRSF units/s` |

При переходе AUX8 выше порога включения ESP32 запоминает текущий газ и начинает линейно уменьшать его со скоростью `70` CRSF-единиц в секунду до значения `172`. Режим остаётся активным, пока AUX8 не опустится ниже `1400`.

Это **не полноценный алгоритм автоматической посадки**: проект не использует высоту, вертикальную скорость или состояние ARM. Он только формирует плавный ramp по каналу газа.

## Подключение

Используются два аппаратных UART ESP32.

| Сторона | ESP32 GPIO | Направление |
|---|---:|---|
| ELRS TX | GPIO16 | ELRS → ESP32 RX |
| ELRS RX | GPIO17 | ESP32 TX → ELRS |
| FC TX | GPIO27 | FC → ESP32 RX |
| FC RX | GPIO26 | ESP32 TX → FC |

Также нужна общая земля между ESP32, ELRS-приёмником и полётным контроллером.

CRSF использует UART `8N1` со скоростью `416666 baud`. Линии CRSF рассчитаны на логические уровни порядка 3.3 В; перед подключением конкретного оборудования проверьте уровни его UART.

## Схема соединения

```text
             ESP32
        ┌──────────────┐
ELRS TX ──> GPIO16 RX1 │
ELRS RX <── GPIO17 TX1 │
        │              │
FC TX   ──> GPIO27 RX2 │
FC RX   <── GPIO26 TX2 │
        └──────────────┘

GND ELRS ─┬─ GND ESP32
GND FC   ─┘
```

## Сборка в Arduino IDE

1. Установить поддержку плат ESP32 в Arduino IDE.
2. Открыть папку проекта.
3. Открыть `esp32-crsf-auto-landing.ino`.
4. Выбрать свою ESP32-плату.
5. Собрать и прошить скетч.
6. Для диагностики открыть Serial Monitor на `115200 baud`.

Внешние Arduino-библиотеки проекту не требуются.

## Основные настройки

Все параметры автопосадки находятся в начале `esp32-crsf-auto-landing.ino`:

```cpp
constexpr uint8_t THROTTLE_CHANNEL_INDEX = 2;
constexpr uint8_t LAND_CHANNEL_INDEX     = 7;
constexpr uint16_t AUX_ON_THRESHOLD      = 1500;
constexpr uint16_t AUX_OFF_THRESHOLD     = 1400;
constexpr uint16_t MIN_THROTTLE          = 172;
constexpr float THROTTLE_DECREASE_PER_SECOND = 70.0f;
```

Для вывода всех 16 каналов в Serial Monitor можно включить:

```cpp
constexpr bool ENABLE_RC_FRAME_LOG = true;
```

На высокой частоте RC-пакетов постоянный вывод в Serial создаёт лишнюю нагрузку, поэтому по умолчанию он выключен.

## CRSF

В проекте используется стандартный CRSF RC frame `0x16`:

- 16 каналов;
- 11 бит на канал;
- 22 байта payload;
- CRC-8/DVB-S2, polynomial `0xD5`;
- типичный low endpoint около `172`;
- center около `992`.

Код модифицирует только Channel 3. Остальные 15 каналов сохраняются.

Подробнее: [docs/CRSF.md](docs/CRSF.md).

## Ограничения

- Парсер синхронизируется по `0xC8`, как это делал исходный вариант и как это обычно используется на линии CRSF к полётному контроллеру.
- Нет watchdog/failsafe-логики внутри ESP32.
- Нет контроля высоты или факта касания земли.
- Нет автоматического disarm.
- Изменяется только обычный `0x16 RC Channels Packed`; остальные кадры проходят транзитом.


## Структура

```text
esp32-crsf-auto-landing/
├── esp32-crsf-auto-landing.ino
├── crsf.h
├── README.md
├── README.en.md
├── CHANGELOG.md
├── .gitignore
├── docs/
│   └── CRSF.md
└── tests/
    └── test_crsf_codec.py
```
