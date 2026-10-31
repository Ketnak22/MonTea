#include <Arduino.h>
#include <Wire.h>
#include <EEPROM.h>

#include <ezButton.h>

#include <Adafruit_NeoPixel.h>
#include <Adafruit_SSD1306.h>

#include <ButtonGestures.h>
#include <CoolingEstimator.h>
#include <SettingsEditor.h>
#include <TeaMonitor.h>

#include "Mlx90614.h"
#include "pixelart.h"

// #define DEBUG

#define EEPROM_ADDR 0
#define SETTINGS_MAGIC 0x4D544541

// The target stays first, older firmware stored just that at this address
struct StoredSettings {
  int target;
  int offset;
  uint32_t magic;
};

#define MLX_ADDRESS 0x5A

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
#define SCREEN_ADDRESS 0x3C
#define TEA_CUP_SIZE 16
#define SCREEN_SLEEP_TIMEOUT 300000UL // 5 minutes of nothing happening

#define ERROR_LED_PIN 5

#define BUZZER_PIN 14

#define STATUS_LED_LEVEL 24 // out of 255, full brightness is painful up close

#define WATCHDOG_TIMEOUT 3000 // well above a worst case pass full of I2C timeouts

#define SENSOR_READ_INTERVAL 250 // the MLX only refreshes a few times per second

#define BLINK_PERIOD 1000

#define LEFT_BUTTON_PIN 27
#define RIGHT_BUTTON_PIN 26

// Set to 1 after moving the MLX90614 to GP2 (SDA) and GP3 (SCL). The display
// then stays alone on GP0/GP1 and can use its full 400 kHz.
#define MLX_ON_OWN_BUS 0

#define MLX_I2C_CLOCK_HZ 100000UL // MLX90614 works only in 10-100KHz range
#define I2C_TIMEOUT_MS 25

#if MLX_ON_OWN_BUS
#define MLX_BUS Wire1
#define OLED_I2C_CLOCK_HZ 400000UL
#else
// The MLX90614 sees all display traffic on a shared bus
#define MLX_BUS Wire
#define OLED_I2C_CLOCK_HZ MLX_I2C_CLOCK_HZ
#endif

// Initialize MLX90614 sensor and OLED display
Mlx90614 mlx(MLX_BUS, MLX_ADDRESS);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1, OLED_I2C_CLOCK_HZ, OLED_I2C_CLOCK_HZ);

ezButton leftBtn(LEFT_BUTTON_PIN, INPUT_PULLUP);
ezButton rightBtn(RIGHT_BUTTON_PIN, INPUT_PULLUP);

// The LED on the Zero expects RGB instead of the usual GRB. If red and green
// come out swapped on your board, change this to NEO_GRB.
Adafruit_NeoPixel statusLed(1, PIN_NEOPIXEL, NEO_RGB + NEO_KHZ800);
uint32_t shownLedColor = 0;

ButtonGestures gestures;
SettingsEditor editor;
TeaMonitor monitor;
CoolingEstimator cooling;

float objectC = 0;
float ambientC = 0;
unsigned long lastSensorReadMillis = 0;

unsigned long lastActivityMillis = 0;
bool screenAsleep = false;

// What is on the screen right now
String drawnStatus;
String drawnText;
bool drawnTextVisible = false;
bool drawnEditing = false;
bool drawnSensorFault = false;

const String formatTemperature(const int temperature) {
  return String(temperature) + (char)247 + 'C';
}

void drawArrows(const int y) {
  display.drawChar(10, y, '<', WHITE, WHITE, 2);
  display.drawChar(128 - 10 - 12, y, '>', WHITE, WHITE, 2);
}

const String statusLine() {
  switch (monitor.state()) {
  case TeaState::Watching: {
    String line = String(F("Watching ")) + editor.target();
    const int minutes = cooling.minutesUntil(editor.target(), ambientC);
    if (minutes >= 0) {
      line += String(F("  ~")) + minutes + F(" min");
    }
    return line;
  }
  case TeaState::CupAway:
    return F("Cup lifted");
  case TeaState::Alarm:
    return F("Tea is ready!");
  case TeaState::Idle:
    break;
  }
  return F("Waiting for tea");
}

