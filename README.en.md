# ESP32 CRSF Auto Landing Bridge

A small ESP32 inline bridge between an **ExpressLRS/CRSF receiver** and a **flight controller**. It forwards CRSF traffic in both directions and modifies only the throttle channel in standard `0x16 RC Channels Packed` frames when an AUX-controlled throttle ramp is enabled.

## Data path

```text
ELRS receiver  --CRSF-->  ESP32  --CRSF-->  Flight Controller
ELRS receiver  <--telemetry-- ESP32 <--telemetry-- Flight Controller
```

The firmware unpacks all 16 CRSF channels, watches Channel 8, and when enabled gradually decreases Channel 3 (throttle) to the configured minimum. All other RC channels are preserved.

This is not a complete autonomous landing controller. It does not use altitude, vertical speed, arming state, or touchdown detection; it only applies a linear throttle ramp.

## Default configuration

| Parameter | Value |
|---|---:|
| Throttle | CRSF Channel 3 (`index 2`) |
| Landing switch | CRSF Channel 8 (`index 7`) |
| AUX ON | `> 1500` |
| AUX OFF | `< 1400` |
| Minimum throttle | `172` |
| Decrease rate | `70 CRSF units/s` |

## Wiring

| Device signal | ESP32 GPIO |
|---|---:|
| ELRS TX → ESP32 RX | GPIO16 |
| ESP32 TX → ELRS RX | GPIO17 |
| FC TX → ESP32 RX | GPIO27 |
| ESP32 TX → FC RX | GPIO26 |

Use a common ground between all devices. CRSF is configured as `416666 baud`, `8N1`.

## Build

1. Install ESP32 board support in Arduino IDE.
2. Open `esp32-crsf-auto-landing.ino`.
3. Select the appropriate ESP32 board.
4. Build and upload.
5. Use Serial Monitor at `115200 baud` for diagnostics.

No third-party Arduino libraries are required.

## Fix compared with the original sketch

The original implementation decoded only eight channels but rebuilt the entire 22-byte CRSF RC payload, which cleared Channels 9–16. This version processes all 16 channels and preserves untouched channels.

It also validates RC-frame CRC and CRSF frame length before modifying a packet and disables per-frame serial logging by default.

## Repository layout

```text
esp32-crsf-auto-landing/
├── esp32-crsf-auto-landing.ino
├── crsf.h
├── README.md
├── README.en.md
├── CHANGELOG.md
├── .gitignore
├── docs/CRSF.md
└── tests/test_crsf_codec.py
```

## License

No license has been selected yet.
