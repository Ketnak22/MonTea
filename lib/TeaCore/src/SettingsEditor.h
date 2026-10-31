#pragma once

#include <stdint.h>

enum class SettingsPage : uint8_t { Closed, Target, Offset };

// What the buttons edit, and for how long. Knows nothing about storage: the
// caller loads the values and saves them when update() says the editor closed.
class SettingsEditor {
public:
  static constexpr int kTargetMin = 20;
  static constexpr int kTargetMax = 90;
  static constexpr int kDefaultTarget = 55;
  static constexpr int kOffsetMin = -20;
  static constexpr int kOffsetMax = 20;
  static constexpr uint32_t kTimeoutMs = 5000;

  // Falls back to the defaults for values nothing could have saved
  void load(int targetC, int offsetC);

  void open(SettingsPage page, uint32_t nowMs);
  SettingsPage page() const { return _page; }
  bool isOpen() const { return _page != SettingsPage::Closed; }

  // Restarts the timeout
  void touch(uint32_t nowMs) { _lastTouchMs = nowMs; }
  uint32_t sinceTouch(uint32_t nowMs) const { return nowMs - _lastTouchMs; }

  void step(int delta);

  // Returns true on the one call where the editor timed out and closed
  bool update(uint32_t nowMs);

  int target() const { return _targetC; }

  // Added to what the sensor reads, see the calibration part of the README
  int offset() const { return _offsetC; }

private:
  SettingsPage _page = SettingsPage::Closed;
  uint32_t _lastTouchMs = 0;
  int _targetC = kDefaultTarget;
  int _offsetC = 0;
};
