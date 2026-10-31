#include "CoolingEstimator.h"

#include <math.h>

void CoolingEstimator::addReading(uint32_t nowMs, float temperatureC) {
  if (_count > 0 && nowMs - _lastMs < kSampleStepMs) {
    return;
  }

  _lastMs = nowMs;
  _times[_next] = nowMs;
  _temperatures[_next] = temperatureC;
  _next = (_next + 1) % kSamples;
  if (_count < kSamples) {
    _count++;
  }
}

int CoolingEstimator::minutesUntil(float targetC, float ambientC) const {
  if (_count < kMinSamples) {
    return -1;
  }

  const uint8_t newest = (_next + kSamples - 1) % kSamples;
  const uint8_t oldest = (_next + kSamples - _count) % kSamples;
  const float newestC = _temperatures[newest];
  const float oldestC = _temperatures[oldest];

  // Tea can't cool below the room, and there is nothing to work out if it's
  // already at the target
  if (targetC <= ambientC + 1 || newestC <= targetC || oldestC - newestC < kMinDropC) {
    return -1;
  }

  const float elapsedMinutes = (_times[newest] - _times[oldest]) / 60000.0f;
  const float rate = logf((oldestC - ambientC) / (newestC - ambientC)) / elapsedMinutes;
  const float minutes = logf((newestC - ambientC) / (targetC - ambientC)) / rate;

  if (!(minutes > 0) || minutes > kMaxMinutes) {
    return -1;
  }
  return static_cast<int>(ceilf(minutes));
}
