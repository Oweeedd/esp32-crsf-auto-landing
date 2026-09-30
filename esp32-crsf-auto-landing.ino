#include <Arduino.h>
#include "crsf.h"

// ESP32 UART pins.
constexpr int ELRS_RX_PIN = 16;  // ESP32 RX <- ELRS receiver TX
constexpr int ELRS_TX_PIN = 17;  // ESP32 TX -> ELRS receiver RX
constexpr int FC_RX_PIN   = 27;  // ESP32 RX <- flight controller TX
constexpr int FC_TX_PIN   = 26;  // ESP32 TX -> flight controller RX

HardwareSerial ELRS(1);
HardwareSerial FC(2);

// CRSF channels are zero-based here: channel 3 = index 2, channel 8 = index 7.
constexpr uint8_t THROTTLE_CHANNEL_INDEX = 2;
constexpr uint8_t LAND_CHANNEL_INDEX     = 7;

// AUX hysteresis in native CRSF channel units.
constexpr uint16_t AUX_ON_THRESHOLD  = 1500;
constexpr uint16_t AUX_OFF_THRESHOLD = 1400;

// 172 is approximately the standard CRSF low endpoint (~988 us).
constexpr uint16_t MIN_THROTTLE = 172;
constexpr float THROTTLE_DECREASE_PER_SECOND = 70.0f;

// Printing every RC frame can add unnecessary serial/CPU load at high packet rates.
constexpr bool ENABLE_RC_FRAME_LOG = false;

bool landingActive = false;
float landingThrottle = 0.0f;
uint32_t lastLandingMillis = 0;

uint8_t crsfCrc8(const uint8_t* data, uint8_t length) {
  uint8_t crc = 0;

  for (uint8_t i = 0; i < length; ++i) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc & 0x80) ? static_cast<uint8_t>((crc << 1) ^ 0xD5)
                         : static_cast<uint8_t>(crc << 1);
    }
  }

  return crc;
}

void unpackChannels(const uint8_t* payload, uint16_t* channels) {
  for (uint8_t i = 0; i < CRSF_MAX_CHANNELS; ++i) {
    const uint16_t bitPosition = static_cast<uint16_t>(CRSF_CHANNEL_BITS) * i;
    const uint8_t byteIndex = bitPosition >> 3;
    const uint8_t shift = bitPosition & 0x07;

    uint32_t raw = payload[byteIndex];
    if (byteIndex + 1 < CRSF_RC_PAYLOAD_SIZE) {
      raw |= static_cast<uint32_t>(payload[byteIndex + 1]) << 8;
    }
    if (byteIndex + 2 < CRSF_RC_PAYLOAD_SIZE) {
      raw |= static_cast<uint32_t>(payload[byteIndex + 2]) << 16;
    }

    channels[i] = static_cast<uint16_t>((raw >> shift) & CRSF_CHANNEL_MASK);
  }
}

void packChannels(const uint16_t* channels, uint8_t* payload) {
  memset(payload, 0, CRSF_RC_PAYLOAD_SIZE);

  for (uint8_t i = 0; i < CRSF_MAX_CHANNELS; ++i) {
    const uint32_t value = channels[i] & CRSF_CHANNEL_MASK;
    const uint16_t bitPosition = static_cast<uint16_t>(CRSF_CHANNEL_BITS) * i;
    const uint8_t byteIndex = bitPosition >> 3;
    const uint8_t shift = bitPosition & 0x07;

    payload[byteIndex] |= static_cast<uint8_t>((value << shift) & 0xFF);

    if (byteIndex + 1 < CRSF_RC_PAYLOAD_SIZE) {
      payload[byteIndex + 1] |= static_cast<uint8_t>((value >> (8 - shift)) & 0xFF);
    }

    if (shift > 5 && byteIndex + 2 < CRSF_RC_PAYLOAD_SIZE) {
      payload[byteIndex + 2] |= static_cast<uint8_t>((value >> (16 - shift)) & 0xFF);
    }
  }
}

bool hasValidCrsfCrc(const uint8_t* frame, uint8_t frameLength) {
  // frameLength is the CRSF length byte: type + payload + CRC.
  if (frameLength < CRSF_MIN_FRAME_LENGTH || frameLength > CRSF_MAX_FRAME_LENGTH) {
    return false;
  }

  const uint8_t expected = frame[frameLength + 1];
  const uint8_t calculated = crsfCrc8(&frame[2], frameLength - 1);
  return expected == calculated;
}

