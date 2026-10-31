#include "Mlx90614.h"

#include <SmbusPec.h>

#define MLX_REG_AMBIENT 0x06
#define MLX_REG_OBJECT 0x07

// Temperatures in RAM use 15 bits, a set top bit means the value is not valid
#define MLX_MAX_RAW 0x7FFF

bool Mlx90614::readTemperature(const uint8_t reg, float &celsius) {
  _bus.beginTransmission(_address);
  _bus.write(reg);
  if (_bus.endTransmission(false) != 0) {
    return false;
  }

  if (_bus.requestFrom(_address, static_cast<size_t>(3), true) != 3) {
    return false;
  }
  const uint8_t lsb = _bus.read();
  const uint8_t msb = _bus.read();
  const uint8_t pec = _bus.read();

  // The checksum covers the whole transaction, including both address bytes
  const uint8_t frame[] = {
      static_cast<uint8_t>(_address << 1), reg,
      static_cast<uint8_t>((_address << 1) | 1), lsb, msb};
  if (smbusPec(frame, sizeof(frame)) != pec) {
    return false;
  }

  const uint16_t raw = static_cast<uint16_t>(msb) << 8 | lsb;
  if (raw > MLX_MAX_RAW) {
    return false;
  }

  celsius = raw * 0.02f - 273.15f;
  return true;
}

bool Mlx90614::read(float &objectC, float &ambientC) {
  return readTemperature(MLX_REG_OBJECT, objectC) &&
         readTemperature(MLX_REG_AMBIENT, ambientC);
}
