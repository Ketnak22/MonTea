#include "ButtonGestures.h"

ButtonEvent ButtonGestures::stepEvent() const {
  return _left ? ButtonEvent::StepLeft : ButtonEvent::StepRight;
}

ButtonEvent ButtonGestures::update(uint32_t nowMs, bool leftDown, bool rightDown) {
  const bool anyDown = leftDown || rightDown;
  const bool bothDown = leftDown && rightDown;

  if (!_active) {
    if (!anyDown) {
      return ButtonEvent::None;
    }
    _active = true;
    _consumed = false;
    _left = leftDown;
    _both = bothDown;
    _chordFired = false;
    _repeated = false;
    _startMs = nowMs;
    _bothSinceMs = nowMs;
    _lastRepeatMs = nowMs;
    return ButtonEvent::Down;
  }

  if (!anyDown) {
    _active = false;
    // The step comes on release, only then we know it wasn't a hold
    if (!_consumed && !_both && !_repeated) {
      return stepEvent();
    }
    return ButtonEvent::None;
  }

  if (bothDown && !_both) {
    _both = true;
    _bothSinceMs = nowMs;
  }

  if (_consumed) {
    return ButtonEvent::None;
  }

  if (_both) {
    if (bothDown && !_chordFired && nowMs - _bothSinceMs >= kChordMs) {
      _chordFired = true;
      return ButtonEvent::Chord;
    }
    return ButtonEvent::None;
  }

  if (nowMs - _startMs >= kHoldMs && nowMs - _lastRepeatMs >= kRepeatMs) {
    _repeated = true;
    _lastRepeatMs = nowMs;
    return stepEvent();
  }
  return ButtonEvent::None;
}