uint32_t statusLedColor() {
  if (monitor.sensorFault()) {
    return Adafruit_NeoPixel::Color(STATUS_LED_LEVEL, 0, 0); // red
  }
  if (editor.isOpen()) {
    return Adafruit_NeoPixel::Color(0, 0, STATUS_LED_LEVEL); // blue
  }
  switch (monitor.state()) {
  case TeaState::Watching:
  case TeaState::CupAway:
    return Adafruit_NeoPixel::Color(STATUS_LED_LEVEL, STATUS_LED_LEVEL / 2, 0); // amber
  case TeaState::Alarm:
    return Adafruit_NeoPixel::Color(0, STATUS_LED_LEVEL, 0); // green
  case TeaState::Idle:
    break;
  }
  return 0;
}

void updateStatusLed() {
  const uint32_t color = statusLedColor();
  if (color != shownLedColor) {
    shownLedColor = color;
    statusLed.setPixelColor(0, color);
    statusLed.show();
  }
}

void updateScreenPower(const unsigned long now) {
  const bool idle = monitor.state() == TeaState::Idle && !editor.isOpen() && !monitor.sensorFault();
  if (!idle || gestures.active()) {
    lastActivityMillis = now;
  }

  const bool sleep = now - lastActivityMillis >= SCREEN_SLEEP_TIMEOUT;
  if (sleep != screenAsleep) {
    screenAsleep = sleep;
    display.ssd1306_command(sleep ? SSD1306_DISPLAYOFF : SSD1306_DISPLAYON);
  }
}

void showSensorError() {
  display.clearDisplay();
  display.setCursor(0, 4);
  display.setTextSize(1);
  display.println(F("Sensor error"));
  display.print(F("Check wiring"));
  display.display();
}

void setup() {
  // First thing, the buzzer is active low and the pin floats until now
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, HIGH);

#ifdef DEBUG
  Serial.begin(9600);
  while (!Serial);
#endif

  EEPROM.begin(sizeof(StoredSettings));
  StoredSettings stored;
  EEPROM.get(EEPROM_ADDR, stored);
  editor.load(stored.target, stored.magic == SETTINGS_MAGIC ? stored.offset : 0);
#ifdef DEBUG
  Serial.println(F("Loaded settings from EEPROM!"));
  Serial.print(F("target = "));
  Serial.println(editor.target());
  Serial.print(F("offset = "));
  Serial.println(editor.offset());
  Serial.println();
#endif

  // Set pins: 0 for SDA and 1 for SCL
  Wire.setSDA(0);
  Wire.setSCL(1);

  Wire.setClock(OLED_I2C_CLOCK_HZ);

  // Initialize I2C
  Wire.begin();
  Wire.setTimeout(I2C_TIMEOUT_MS, true);

#if MLX_ON_OWN_BUS
  Wire1.setSDA(2);
  Wire1.setSCL(3);
  Wire1.setClock(MLX_I2C_CLOCK_HZ);
  Wire1.begin();
  Wire1.setTimeout(I2C_TIMEOUT_MS, true);
#endif

  pinMode(ERROR_LED_PIN, OUTPUT);

  digitalWrite(ERROR_LED_PIN, HIGH);
  delay(500);
  digitalWrite(ERROR_LED_PIN, LOW);

  statusLed.begin();
  statusLed.show();

  leftBtn.setDebounceTime(50);
  rightBtn.setDebounceTime(50);

  // Display goes first, so a missing sensor can be reported on it
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    #ifdef DEBUG
      Serial.println(F("SSD1306 allocation failed"));
    #endif
    digitalWrite(ERROR_LED_PIN, HIGH);
    while (1);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  monitor.setTarget(editor.target());

  if (mlx.read(objectC, ambientC)) {
    monitor.onReading(millis(), objectC + editor.offset());
  } else {
    #ifdef DEBUG
      Serial.println(F("Error connecting to MLX sensor. Check wiring."));
    #endif
    // loop() shows the fault and keeps retrying, a wire can be reseated live
    for (uint8_t i = 0; i < TeaMonitor::kMaxFailures; i++) {
      monitor.onReadFailed();
    }
  }

  rp2040.wdt_begin(WATCHDOG_TIMEOUT);
}

void saveSettings() {
  const StoredSettings stored = {editor.target(), editor.offset(), SETTINGS_MAGIC};

  // Only touches the flash if a byte actually changed
  EEPROM.put(EEPROM_ADDR, stored);
  if (EEPROM.commit()) {
    #ifdef DEBUG
    Serial.println(F("Saved settings to EEPROM!"));
    Serial.print(F("target = "));
    Serial.println(stored.target);
    Serial.print(F("offset = "));
    Serial.println(stored.offset);
    Serial.println();
    #endif
  }
}

