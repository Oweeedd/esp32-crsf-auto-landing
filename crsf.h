#ifndef CRSF_H
#define CRSF_H

#include <Arduino.h>

constexpr uint8_t CRSF_SYNC_BYTE = 0xC8;
constexpr uint32_t CRSF_BAUD_RATE = 416666;

constexpr uint8_t CRSF_FRAME_RC_CHANNELS = 0x16;

constexpr uint8_t CRSF_MAX_CHANNELS = 16;
constexpr uint8_t CRSF_CHANNEL_BITS = 11;
constexpr uint16_t CRSF_CHANNEL_MASK = 0x07FF;

constexpr uint8_t CRSF_RC_PAYLOAD_SIZE = 22;
constexpr uint8_t CRSF_RC_FRAME_LENGTH = 24;  // type + 22-byte payload + CRC
constexpr uint8_t CRSF_RC_FRAME_SIZE = 26;    // sync + length + frame length

constexpr uint8_t CRSF_MIN_FRAME_LENGTH = 2;
constexpr uint8_t CRSF_MAX_FRAME_LENGTH = 62;
constexpr uint8_t CRSF_FRAME_SIZE_MAX = 64;

#endif
