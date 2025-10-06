#include <Arduino.h>
#include <Wire.h>
#include <EEPROM.h>

#include <ezButton.h>

#include <Adafruit_MLX90614.h>
#include <Adafruit_SSD1306.h>

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

#define THRESHOLD_MODE_TIMEOUT 5000

#define LEFT_BUTTON_PIN 27
#define RIGHT_BUTTON_PIN 26

// Initialize MLX90614 sensor and OLED display
Adafruit_MLX90614 mlx;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT);

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

unsigned long thresholdModeStartMillis = 0;

int temperatureThreshold = 40;

const String formatTemperature(const int temperature) {
  return String(temperature) + (char)247 + 'C';
}

void drawArrows(const int y) {
  display.drawChar(10, y, '<', WHITE, WHITE, 2);
  display.drawChar(128 - 10 - 12, y, '>', WHITE, WHITE, 2);
}

void setup() {
#ifdef DEBUG
  Serial.begin(9600);
  while (!Serial);
#endif

  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(EEPROM_ADDR, temperatureThreshold);
  if (temperatureThreshold < 0 || temperatureThreshold >= 100) {
    temperatureThreshold = 40;
  }
#ifdef DEBUG
  Serial.println(F("Loaded temperatureThreshold from EEPROM!"));
  Serial.print(F("temperatureThreshold = "));
  Serial.println(temperatureThreshold);
  Serial.println();
#endif

  // Set pins: 16 for SDA and 17 for SCL
  Wire.setSDA(0);
  Wire.setSCL(1);

  // Set frequency to 50KHz (MLX90164 works only in 10-100KHz range)
  Wire.setClock(50000);

  // Initialize I2C
  Wire.begin();

  pinMode(ERROR_LED_PIN, OUTPUT);

  digitalWrite(ERROR_LED_PIN, HIGH);
  delay(500);
  digitalWrite(ERROR_LED_PIN, LOW);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, HIGH);
  
  leftBtn.setDebounceTime(50);
  rightBtn.setDebounceTime(50);

  if (!mlx.begin(MLX_ADDRESS, &Wire)) {
    #ifdef DEBUG
      Serial.println(F("Error connecting to MLX sensor. Check wiring."));
    #endif
    digitalWrite(ERROR_LED_PIN, HIGH);
    while (1);
  }

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
}

void saveTemperaturetoEEPROM() {
  if (EEPROM.read(EEPROM_ADDR) != temperatureThreshold) {
        EEPROM.put(EEPROM_ADDR, temperatureThreshold);
        if (EEPROM.commit()) {
          #ifdef DEBUG
          Serial.println(F("Saved temperatureThreshold to EEPROM!"));
          Serial.print(F("temperatureThreshold = "));
          Serial.println(temperatureThreshold);
          Serial.println();
          #endif
        }
      } else {
        #ifdef DEBUG
        Serial.println(F("temperatureThreshold wasn't changed!"));
        Serial.print(F("temperatureThreshold = "));
        Serial.println(temperatureThreshold);
        Serial.println();
        #endif
      }
}
// TODO Dodać możliwość startu trybu czuwania
void loop() {
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
        thresholdModeStartMillis = millis(); // Start timer
    } else {
      saveTemperaturetoEEPROM();
    }

    leftPressTime = 0;
    rightPressTime = 0;
  }

  // Timeout if user does nothing for 5 seconds
  if (thresholdMode) {
    if (millis() - thresholdModeStartMillis >= THRESHOLD_MODE_TIMEOUT) {
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
        temperatureThreshold -= 5;
      }
      if (rightBtn.isPressed()) {
        temperatureThreshold += 5;
      }
    }

  } else {
    textVisible = true;
  }

  display.clearDisplay();

  if (isnan(mlx.readAmbientTempC())) {
    digitalWrite(ERROR_LED_PIN, HIGH);
    #ifdef DEBUG
    Serial.println("Error reading from MLX sensor!");
    #endif
    while(1);
  }

  constexpr int y = (32 / 2) - (16 / 2);

  drawArrows(y);

  String text = formatTemperature(static_cast<int>(mlx.readObjectTempC()));
  if (thresholdMode) {
    text = formatTemperature(temperatureThreshold);
  }
  const int x = (SCREEN_WIDTH / 2) - (text.length() * 12 / 2);

  if (textVisible) {
    display.setCursor(x, y);
    display.setTextSize(2);
    display.print(text);
    display.setTextSize(1);
  }

  display.display();

  beepMillis = millis();
  if (static_cast<int>(mlx.readObjectTempC()) <= temperatureThreshold && !thresholdMode) {
    unsigned long cycleTime = beepMillis % BEEP_INTERVAL;
    if (cycleTime < 500) {
      digitalWrite(BUZZER_PIN, LOW); // ON
    } else {
      digitalWrite(BUZZER_PIN, HIGH); // OFF
    }
  } else {
    digitalWrite(BUZZER_PIN, HIGH); // OFF when not above threshold
  }
}
