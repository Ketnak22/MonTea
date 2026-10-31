#pragma once

#include <Arduino.h>
#include <Wire.h>

class Mlx90614 {
public:
  Mlx90614(TwoWire &bus, uint8_t address) : _bus(bus), _address(address) {}

  // Returns false unless both temperatures arrived intact
  bool read(float &objectC, float &ambientC);

private:
  bool readTemperature(uint8_t reg, float &celsius);

  TwoWire &_bus;
  uint8_t _address;
};