void updateLanding(uint16_t* channels) {
  const uint16_t aux = channels[LAND_CHANNEL_INDEX];

  if (landingActive && aux < AUX_OFF_THRESHOLD) {
    landingActive = false;
    Serial.println("[Landing] OFF");
  }

  if (!landingActive && aux > AUX_ON_THRESHOLD) {
    landingActive = true;
    landingThrottle = static_cast<float>(channels[THROTTLE_CHANNEL_INDEX]);
    lastLandingMillis = millis();
    Serial.printf("[Landing] ON (thr=%d)\n", static_cast<int>(landingThrottle));
  }

  if (!landingActive) {
    return;
  }

  const uint32_t now = millis();
  const float dt = static_cast<float>(now - lastLandingMillis) / 1000.0f;
  lastLandingMillis = now;

  landingThrottle -= THROTTLE_DECREASE_PER_SECOND * dt;
  if (landingThrottle < MIN_THROTTLE) {
    landingThrottle = MIN_THROTTLE;
  }

  channels[THROTTLE_CHANNEL_INDEX] = static_cast<uint16_t>(landingThrottle);
}

void printRcChannels(const uint16_t* channels) {
  if (!ENABLE_RC_FRAME_LOG) {
    return;
  }

  Serial.print("RC: ");
  for (uint8_t i = 0; i < CRSF_MAX_CHANNELS; ++i) {
    Serial.print(channels[i]);
    Serial.print(i + 1 == CRSF_MAX_CHANNELS ? '\n' : ' ');
  }
}

void setup() {
  Serial.begin(115200);
  ELRS.begin(CRSF_BAUD_RATE, SERIAL_8N1, ELRS_RX_PIN, ELRS_TX_PIN);
  FC.begin(CRSF_BAUD_RATE, SERIAL_8N1, FC_RX_PIN, FC_TX_PIN);

  Serial.println("[CRSF Auto Landing Bridge] started");
}

void loop() {
  static uint8_t elrsBuffer[CRSF_FRAME_SIZE_MAX];
  static uint8_t elrsIndex = 0;
  static uint8_t elrsFrameLength = 0;
  static bool elrsSync = false;

  while (ELRS.available()) {
    const uint8_t byte = ELRS.read();

    if (!elrsSync && byte == CRSF_SYNC_BYTE) {
      elrsIndex = 0;
      elrsSync = true;
    }

    if (!elrsSync) {
      continue;
    }

    if (elrsIndex >= sizeof(elrsBuffer)) {
      elrsSync = false;
      elrsIndex = 0;
      continue;
    }

    elrsBuffer[elrsIndex++] = byte;

    if (elrsIndex == 2) {
      elrsFrameLength = elrsBuffer[1];
      if (elrsFrameLength < CRSF_MIN_FRAME_LENGTH || elrsFrameLength > CRSF_MAX_FRAME_LENGTH) {
        elrsSync = false;
        elrsIndex = 0;
        continue;
      }
    }

    if (elrsIndex < elrsFrameLength + 2) {
      continue;
    }

    const bool isRcFrame =
        elrsBuffer[2] == CRSF_FRAME_RC_CHANNELS &&
        elrsFrameLength == CRSF_RC_FRAME_LENGTH;

    if (isRcFrame && hasValidCrsfCrc(elrsBuffer, elrsFrameLength)) {
      uint16_t channels[CRSF_MAX_CHANNELS];
      unpackChannels(&elrsBuffer[3], channels);

      updateLanding(channels);

      uint8_t payload[CRSF_RC_PAYLOAD_SIZE];
      packChannels(channels, payload);

      uint8_t frame[CRSF_RC_FRAME_SIZE];
      frame[0] = CRSF_SYNC_BYTE;
      frame[1] = CRSF_RC_FRAME_LENGTH;
      frame[2] = CRSF_FRAME_RC_CHANNELS;
      memcpy(&frame[3], payload, CRSF_RC_PAYLOAD_SIZE);
      frame[CRSF_RC_FRAME_SIZE - 1] = crsfCrc8(&frame[2], CRSF_RC_FRAME_LENGTH - 1);

      printRcChannels(channels);
      FC.write(frame, sizeof(frame));
    } else {
      // Non-RC frames and malformed RC frames are passed through unchanged.
      FC.write(elrsBuffer, elrsIndex);
    }

    elrsSync = false;
    elrsIndex = 0;
  }

  // FC -> ELRS telemetry passthrough.
  static uint8_t fcBuffer[CRSF_FRAME_SIZE_MAX];
  static uint8_t fcIndex = 0;
  static uint8_t fcFrameLength = 0;
  static bool fcSync = false;

  while (FC.available()) {
    const uint8_t byte = FC.read();

    if (!fcSync && byte == CRSF_SYNC_BYTE) {
      fcIndex = 0;
      fcSync = true;
    }

    if (!fcSync) {
      continue;
    }

    if (fcIndex >= sizeof(fcBuffer)) {
      fcSync = false;
      fcIndex = 0;
      continue;
    }

    fcBuffer[fcIndex++] = byte;

    if (fcIndex == 2) {
      fcFrameLength = fcBuffer[1];
      if (fcFrameLength < CRSF_MIN_FRAME_LENGTH || fcFrameLength > CRSF_MAX_FRAME_LENGTH) {
        fcSync = false;
        fcIndex = 0;
        continue;
      }
    }

    if (fcIndex >= fcFrameLength + 2) {
      ELRS.write(fcBuffer, fcIndex);
      fcSync = false;
      fcIndex = 0;
    }
  }
}
