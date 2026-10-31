#include "TeaMonitor.h"

void TeaMonitor::enter(TeaState state, uint32_t nowMs) {
  _state = state;
  _stateSinceMs = nowMs;
  _conditionHeld = false;
}

bool TeaMonitor::heldFor(bool condition, uint32_t nowMs) {
  if (!condition) {
    _conditionHeld = false;
    return false;
  }
  if (!_conditionHeld) {
    _conditionHeld = true;
    _conditionSinceMs = nowMs;
  }
  return nowMs - _conditionSinceMs >= kDwellMs;
}

bool TeaMonitor::rememberAndCheckDrop(uint32_t nowMs, float temperatureC) {
  float peakC = temperatureC;
  for (uint8_t i = 0; i < _historyCount; i++) {
    if (_history[i] > peakC) {
      peakC = _history[i];
    }
  }

  if (_historyCount == 0 || nowMs - _historyLastMs >= kHistoryStepMs) {
    _historyLastMs = nowMs;
    _history[_historyNext] = temperatureC;
    _historyNext = (_historyNext + 1) % kHistorySize;
    if (_historyCount < kHistorySize) {
      _historyCount++;
    }
  }

  return peakC - temperatureC >= kCupDropC;
}

void TeaMonitor::onReading(uint32_t nowMs, float temperatureC) {
  _failures = 0;
  _temperatureC = temperatureC;
  const bool dropped = rememberAndCheckDrop(nowMs, temperatureC);

  switch (_state) {
  case TeaState::Idle:
    if (heldFor(temperatureC >= _targetC + kArmMarginC, nowMs)) {
      enter(TeaState::Watching, nowMs);
    }
    break;

  case TeaState::Watching:
    if (dropped) {
      // Tea loses a degree or two per minute, this was the cup leaving
      enter(TeaState::CupAway, nowMs);
      _lowestAwayC = temperatureC;
    } else if (heldFor(temperatureC <= _targetC, nowMs)) {
      enter(TeaState::Alarm, nowMs);
    }
    break;

  case TeaState::CupAway:
    if (temperatureC < _lowestAwayC) {
      _lowestAwayC = temperatureC;
    }
    if (heldFor(temperatureC >= _lowestAwayC + kCupDropC, nowMs)) {
      enter(temperatureC > _targetC ? TeaState::Watching : TeaState::Alarm, nowMs);
    }
    break;

  case TeaState::Alarm:
    if (dropped) {
      enter(TeaState::Idle, nowMs); // Picked up, no need to keep beeping
    }
    break;
  }
}

void TeaMonitor::onReadFailed() {
  if (_failures < kMaxFailures) {
    _failures++;
  }
}

void TeaMonitor::update(uint32_t nowMs) {
  if (_state == TeaState::Alarm && nowMs - _stateSinceMs >= kAlarmMaxMs) {
    enter(TeaState::Idle, nowMs);
  } else if (_state == TeaState::CupAway && nowMs - _stateSinceMs >= kCupAwayMaxMs) {
    enter(TeaState::Idle, nowMs);
  }
}

bool TeaMonitor::silence() {
  if (_state != TeaState::Alarm) {
    return false;
  }
  _state = TeaState::Idle;
  _conditionHeld = false;
  return true;
}

bool TeaMonitor::buzzerOn(uint32_t nowMs) const {
  if (_state != TeaState::Alarm || sensorFault()) {
    return false;
  }
  return (nowMs - _stateSinceMs) % kBeepPeriodMs < kBeepOnMs;
}
