#pragma once

#include <stdint.h>

enum class TeaState : uint8_t {
  Idle,     // Nothing to watch, stays quiet at any temperature
  Watching, // Hot tea seen, waiting for it to cool down to the target
  CupAway,  // Cup got lifted while watching, waiting for it to come back
  Alarm     // Tea is ready, beeping
};

// Decides when to beep. Knows nothing about the hardware, so it can be tested
// on a PC. All times are in milliseconds, all temperatures in Celsius.
class TeaMonitor {
public:
  static constexpr float kRiseC = 3.0f;            // sudden rise, in a few seconds, that means a hot cup arrived
  static constexpr uint32_t kDwellMs = 3000;        // a condition must hold this long
  static constexpr float kCupDropC = 5.0f;          // drop that can't be normal cooling
  static constexpr float kCoolingC = 0.5f;          // below the peak before it counts as cooling
  static constexpr uint32_t kCupAwayMaxMs = 600000; // give up on a lifted cup
  static constexpr uint32_t kServedMs = 600000;     // after an alarm, a cool cup is the same cup
  static constexpr uint32_t kAlarmMaxMs = 60000;    // stop beeping by itself
  static constexpr uint32_t kBeepPeriodMs = 1000;
  static constexpr uint32_t kBeepOnMs = 500;
  static constexpr uint8_t kMaxFailures = 8;        // failed reads in a row for a fault

  void setTarget(int targetC) { _targetC = targetC; }

  void onReading(uint32_t nowMs, float temperatureC);
  void onReadFailed();

  // Handles the timeouts, call it on every pass of the main loop
  void update(uint32_t nowMs);

  // Returns true if there was an alarm to stop
  bool silence();

  TeaState state() const { return _state; }
  bool sensorFault() const { return _failures >= kMaxFailures; }
  float temperature() const { return _temperatureC; }
  bool buzzerOn(uint32_t nowMs) const;

private:
  static constexpr uint8_t kHistorySize = 5;
  static constexpr uint32_t kHistoryStepMs = 1000;

  void enter(TeaState state, uint32_t nowMs);
  bool heldFor(bool condition, uint32_t nowMs);
  bool rememberAndCheckDrop(uint32_t nowMs, float temperatureC);

  TeaState _state = TeaState::Idle;
  uint32_t _stateSinceMs = 0;

  int _targetC = 0;
  float _temperatureC = 0;
  uint8_t _failures = 0;

  bool _conditionHeld = false;
  uint32_t _conditionSinceMs = 0;

  // The last few seconds of readings, to tell a lifted cup from cooling
  float _history[kHistorySize] = {};
  uint8_t _historyCount = 0;
  uint8_t _historyNext = 0;
  uint32_t _historyLastMs = 0;

  float _peakC = 0;
  bool _alarmed = false;
  uint32_t _lastAlarmMs = 0;

  float _windowLowC = 0;
  float _riseBaseC = 0;
  bool _riseSeen = false;

  float _lowestAwayC = 0;
};
