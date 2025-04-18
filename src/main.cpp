#include <Arduino.h>
#include <Wire.h>
#include <EEPROM.h>

#include <ezButton.h>

#include <Adafruit_MLX90614.h>
#include <Adafruit_SSD1306.h>

#define DEBUG

#define EEPROM_SIZE 4
#define EEPROM_ADDR 0

#define MLX_ADDRESS 0x5A

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
#define SCREEN_ADDRESS 0x3C

#define ERROR_LED_PIN 15

#define LEFT_BUTTON_PIN 13
#define RIGHT_BUTTON_PIN 12

// Initialize MLX90614 sensor and OLED display
Adafruit_MLX90614 mlx;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT);

ezButton leftBtn(LEFT_BUTTON_PIN, INPUT_PULLUP);
ezButton rightBtn(RIGHT_BUTTON_PIN, INPUT_PULLUP);

// Variables for blinking text
bool thresholdMode = false; // = blinking mode
bool buttonsPreviouslyPressed = false;
unsigned long previousMillis = 0;  // Stores the last time the text was toggled
const long blinkInterval = 500;
bool textVisible = true;

int temperatureThreshold = 40;

const String formatTemperature(const int temperature) {
  String text = "";
  text += temperature;
  text += (char)247;
  text += "C";
  return text;
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
#endif

  // Set pins: 16 for SDA and 17 for SCL
  Wire.setSDA(16);
  Wire.setSCL(17);

  // Set frequency to 50KHz (MLX90164 works only in 10-100KHz range)
  Wire.setClock(50000);

  // Initialize I2C
  Wire.begin();

  pinMode(ERROR_LED_PIN, OUTPUT);
  
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

void loop() {
  leftBtn.loop();
  rightBtn.loop();

  const bool bothPressed = leftBtn.isPressed() && rightBtn.isPressed();
  if (bothPressed && !buttonsPreviouslyPressed) {
    thresholdMode = !thresholdMode;
    textVisible = true;
    previousMillis = millis();

    if (!thresholdMode) {
      EEPROM.put(EEPROM_ADDR, temperatureThreshold);
      if (EEPROM.commit()) {
        #ifdef DEBUG
        Serial.println(F("Saved temperatureThreshold to EEPROM!"));
        Serial.print(F("temperatureThreshold = "));
        Serial.println(temperatureThreshold);
        #endif
      }

    }
  }
  buttonsPreviouslyPressed = bothPressed;

  if (thresholdMode) {
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= blinkInterval) {
      previousMillis = currentMillis;
      textVisible = !textVisible;
    }

    if (!bothPressed) {
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

  if ((int)mlx.readObjectTempC() >= temperatureThreshold && !thresholdMode) {
    digitalWrite(ERROR_LED_PIN, HIGH);
  } else {
    digitalWrite(ERROR_LED_PIN, LOW);
  }
}
