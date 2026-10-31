#include "SettingsEditor.h"

void SettingsEditor::load(int targetC) {
  const bool valid = targetC >= kTargetMin && targetC <= kTargetMax;
  _targetC = valid ? targetC : kDefaultTarget;
}

void SettingsEditor::open(uint32_t nowMs) {
  _open = true;
  _lastTouchMs = nowMs;
}

void SettingsEditor::step(int delta) {
  int value = _targetC + delta;
  if (value < kTargetMin) {
    value = kTargetMin;
  } else if (value > kTargetMax) {
    value = kTargetMax;
  }
  _targetC = value;
}

bool SettingsEditor::update(uint32_t nowMs) {
  if (!_open || sinceTouch(nowMs) < kTimeoutMs) {
    return false;
  }
  _open = false;
  return true;
}
