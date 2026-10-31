#pragma once

#include <stddef.h>
#include <stdint.h>

// CRC-8 used for SMBus Packet Error Checking (polynomial x^8 + x^2 + x + 1)
inline uint8_t smbusPec(const uint8_t *data, size_t length) {
  uint8_t crc = 0;
  while (length--) {
    crc ^= *data++;
    for (uint8_t bit = 0; bit < 8; bit++) {
      crc = (crc & 0x80) ? static_cast<uint8_t>((crc << 1) ^ 0x07) : static_cast<uint8_t>(crc << 1);
    }
  }
  return crc;
}
