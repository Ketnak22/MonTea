#pragma once

#include <stdint.h>

enum class ButtonEvent : uint8_t {
  None,
  Down,      // A button went down while none were held
  StepLeft,  // Tap, or one repeat of a hold
  StepRight,
  Chord      // Both buttons held together for kChordMs
};

// Turns the debounced button levels into events. A gesture lasts from the
// first button going down until both are released.
class ButtonGestures {
public:
  static constexpr uint32_t kHoldMs = 500;
  static constexpr uint32_t kRepeatMs = 150;
  static constexpr uint32_t kChordMs = 1000;

  // Call on every pass, returns at most one event
  ButtonEvent update(uint32_t nowMs, bool leftDown, bool rightDown);

  // Ignore the rest of the current gesture
  void consume() { _consumed = true; }

  bool active() const { return _active; }

private:
  ButtonEvent stepEvent() const;

  bool _active = false;
  bool _consumed = false;
  bool _left = false;
  bool _both = false;
  bool _chordFired = false;
  bool _repeated = false;
  uint32_t _startMs = 0;
  uint32_t _bothSinceMs = 0;
  uint32_t _lastRepeatMs = 0;
};