void applyStep(const int delta, const unsigned long now) {
  if (editor.isOpen()) {
    editor.step(delta);
  } else {
    // The first press only opens the editor, you want to see the value first
    editor.open(SettingsPage::Target, now);
    gestures.consume();
  }
}

void handleButtons(const unsigned long now) {
  leftBtn.loop();
  rightBtn.loop();

  const bool leftDown = leftBtn.getState() == LOW;
  const bool rightDown = rightBtn.getState() == LOW;

  switch (gestures.update(now, leftDown, rightDown)) {
  case ButtonEvent::Down:
    // A press during the alarm only stops the alarm, and one on a dark
    // screen only wakes it up
    if ((!editor.isOpen() && monitor.silence()) || screenAsleep) {
      gestures.consume();
    }
    break;
  case ButtonEvent::StepLeft:
    applyStep(-1, now);
    break;
  case ButtonEvent::StepRight:
    applyStep(1, now);
    break;
  case ButtonEvent::Chord:
    editor.open(editor.page() == SettingsPage::Offset ? SettingsPage::Target : SettingsPage::Offset, now);
    break;
  case ButtonEvent::None:
    break;
  }

  if (gestures.active()) {
    editor.touch(now);
  }
}

void loop() {
  rp2040.wdt_reset();

  handleButtons(millis());

  if (editor.update(millis())) {
    saveSettings();
  }
  monitor.setTarget(editor.target());

  if (millis() - lastSensorReadMillis >= SENSOR_READ_INTERVAL) {
    lastSensorReadMillis = millis();
    if (mlx.read(objectC, ambientC)) {
      monitor.onReading(millis(), objectC + editor.offset());
    } else {
      monitor.onReadFailed();
      #ifdef DEBUG
      Serial.println("Error reading from MLX sensor!");
      #endif
    }
  }
  monitor.update(millis());

  // Readings from before a lifted cup say nothing about where the tea is now
  if (monitor.state() == TeaState::Watching) {
    cooling.addReading(millis(), monitor.temperature());
  } else {
    cooling.reset();
  }

  updateStatusLed();
  updateScreenPower(millis());

  // Short dropouts just keep the last good value, only a long one is a fault
  const bool sensorFault = monitor.sensorFault();
  digitalWrite(ERROR_LED_PIN, sensorFault ? HIGH : LOW);
  if (sensorFault) {
    if (!drawnSensorFault) {
      drawnSensorFault = true;
      showSensorError();
    }
    digitalWrite(BUZZER_PIN, HIGH); // OFF
    return;
  }

  const bool editing = editor.isOpen();

  String status = statusLine();
  String text = formatTemperature(static_cast<int>(monitor.temperature()));
  if (editor.page() == SettingsPage::Target) {
    status = F("Set target");
    text = formatTemperature(editor.target());
  } else if (editor.page() == SettingsPage::Offset) {
    // The live value is there to compare against a real thermometer
    status = String(F("Set offset, now ")) + static_cast<int>(monitor.temperature());
    text = (editor.offset() > 0 ? "+" : "") + formatTemperature(editor.offset());
  }

  // Solid while the buttons are in use, blinking while the editor waits
  const bool textVisible = !editing || editor.sinceTouch(millis()) % BLINK_PERIOD < BLINK_PERIOD / 2;

  const bool screenChanged = drawnSensorFault || status != drawnStatus || text != drawnText || textVisible != drawnTextVisible || editing != drawnEditing;
  if (screenChanged && !screenAsleep) {
    drawnSensorFault = false;
    drawnStatus = status;
    drawnText = text;
    drawnTextVisible = textVisible;
    drawnEditing = editing;

    constexpr int y = SCREEN_HEIGHT - 16;
    const int x = (SCREEN_WIDTH / 2) - (text.length() * 12 / 2);

    display.clearDisplay();

    display.setCursor(0, 0);
    display.print(status);

    if (editing) {
      drawArrows(y);
    } else if (monitor.state() != TeaState::Idle) {
      display.drawBitmap(0, SCREEN_HEIGHT - TEA_CUP_SIZE, PixelArt::teaCup, TEA_CUP_SIZE, TEA_CUP_SIZE, SSD1306_WHITE);
    }

    if (textVisible) {
      display.setCursor(x, y);
      display.setTextSize(2);
      display.print(text);
      display.setTextSize(1);
    }

    display.display();
  }

  // Quiet while editing, it would drown out the point of the screen
  const bool buzzerOn = monitor.buzzerOn(millis()) && !editing;
  digitalWrite(BUZZER_PIN, buzzerOn ? LOW : HIGH);
}
