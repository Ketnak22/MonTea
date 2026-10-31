#pragma once

#include <stdint.h>

// What the buttons edit, and for how long. Knows nothing about storage: the
// caller loads the values and saves them when update() says the editor closed.
class SettingsEditor {
public:
  static constexpr int kTargetMin = 20;
  static constexpr int kTargetMax = 90;
  static constexpr int kDefaultTarget = 40;
  static constexpr uint32_t kTimeoutMs = 5000;

  // Falls back to the default for values nothing could have saved
  void load(int targetC);

  void open(uint32_t nowMs);
  bool isOpen() const { return _open; }

  // Restarts the timeout
  void touch(uint32_t nowMs) { _lastTouchMs = nowMs; }
  uint32_t sinceTouch(uint32_t nowMs) const { return nowMs - _lastTouchMs; }

  void step(int delta);

  // Returns true on the one call where the editor timed out and closed
  bool update(uint32_t nowMs);

  int target() const { return _targetC; }

private:
  bool _open = false;
  uint32_t _lastTouchMs = 0;
  int _targetC = kDefaultTarget;
};
