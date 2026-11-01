#include <math.h>
#include <unity.h>

#include <ButtonGestures.h>
#include <CoolingEstimator.h>
#include <SettingsEditor.h>
#include <SmbusPec.h>
#include <TeaMonitor.h>

static const int kTarget = 55;
static const uint32_t kReadMs = 250;

static TeaMonitor monitor;
static ButtonGestures gestures;
static uint32_t nowMs;

void setUp() {
  monitor = TeaMonitor();
  monitor.setTarget(kTarget);
  gestures = ButtonGestures();
  nowMs = 1000;
}

void tearDown() {}

// Feeds the same temperature for a while, the way the main loop would
static void hold(float temperatureC, uint32_t durationMs) {
  for (uint32_t elapsed = 0; elapsed < durationMs; elapsed += kReadMs) {
    monitor.onReading(nowMs, temperatureC);
    monitor.update(nowMs);
    nowMs += kReadMs;
  }
}

// A cup of tea arriving: the reading climbs a few degrees in a few seconds
static void pour(float temperatureC) {
  hold(22, 2000);
  hold((22 + temperatureC) / 2, 1000);
  hold(temperatureC, 4000);
}

static void startWatching() {
  pour(kTarget + 6);
  TEST_ASSERT_TRUE(monitor.state() == TeaState::Watching);
}

// TeaMonitor

static void test_room_temperature_stays_quiet_from_power_on() {
  hold(22, 60000);
  TEST_ASSERT_TRUE(monitor.state() == TeaState::Idle);
  TEST_ASSERT_FALSE(monitor.buzzerOn(nowMs));
}

static void test_short_hot_spike_does_not_start_watching() {
  hold(70, 1000);
  hold(22, 1000);
  hold(70, 1000);
  TEST_ASSERT_TRUE(monitor.state() == TeaState::Idle);
}

static void test_still_hot_cup_without_a_rise_is_ignored() {
  hold(70, 60000);
  TEST_ASSERT_TRUE(monitor.state() == TeaState::Idle);
}

static void test_slow_warming_does_not_start_watching() {
  for (int i = 0; i < 40; i++) {
    hold(40 + i * 0.5f, 3000); // 0.5 degrees per 3 seconds, crosses the target
  }
  TEST_ASSERT_TRUE(monitor.state() == TeaState::Idle);
}

static void test_sudden_rise_starts_watching_even_just_above_target() {
  pour(kTarget + 2);
  TEST_ASSERT_TRUE(monitor.state() == TeaState::Watching);
}

static void test_cup_below_target_alarms_only_once_it_cools() {
  pour(kTarget - 2);
  TEST_ASSERT_TRUE(monitor.state() == TeaState::Watching);
  hold(kTarget - 2, 10000);
  TEST_ASSERT_TRUE(monitor.state() == TeaState::Watching); // still warming the glass
  hold(kTarget - 3, 3500);
  TEST_ASSERT_TRUE(monitor.state() == TeaState::Alarm);
}

static void test_cool_cup_put_back_after_the_alarm_stays_quiet() {
  startWatching();
  hold(kTarget + 3, 6000);
  hold(kTarget, 3500);
  TEST_ASSERT_TRUE(monitor.state() == TeaState::Alarm);
  monitor.silence();
  hold(22, 6000); // a sip
  hold(kTarget - 1, 6000);
  TEST_ASSERT_TRUE(monitor.state() == TeaState::Idle);
}

static void test_hot_cup_after_the_alarm_is_watched() {
  startWatching();
  hold(kTarget + 3, 6000);
  hold(kTarget, 3500);
  monitor.silence();
  hold(22, 6000);
  hold(kTarget + 6, 4000);
  TEST_ASSERT_TRUE(monitor.state() == TeaState::Watching);
}

