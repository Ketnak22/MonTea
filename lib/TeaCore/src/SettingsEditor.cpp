#include "SettingsEditor.h"

static int limited(int value, int minimum, int maximum) {
  if (value < minimum) {
    return minimum;
  }
  return value > maximum ? maximum : value;
}

void SettingsEditor::load(int targetC, int offsetC) {
  const bool validTarget = targetC >= kTargetMin && targetC <= kTargetMax;
  const bool validOffset = offsetC >= kOffsetMin && offsetC <= kOffsetMax;
  _targetC = validTarget ? targetC : kDefaultTarget;
  _offsetC = validOffset ? offsetC : 0;
}

void SettingsEditor::open(SettingsPage page, uint32_t nowMs) {
  _page = page;
  _lastTouchMs = nowMs;
}

void SettingsEditor::step(int delta) {
  if (_page == SettingsPage::Target) {
    _targetC = limited(_targetC + delta, kTargetMin, kTargetMax);
  } else if (_page == SettingsPage::Offset) {
    _offsetC = limited(_offsetC + delta, kOffsetMin, kOffsetMax);
  }
}

bool SettingsEditor::update(uint32_t nowMs) {
  if (!isOpen() || sinceTouch(nowMs) < kTimeoutMs) {
    return false;
  }
  _page = SettingsPage::Closed;
  return true;
}
