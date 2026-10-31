#include <Arduino.h>
#include <Wire.h>
#include <EEPROM.h>

#include <ezButton.h>

#include <Adafruit_SSD1306.h>

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
#define BEEP_INTERVAL 1000

#define WATCHDOG_TIMEOUT 3000 // well above a worst case pass full of I2C timeouts

#define SENSOR_READ_INTERVAL 250 // the MLX only refreshes a few times per second
#define SENSOR_MAX_FAILURES 8 // failed reads in a row before showing a fault

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

unsigned long beepMillis = 0;

unsigned long thresholdModeLastPressMillis = 0;

int temperatureThreshold = 40;

float objectC = 0;
float ambientC = 0;
unsigned long lastSensorReadMillis = 0;
int sensorFailures = 0;

// What is on the screen right now
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

  if (!mlx.read(objectC, ambientC)) {
    #ifdef DEBUG
      Serial.println(F("Error connecting to MLX sensor. Check wiring."));
    #endif
    // loop() shows the fault and keeps retrying, a wire can be reseated live
    sensorFailures = SENSOR_MAX_FAILURES;
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
// TODO Dodać możliwość startu trybu czuwania
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

  if (millis() - lastSensorReadMillis >= SENSOR_READ_INTERVAL) {
    lastSensorReadMillis = millis();
    if (mlx.read(objectC, ambientC)) {
      sensorFailures = 0;
    } else if (sensorFailures < SENSOR_MAX_FAILURES) {
      sensorFailures++;
      #ifdef DEBUG
      Serial.println("Error reading from MLX sensor!");
      #endif
    }
  }

  // Short dropouts just keep the last good value, only a long one is a fault
  const bool sensorFault = sensorFailures >= SENSOR_MAX_FAILURES;
  digitalWrite(ERROR_LED_PIN, sensorFault ? HIGH : LOW);
  if (sensorFault) {
    if (!drawnSensorFault) {
      drawnSensorFault = true;
      showSensorError();
    }
    digitalWrite(BUZZER_PIN, HIGH); // OFF
    return;
  }

  // Screen and alarm work from the same value
  const int objectTemperature = static_cast<int>(objectC);

  String text = formatTemperature(objectTemperature);
  if (thresholdMode) {
    text = formatTemperature(temperatureThreshold);
  }

  if (drawnSensorFault || text != drawnText || textVisible != drawnTextVisible || thresholdMode != drawnThresholdMode) {
    drawnSensorFault = false;
    drawnText = text;
    drawnTextVisible = textVisible;
    drawnThresholdMode = thresholdMode;

    constexpr int y = (32 / 2) - (16 / 2);
    const int x = (SCREEN_WIDTH / 2) - (text.length() * 12 / 2);

    display.clearDisplay();

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

  beepMillis = millis();
  if (objectTemperature <= temperatureThreshold && !thresholdMode) {
    unsigned long cycleTime = beepMillis % BEEP_INTERVAL;
    if (cycleTime < 500) {
      digitalWrite(BUZZER_PIN, LOW); // ON
    } else {
      digitalWrite(BUZZER_PIN, HIGH); // OFF
    }
  } else {
    digitalWrite(BUZZER_PIN, HIGH); // OFF while the tea is above the threshold
  }
}