static void test_rise_must_last_a_few_seconds() {
  hold(22, 2000);
  hold(kTarget + 6, 2000);
  hold(22, 5000);
  TEST_ASSERT_TRUE(monitor.state() == TeaState::Idle);
}

static void test_rise_starts_watching_after_the_dwell() {
  hold(22, 2000);
  hold(kTarget + 6, 2500);
  TEST_ASSERT_TRUE(monitor.state() == TeaState::Idle);
  hold(kTarget + 6, 1000);
  TEST_ASSERT_TRUE(monitor.state() == TeaState::Watching);
}

static void test_slow_cooling_ends_in_the_alarm_not_in_cup_away() {
  startWatching();
  // 3 degrees per minute, faster than a real cup
  for (float temperatureC = kTarget + 6; temperatureC > kTarget - 1; temperatureC -= 0.0125f) {
    hold(temperatureC, kReadMs);
    TEST_ASSERT_TRUE(monitor.state() != TeaState::CupAway);
  }
  TEST_ASSERT_TRUE(monitor.state() == TeaState::Alarm);
}

static void test_noise_around_the_target_does_not_alarm() {
  startWatching();
  hold(kTarget + 2, 6000);
  for (int i = 0; i < 40; i++) {
    hold(kTarget - 0.5f, 1000);
    hold(kTarget + 0.5f, 1000);
  }
  TEST_ASSERT_TRUE(monitor.state() == TeaState::Watching);
}

static void test_alarm_beeps_half_of_each_second() {
  startWatching();
  hold(kTarget + 3, 6000);
  hold(kTarget, 3500);
  TEST_ASSERT_TRUE(monitor.state() == TeaState::Alarm);

  uint32_t beepingMs = 0;
  for (uint32_t i = 0; i < 1000; i += 10) {
    beepingMs += monitor.buzzerOn(nowMs + i) ? 10 : 0;
  }
  TEST_ASSERT_EQUAL_UINT32(500, beepingMs);
}

static void reachAlarm() {
  startWatching();
  hold(kTarget + 3, 6000);
  hold(kTarget, 3500);
  TEST_ASSERT_TRUE(monitor.state() == TeaState::Alarm);
}

static void test_silence_stops_the_alarm_and_it_stays_stopped() {
  reachAlarm();
  TEST_ASSERT_TRUE(monitor.silence());
  TEST_ASSERT_TRUE(monitor.state() == TeaState::Idle);
  hold(kTarget, 30000);
  TEST_ASSERT_TRUE(monitor.state() == TeaState::Idle);
  TEST_ASSERT_FALSE(monitor.buzzerOn(nowMs));
}

static void test_silence_without_an_alarm_does_nothing() {
  startWatching();
  TEST_ASSERT_FALSE(monitor.silence());
  TEST_ASSERT_TRUE(monitor.state() == TeaState::Watching);
}

static void test_alarm_gives_up_after_a_minute() {
  reachAlarm();
  hold(kTarget, TeaMonitor::kAlarmMaxMs + kReadMs);
  TEST_ASSERT_TRUE(monitor.state() == TeaState::Idle);
}

static void test_lifting_the_cup_during_the_alarm_stops_it() {
  reachAlarm();
  hold(25, 1000);
  TEST_ASSERT_TRUE(monitor.state() == TeaState::Idle);
}

static void test_lifting_the_cup_above_target_does_not_alarm() {
  startWatching();
  hold(58, 6000);
  hold(25, 20000);
  TEST_ASSERT_TRUE(monitor.state() == TeaState::CupAway);
  TEST_ASSERT_FALSE(monitor.buzzerOn(nowMs));
}

static void test_cup_back_while_hot_resumes_watching() {
  startWatching();
  hold(25, 20000);
  TEST_ASSERT_TRUE(monitor.state() == TeaState::CupAway);
  hold(62, 4000);
  TEST_ASSERT_TRUE(monitor.state() == TeaState::Watching);
}

