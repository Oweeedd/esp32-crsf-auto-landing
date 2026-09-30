#!/usr/bin/env python3
"""Host-side regression tests for the CRSF 16-channel pack/unpack logic."""

from __future__ import annotations

import random

CHANNELS = 16
CHANNEL_BITS = 11
CHANNEL_MASK = 0x07FF
PAYLOAD_SIZE = 22


def pack_channels(channels: list[int]) -> bytearray:
    assert len(channels) == CHANNELS
    payload = bytearray(PAYLOAD_SIZE)

    for i, channel in enumerate(channels):
        value = channel & CHANNEL_MASK
        bit_position = CHANNEL_BITS * i
        byte_index = bit_position >> 3
        shift = bit_position & 0x07

        payload[byte_index] |= (value << shift) & 0xFF
        if byte_index + 1 < PAYLOAD_SIZE:
            payload[byte_index + 1] |= (value >> (8 - shift)) & 0xFF
        if shift > 5 and byte_index + 2 < PAYLOAD_SIZE:
            payload[byte_index + 2] |= (value >> (16 - shift)) & 0xFF

    return payload


def unpack_channels(payload: bytes) -> list[int]:
    assert len(payload) == PAYLOAD_SIZE
    channels: list[int] = []

    for i in range(CHANNELS):
        bit_position = CHANNEL_BITS * i
        byte_index = bit_position >> 3
        shift = bit_position & 0x07

        raw = payload[byte_index]
        if byte_index + 1 < PAYLOAD_SIZE:
            raw |= payload[byte_index + 1] << 8
        if byte_index + 2 < PAYLOAD_SIZE:
            raw |= payload[byte_index + 2] << 16

        channels.append((raw >> shift) & CHANNEL_MASK)

    return channels


def crc8_dvb_s2(data: bytes) -> int:
    crc = 0
    for value in data:
        crc ^= value
        for _ in range(8):
            crc = ((crc << 1) ^ 0xD5) & 0xFF if crc & 0x80 else (crc << 1) & 0xFF
    return crc


def test_known_channel_vector() -> None:
    channels = [172, 992, 1811, 1000, 1200, 1400, 1600, 1800,
                200, 400, 600, 800, 1000, 1200, 1400, 1600]
    assert unpack_channels(pack_channels(channels)) == channels


def test_random_roundtrip() -> None:
    rng = random.Random(0xC85F)
    for _ in range(5000):
        channels = [rng.randrange(0, 2048) for _ in range(CHANNELS)]
        assert unpack_channels(pack_channels(channels)) == channels


def test_channels_9_to_16_are_preserved() -> None:
    channels = [992] * 8 + [172, 300, 600, 900, 1200, 1500, 1700, 1811]
    result = unpack_channels(pack_channels(channels))
    assert result[8:] == channels[8:]


def test_crc_known_vector() -> None:
    # 0x16 + 16 centered channels (992) encoded as a 22-byte payload.
    payload = pack_channels([992] * CHANNELS)
    frame_without_crc = bytes([0x16]) + payload
    assert payload.hex() == "e0031ff8c0073ef0810f7ce0031ff8c0073ef0810f7c"
    assert crc8_dvb_s2(frame_without_crc) == 0xAD


def main() -> None:
    tests = [
        test_known_channel_vector,
        test_random_roundtrip,
        test_channels_9_to_16_are_preserved,
        test_crc_known_vector,
    ]

    for test in tests:
        test()
        print(f"PASS: {test.__name__}")

    print(f"PASS: {len(tests)} tests")


if __name__ == "__main__":
    main()
