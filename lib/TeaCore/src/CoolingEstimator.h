#pragma once

#include <stdint.h>

// Guesses when the tea reaches the target from how fast it has been cooling.
// A cooling cup follows Newton's law: the gap to the room temperature shrinks
// exponentially, so two readings are enough to get the rate.
class CoolingEstimator {
public:
  static constexpr uint32_t kSampleStepMs = 30000;
  static constexpr uint8_t kSamples = 7;     // 3 minutes between oldest and newest
  static constexpr uint8_t kMinSamples = 3;  // No guessing during the first minute
  static constexpr float kMinDropC = 0.5f;   // Below that it's just sensor noise
  static constexpr int kMaxMinutes = 120;

  void reset() { _count = 0; }
  void addReading(uint32_t nowMs, float temperatureC);

  // Minutes until the target, rounded up, or -1 when there is no sane answer
  int minutesUntil(float targetC, float ambientC) const;

private:
  uint8_t _count = 0;
  uint8_t _next = 0;
  uint32_t _lastMs = 0;
  uint32_t _times[kSamples] = {};
  float _temperatures[kSamples] = {};
};