static void test_cup_back_already_cool_is_an_alarm() {
  startWatching();
  hold(25, 20000);
  hold(50, 4000);
  TEST_ASSERT_TRUE(monitor.state() == TeaState::Alarm);
}

static void test_cup_never_coming_back_goes_idle() {
  startWatching();
  hold(25, TeaMonitor::kCupAwayMaxMs + kReadMs);
  TEST_ASSERT_TRUE(monitor.state() == TeaState::Idle);
}

static void test_few_failed_reads_are_not_a_fault() {
  for (int i = 0; i < TeaMonitor::kMaxFailures - 1; i++) {
    monitor.onReadFailed();
  }
  TEST_ASSERT_FALSE(monitor.sensorFault());
}

static void test_fault_mutes_the_alarm_and_clears_on_a_good_read() {
  reachAlarm();
  for (int i = 0; i < TeaMonitor::kMaxFailures; i++) {
    monitor.onReadFailed();
  }
  TEST_ASSERT_TRUE(monitor.sensorFault());
  for (uint32_t i = 0; i < 1000; i += 10) {
    TEST_ASSERT_FALSE(monitor.buzzerOn(nowMs + i));
  }
  hold(kTarget, kReadMs);
  TEST_ASSERT_FALSE(monitor.sensorFault());
}

static void test_clock_rollover_does_not_upset_the_timers() {
  nowMs = 0xFFFFF000UL;
  startWatching();
  hold(kTarget + 3, 6000);
  hold(kTarget, 3500);
  TEST_ASSERT_TRUE(monitor.state() == TeaState::Alarm);
}

// ButtonGestures

static ButtonEvent press(bool left, bool right) {
  nowMs += 10;
  return gestures.update(nowMs, left, right);
}

// Keeps both levels for a while and counts the events of one type
static int run(bool left, bool right, uint32_t durationMs, ButtonEvent counted) {
  int count = 0;
  for (uint32_t elapsed = 0; elapsed < durationMs; elapsed += 10) {
    if (press(left, right) == counted) {
      count++;
    }
  }
  return count;
}

static void test_tap_gives_down_and_one_step_on_release() {
  TEST_ASSERT_TRUE(press(true, false) == ButtonEvent::Down);
  TEST_ASSERT_EQUAL(0, run(true, false, 100, ButtonEvent::StepLeft));
  TEST_ASSERT_TRUE(press(false, false) == ButtonEvent::StepLeft);
  TEST_ASSERT_FALSE(gestures.active());
}

static void test_hold_repeats_and_adds_nothing_on_release() {
  press(false, true);
  const int repeats = run(false, true, 1000, ButtonEvent::StepRight);
  TEST_ASSERT_EQUAL(4, repeats); // at 500, 650, 800 and 950 ms
  TEST_ASSERT_TRUE(press(false, false) == ButtonEvent::None);
}

static void test_uneven_press_of_both_buttons_never_steps() {
  // The first button leads by a moment, which is what broke the old combo
  press(true, false);
  run(true, false, 30, ButtonEvent::None);
  TEST_ASSERT_EQUAL(0, run(true, true, 300, ButtonEvent::StepLeft));
  TEST_ASSERT_TRUE(press(true, false) == ButtonEvent::None);
  TEST_ASSERT_TRUE(press(false, false) == ButtonEvent::None);
}

static void test_quick_left_then_right_are_two_separate_steps() {
  press(true, false);
  TEST_ASSERT_TRUE(press(false, false) == ButtonEvent::StepLeft);
  press(false, true);
  TEST_ASSERT_TRUE(press(false, false) == ButtonEvent::StepRight);
}

static void test_chord_fires_once_after_a_second() {
  press(true, true);
  TEST_ASSERT_EQUAL(0, run(true, true, 900, ButtonEvent::Chord));
  TEST_ASSERT_EQUAL(1, run(true, true, 2000, ButtonEvent::Chord));
}

