#include <Arduino.h>
#include <Wire.h>
#include <EEPROM.h>

#include <ezButton.h>

#include <Adafruit_MLX90614.h>
#include <Adafruit_SSD1306.h>

// #define DEBUG // Uncomment to enable debug output

#define EEPROM_SIZE 4 // Size of needed EEPROM in bytes
#define EEPROM_ADDR 0 // EEPROM address to store temperature threshold

#define MLX_ADDRESS 0x5A // MLX90614 I2C address

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
#define SCREEN_ADDRESS 0x3C // OLED display I2C address

#define ERROR_LED_PIN 15

#define BUZZER_PIN 0
#define BEEP_INTERVAL 1000
#define BEEP_TIME 1000

#define SET_BUTTON_PIN 13

#define HOLD_TIME 2500 // 2.5 seconds hold time for toggling threshold mode

#define MAX_SET_TEMPERATURE 60 // Maximum temperature threshold value
#define MIN_SET_TEMPERATURE 25  // Minimum temperature threshold value

// Initialize MLX90614 sensor and OLED display
Adafruit_MLX90614 mlx;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT);

ezButton setBtn(SET_BUTTON_PIN, INPUT_PULLUP);

// Variables for blinking text
bool thresholdMode = false; // = blinking mode
bool buttonsPreviouslyPressed = false;
unsigned long previousMillis = 0;  // Stores the last time the text was toggled
constexpr unsigned long blinkInterval = 500;
bool textVisible = true;

unsigned long setPressTime = 0;
unsigned long thresholdModeStartTime = 0; // Time when threshold mode was entered or last incremented

unsigned long beepMillis = 0;

int temperatureThreshold = 40;

const String formatTemperature(const int temperature) {
  return String(temperature) + (char)247 + 'C';
}

// Draw arrows on the display
void drawArrows(const int y) {
  display.drawChar(10, y, '<', WHITE, WHITE, 2);
  display.drawChar(128 - 10 - 12, y, '>', WHITE, WHITE, 2);
}

void setup() {
#ifdef DEBUG
  Serial.begin(9600); 
  while (!Serial);
  Serial.println(F("Serial started!"));
#endif

  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(EEPROM_ADDR, temperatureThreshold);
  if (temperatureThreshold < MIN_SET_TEMPERATURE || temperatureThreshold >= MAX_SET_TEMPERATURE) {
    temperatureThreshold = 40;
  }
#ifdef DEBUG
  Serial.println(F("Loaded temperatureThreshold from EEPROM!"));
  Serial.print(F("temperatureThreshold = "));
  Serial.println(temperatureThreshold);
  Serial.println();
#endif

  // Set pins: 16 for SDA and 17 for SCL
  Wire.setSDA(16);
  Wire.setSCL(17);

  // Set frequency to 50KHz (MLX90164 works only in 10-100KHz range)
  Wire.setClock(50000);

  // Initialize I2C
  Wire.begin();

  pinMode(ERROR_LED_PIN, OUTPUT);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, HIGH); // Disable buzzer
  
  setBtn.setDebounceTime(50);
  
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    #ifdef DEBUG
    Serial.println(F("SSD1306 allocation failed"));
    #endif
    digitalWrite(ERROR_LED_PIN, HIGH);
    while (1);
  }
  
  if (!mlx.begin(MLX_ADDRESS, &Wire)) {
    #ifdef DEBUG
      Serial.println(F("Error connecting to MLX sensor. Check wiring."));
    #endif
    digitalWrite(ERROR_LED_PIN, HIGH);
    display.print(F("Error reading from MLX sensor!"));
    display.display();
    while (1);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
}

void loop() {
  setBtn.loop();

  if (!thresholdMode) {
    if (setBtn.isPressed()) {
      setPressTime = millis();
    }
    const bool holdDetected = setPressTime != 0 && millis() - setPressTime >= HOLD_TIME;
    if (holdDetected) {
      thresholdMode = true;
      textVisible = true;
      previousMillis = millis();
      thresholdModeStartTime = millis(); // Start threshold mode timer
      setPressTime = 0;
      buttonsPreviouslyPressed = false;
    }
  }

  if (thresholdMode) {
    const unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= blinkInterval) {
      previousMillis = currentMillis;
      textVisible = !textVisible;
    }

    // If button pressed, increment threshold and reset timer
    if (setBtn.isPressed()) {
      if (!buttonsPreviouslyPressed) {
        buttonsPreviouslyPressed = true;
        temperatureThreshold += 5;
        if (temperatureThreshold > MAX_SET_TEMPERATURE) {
          temperatureThreshold = MIN_SET_TEMPERATURE;
        }
        thresholdModeStartTime = millis(); // Reset timeout on press
      }
    } else {
      buttonsPreviouslyPressed = false;
    }

    // Exit threshold mode if timeout reached
    if (millis() - thresholdModeStartTime >= HOLD_TIME) {
      thresholdMode = false;
      textVisible = true;
      buttonsPreviouslyPressed = false;
      // Save threshold to EEPROM if changed
      if (EEPROM.read(EEPROM_ADDR) != temperatureThreshold) {
        EEPROM.put(EEPROM_ADDR, temperatureThreshold);
        EEPROM.commit();

        #ifdef DEBUG
        Serial.print(F("Saved temperatureThreshold to EEPROM\ntemperatureThreshold = "));
        Serial.println(temperatureThreshold);
        #endif
      }
    }
  }

  display.clearDisplay();

  if (isnan(mlx.readAmbientTempC())) {
    digitalWrite(ERROR_LED_PIN, HIGH);
    display.print(F("Error reading from MLX sensor!"));
    display.display();
    #ifdef DEBUG
    Serial.println("Error reading from MLX sensor!");
    #endif
    while(1);
  }

  constexpr int y = (SCREEN_HEIGHT / 2) - (16 / 2);

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
  
  // Beep repeatedly
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
