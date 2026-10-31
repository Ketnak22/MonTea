#include "ButtonGestures.h"

ButtonEvent ButtonGestures::stepEvent() const {
  return _left ? ButtonEvent::StepLeft : ButtonEvent::StepRight;
}

ButtonEvent ButtonGestures::update(uint32_t nowMs, bool leftDown, bool rightDown) {
  const bool anyDown = leftDown || rightDown;

  if (!_active) {
    if (!anyDown) {
      return ButtonEvent::None;
    }
    _active = true;
    _consumed = false;
    _left = leftDown;
    _both = leftDown && rightDown;
    _repeated = false;
    _startMs = nowMs;
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

  if (leftDown && rightDown) {
    _both = true;
  }
  if (_consumed || _both) {
    return ButtonEvent::None;
  }

  if (nowMs - _startMs >= kHoldMs && nowMs - _lastRepeatMs >= kRepeatMs) {
    _repeated = true;
    _lastRepeatMs = nowMs;
    return stepEvent();
  }
  return ButtonEvent::None;
}