static void test_consumed_gesture_stays_silent() {
  press(true, false);
  gestures.consume();
  TEST_ASSERT_EQUAL(0, run(true, false, 1000, ButtonEvent::StepLeft));
  TEST_ASSERT_TRUE(press(false, false) == ButtonEvent::None);
  // The next gesture counts again
  press(true, false);
  TEST_ASSERT_TRUE(press(false, false) == ButtonEvent::StepLeft);
}

// SettingsEditor

static void test_editor_rejects_values_nothing_could_have_saved() {
  SettingsEditor editor;
  editor.load(-1, 500);
  TEST_ASSERT_EQUAL(SettingsEditor::kDefaultTarget, editor.target());
  TEST_ASSERT_EQUAL(0, editor.offset());
  editor.load(60, -3);
  TEST_ASSERT_EQUAL(60, editor.target());
  TEST_ASSERT_EQUAL(-3, editor.offset());
}

static void test_editor_clamps_both_pages() {
  SettingsEditor editor;
  editor.load(SettingsEditor::kTargetMax, SettingsEditor::kOffsetMin);
  editor.open(SettingsPage::Target, 0);
  editor.step(1);
  TEST_ASSERT_EQUAL(SettingsEditor::kTargetMax, editor.target());
  editor.open(SettingsPage::Offset, 0);
  editor.step(-1);
  TEST_ASSERT_EQUAL(SettingsEditor::kOffsetMin, editor.offset());
}

static void test_editor_timeout_counts_from_the_last_touch() {
  SettingsEditor editor;
  editor.open(SettingsPage::Target, 1000);
  editor.touch(5000);
  TEST_ASSERT_FALSE(editor.update(9999));
  TEST_ASSERT_TRUE(editor.isOpen());
  TEST_ASSERT_TRUE(editor.update(10000));
  TEST_ASSERT_FALSE(editor.isOpen());
  TEST_ASSERT_FALSE(editor.update(20000)); // saves only once
}

// CoolingEstimator

static float cupAt(float minutes, float ambientC) {
  return ambientC + (80 - ambientC) * expf(-0.03f * minutes);
}

static void test_estimate_waits_for_enough_data() {
  CoolingEstimator estimator;
  estimator.addReading(0, 80);
  estimator.addReading(30000, 79);
  TEST_ASSERT_EQUAL(-1, estimator.minutesUntil(55, 22));
}

static void test_estimate_matches_a_real_cooling_curve() {
  CoolingEstimator estimator;
  const float ambientC = 22;
  for (uint32_t second = 0; second <= 600; second++) {
    estimator.addReading(second * 1000, cupAt(second / 60.0f, ambientC));
  }
  // 80 C reaches 55 C after ln(58 / 33) / 0.03 = 18.8 minutes
  const int minutes = estimator.minutesUntil(55, ambientC);
  TEST_ASSERT_INT_WITHIN(1, 9, minutes);
}

static void test_estimate_refuses_nonsense() {
  CoolingEstimator estimator;
  for (uint32_t step = 0; step < 7; step++) {
    estimator.addReading(step * 30000, 60 - step * 0.5f);
  }
  TEST_ASSERT_EQUAL(-1, estimator.minutesUntil(20, 22)); // below the room
  TEST_ASSERT_EQUAL(-1, estimator.minutesUntil(58, 22)); // already there

  estimator.reset();
  for (uint32_t step = 0; step < 7; step++) {
    estimator.addReading(step * 30000, 60); // not cooling at all
  }
  TEST_ASSERT_EQUAL(-1, estimator.minutesUntil(55, 22));
}

// SmbusPec

