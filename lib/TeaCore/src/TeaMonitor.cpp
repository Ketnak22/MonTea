#include "TeaMonitor.h"

void TeaMonitor::enter(TeaState state, uint32_t nowMs) {
  _state = state;
  _stateSinceMs = nowMs;
  _conditionHeld = false;
  _riseSeen = false;
  if (state == TeaState::Watching) {
    _peakC = _temperatureC;
  } else if (state == TeaState::Alarm) {
    _alarmed = true;
    _lastAlarmMs = nowMs;
  }
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
  float lowC = temperatureC;
  for (uint8_t i = 0; i < _historyCount; i++) {
    if (_history[i] > peakC) {
      peakC = _history[i];
    }
    if (_history[i] < lowC) {
      lowC = _history[i];
    }
  }
  _windowLowC = lowC;

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
  case TeaState::Idle: {
    // A cup shows up as a sudden rise that then stays up. A sip right after
    // the alarm looks the same, so a cool cup doesn't count for a while.
    const bool justServed = _alarmed && nowMs - _lastAlarmMs < kServedMs;
    if (!_riseSeen && temperatureC - _windowLowC >= kRiseC &&
        (temperatureC > _targetC || !justServed)) {
      _riseSeen = true;
      _riseBaseC = _windowLowC;
    }
    if (_riseSeen && temperatureC < _riseBaseC + kRiseC) {
      _riseSeen = false;
    }
    if (heldFor(_riseSeen, nowMs)) {
      enter(TeaState::Watching, nowMs);
    }
    break;
  }

  case TeaState::Watching:
    if (temperatureC > _peakC) {
      _peakC = temperatureC;
    }
    if (dropped) {
      // Tea loses a degree or two per minute, this was the cup leaving
      enter(TeaState::CupAway, nowMs);
      _lowestAwayC = temperatureC;
    } else if (heldFor(temperatureC <= _targetC && temperatureC <= _peakC - kCoolingC, nowMs)) {
      // The glass warms up slowly, so wait for the reading to turn around
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
