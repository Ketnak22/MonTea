#include <Arduino.h>
#include <Wire.h>
#include <EEPROM.h>

#include <ezButton.h>

#include <Adafruit_SSD1306.h>

#include <TeaMonitor.h>

#include "Mlx90614.h"

// #define DEBUG

#define EEPROM_SIZE 4
#define EEPROM_ADDR 0

#define MLX_ADDRESS 0x5A

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
#define SCREEN_ADDRESS 0x3C

#define ERROR_LED_PIN 5

#define BUZZER_PIN 14

#define WATCHDOG_TIMEOUT 3000 // well above a worst case pass full of I2C timeouts

#define SENSOR_READ_INTERVAL 250 // the MLX only refreshes a few times per second

#define THRESHOLD_MODE_TIMEOUT 5000
#define THRESHOLD_MIN 0
#define THRESHOLD_MAX 95

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

// Variables for blinking text
bool thresholdMode = false; // = blinking mode
bool buttonsPreviouslyPressed = false;
unsigned long previousMillis = 0;  // Stores the last time the text was toggled
constexpr unsigned long blinkInterval = 500;
bool textVisible = true;

unsigned long leftPressTime = 0;
unsigned long rightPressTime = 0;
constexpr unsigned long comboTimeout = 500;

unsigned long thresholdModeLastPressMillis = 0;

int temperatureThreshold = 40;

TeaMonitor monitor;

float objectC = 0;
float ambientC = 0;
unsigned long lastSensorReadMillis = 0;

// What is on the screen right now
String drawnStatus;
String drawnText;
bool drawnTextVisible = false;
bool drawnThresholdMode = false;
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
  case TeaState::Watching:
    return String(F("Watching, target ")) + temperatureThreshold;
  case TeaState::CupAway:
    return F("Cup lifted");
  case TeaState::Alarm:
    return F("Tea is ready!");
  case TeaState::Idle:
    break;
  }
  return F("Waiting for tea");
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

  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(EEPROM_ADDR, temperatureThreshold);
  if (temperatureThreshold < THRESHOLD_MIN || temperatureThreshold > THRESHOLD_MAX) {
    temperatureThreshold = 40;
  }
#ifdef DEBUG
  Serial.println(F("Loaded temperatureThreshold from EEPROM!"));
  Serial.print(F("temperatureThreshold = "));
  Serial.println(temperatureThreshold);
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

  monitor.setTarget(temperatureThreshold);

  if (mlx.read(objectC, ambientC)) {
    monitor.onReading(millis(), objectC);
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

void saveTemperaturetoEEPROM() {
  // Only touches the flash if a byte actually changed
  EEPROM.put(EEPROM_ADDR, temperatureThreshold);
  if (EEPROM.commit()) {
    #ifdef DEBUG
    Serial.println(F("Saved temperatureThreshold to EEPROM!"));
    Serial.print(F("temperatureThreshold = "));
    Serial.println(temperatureThreshold);
    Serial.println();
    #endif
  }
}

void loop() {
  rp2040.wdt_reset();

  leftBtn.loop();
  rightBtn.loop();

  if (leftBtn.isPressed()) {
    leftPressTime = millis();
  }
  if (rightBtn.isPressed()) {
    rightPressTime = millis();
  }

  // A press during the alarm only stops the alarm
  if ((leftBtn.isPressed() || rightBtn.isPressed()) && monitor.silence()) {
    leftPressTime = 0;
    rightPressTime = 0;
  }

  const bool comboDetected = abs((long)(leftPressTime - rightPressTime)) <= comboTimeout && leftPressTime != 0 && rightPressTime != 0;
  if (comboDetected) {
    thresholdMode = !thresholdMode;
    textVisible = true;
    previousMillis = millis();

    if (thresholdMode) {
        thresholdModeLastPressMillis = millis(); // Start timer
    } else {
      saveTemperaturetoEEPROM();
    }

    leftPressTime = 0;
    rightPressTime = 0;
  }

  // Timeout if user does nothing for 5 seconds
  if (thresholdMode) {
    if (millis() - thresholdModeLastPressMillis >= THRESHOLD_MODE_TIMEOUT) {
      thresholdMode = false;
      textVisible = true;
      saveTemperaturetoEEPROM();
    }
  }

  if (thresholdMode) {
    const unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= blinkInterval) {
      previousMillis = currentMillis;
      textVisible = !textVisible;
    }

    if (!comboDetected) {
      if (leftBtn.isPressed()) {
        temperatureThreshold = constrain(temperatureThreshold - 5, THRESHOLD_MIN, THRESHOLD_MAX);
        thresholdModeLastPressMillis = millis();
      }
      if (rightBtn.isPressed()) {
        temperatureThreshold = constrain(temperatureThreshold + 5, THRESHOLD_MIN, THRESHOLD_MAX);
        thresholdModeLastPressMillis = millis();
      }
    }

  } else {
    textVisible = true;
  }

  monitor.setTarget(temperatureThreshold);

  if (millis() - lastSensorReadMillis >= SENSOR_READ_INTERVAL) {
    lastSensorReadMillis = millis();
    if (mlx.read(objectC, ambientC)) {
      monitor.onReading(millis(), objectC);
    } else {
      monitor.onReadFailed();
      #ifdef DEBUG
      Serial.println("Error reading from MLX sensor!");
      #endif
    }
  }
  monitor.update(millis());

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

  String status = statusLine();
  String text = formatTemperature(static_cast<int>(monitor.temperature()));
  if (thresholdMode) {
    status = F("Set target");
    text = formatTemperature(temperatureThreshold);
  }

  if (drawnSensorFault || status != drawnStatus || text != drawnText || textVisible != drawnTextVisible || thresholdMode != drawnThresholdMode) {
    drawnSensorFault = false;
    drawnStatus = status;
    drawnText = text;
    drawnTextVisible = textVisible;
    drawnThresholdMode = thresholdMode;

    constexpr int y = SCREEN_HEIGHT - 16;
    const int x = (SCREEN_WIDTH / 2) - (text.length() * 12 / 2);

    display.clearDisplay();

    display.setCursor(0, 0);
    display.print(status);

    if (thresholdMode) {
      drawArrows(y);
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
  const bool buzzerOn = monitor.buzzerOn(millis()) && !thresholdMode;
  digitalWrite(BUZZER_PIN, buzzerOn ? LOW : HIGH);
}