static void test_pec_matches_the_standard_crc8_check_value() {
  const uint8_t text[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
  TEST_ASSERT_EQUAL_HEX8(0xF4, smbusPec(text, sizeof(text)));
}

static void test_pec_matches_a_real_mlx_read() {
  // Object temperature read from address 0x5A: 0x3AD2 with a PEC of 0x30
  const uint8_t frame[] = {0xB4, 0x07, 0xB5, 0xD2, 0x3A};
  TEST_ASSERT_EQUAL_HEX8(0x30, smbusPec(frame, sizeof(frame)));
}

static void test_pec_catches_a_flipped_bit() {
  const uint8_t frame[] = {0xB4, 0x07, 0xB5, 0xD2, 0x3B};
  const uint8_t good = smbusPec(frame, sizeof(frame));
  uint8_t damaged[sizeof(frame)];
  for (size_t i = 0; i < sizeof(frame); i++) {
    damaged[i] = frame[i];
  }
  damaged[4] ^= 0x10;
  TEST_ASSERT_NOT_EQUAL(good, smbusPec(damaged, sizeof(damaged)));
}

int main() {
  UNITY_BEGIN();

  RUN_TEST(test_room_temperature_stays_quiet_from_power_on);
  RUN_TEST(test_short_hot_spike_does_not_start_watching);
  RUN_TEST(test_still_hot_cup_without_a_rise_is_ignored);
  RUN_TEST(test_slow_warming_does_not_start_watching);
  RUN_TEST(test_sudden_rise_starts_watching_even_just_above_target);
  RUN_TEST(test_cup_below_target_alarms_only_once_it_cools);
  RUN_TEST(test_cool_cup_put_back_after_the_alarm_stays_quiet);
  RUN_TEST(test_hot_cup_after_the_alarm_is_watched);
  RUN_TEST(test_rise_must_last_a_few_seconds);
  RUN_TEST(test_rise_starts_watching_after_the_dwell);
  RUN_TEST(test_slow_cooling_ends_in_the_alarm_not_in_cup_away);
  RUN_TEST(test_noise_around_the_target_does_not_alarm);
  RUN_TEST(test_alarm_beeps_half_of_each_second);
  RUN_TEST(test_silence_stops_the_alarm_and_it_stays_stopped);
  RUN_TEST(test_silence_without_an_alarm_does_nothing);
  RUN_TEST(test_alarm_gives_up_after_a_minute);
  RUN_TEST(test_lifting_the_cup_during_the_alarm_stops_it);
  RUN_TEST(test_lifting_the_cup_above_target_does_not_alarm);
  RUN_TEST(test_cup_back_while_hot_resumes_watching);
  RUN_TEST(test_cup_back_already_cool_is_an_alarm);
  RUN_TEST(test_cup_never_coming_back_goes_idle);
  RUN_TEST(test_few_failed_reads_are_not_a_fault);
  RUN_TEST(test_fault_mutes_the_alarm_and_clears_on_a_good_read);
  RUN_TEST(test_clock_rollover_does_not_upset_the_timers);

  RUN_TEST(test_tap_gives_down_and_one_step_on_release);
  RUN_TEST(test_hold_repeats_and_adds_nothing_on_release);
  RUN_TEST(test_uneven_press_of_both_buttons_never_steps);
  RUN_TEST(test_quick_left_then_right_are_two_separate_steps);
  RUN_TEST(test_chord_fires_once_after_a_second);
  RUN_TEST(test_consumed_gesture_stays_silent);

  RUN_TEST(test_editor_rejects_values_nothing_could_have_saved);
  RUN_TEST(test_editor_clamps_both_pages);
  RUN_TEST(test_editor_timeout_counts_from_the_last_touch);

  RUN_TEST(test_estimate_waits_for_enough_data);
  RUN_TEST(test_estimate_matches_a_real_cooling_curve);
  RUN_TEST(test_estimate_refuses_nonsense);

  RUN_TEST(test_pec_matches_the_standard_crc8_check_value);
  RUN_TEST(test_pec_matches_a_real_mlx_read);
  RUN_TEST(test_pec_catches_a_flipped_bit);

  return UNITY_END();
}
