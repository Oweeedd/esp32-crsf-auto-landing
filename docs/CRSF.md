# CRSF notes used by this project

This firmware implements only the part of CRSF required for an inline RC-channel bridge.

## UART

```text
416666 baud
8 data bits
no parity
1 stop bit
full-duplex UART
```

## Standard RC frame

The project modifies frame type `0x16` (`RC Channels Packed`).

```text
Byte 0     Sync/address
Byte 1     Length = 24
Byte 2     Type = 0x16
Byte 3..24 22-byte packed channel payload
Byte 25    CRC
```

The payload contains 16 channels × 11 bits = 176 bits = 22 bytes.

CRSF frame length does not include the first two bytes (`sync` and `length`). For a normal RC frame it therefore equals:

```text
1 byte type + 22 bytes payload + 1 byte CRC = 24
```

Total frame size is 26 bytes.

## Channel representation

Standard `0x16` channels are 11-bit values (`0..2047`). Common CRSF values are approximately:

```text
low    172
center 992
high   1811
```

The firmware works directly in CRSF units and does not convert channels to microseconds.

## CRC

The normal CRSF CRC is CRC-8/DVB-S2 with polynomial `0xD5`.

CRC covers:

```text
Type + Payload
```

It does not include the sync/address byte or length byte.

## Bridge behavior

### ELRS -> FC

- non-`0x16` frames: forwarded unchanged;
- malformed/CRC-invalid `0x16` frames: forwarded unchanged;
- valid `0x16` frames: decoded, optionally modify Channel 3, re-packed, CRC recalculated.

### FC -> ELRS

Frames are forwarded without payload modification.

## Why all 16 channels must be decoded

A `0x16` frame always carries 16 channels in one 22-byte payload. Rebuilding that payload from only Channels 1–8 clears the bits that contain Channels 9–16. Therefore a bridge that changes only one channel still has to preserve the complete 16-channel payload.

## References

- TBS CRSF public specification: `https://github.com/tbs-fpv/tbs-crsf-spec`
- ExpressLRS documentation: `https://www.expresslrs.org/`
