#include <Arduino.h>
#include <Wire.h>
#include <EEPROM.h>

#include <ezButton.h>

#include <Adafruit_MLX90614.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>

#define DEBUG

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

int temperatureThreshold = -1;

void setup() {
  // Initialize I2C for pins 16 (SDA) and 17 (SCL)
  Wire.setSDA(16);
  Wire.setSCL(17);
  Wire.setClock(100000);
  Wire.begin();

  pinMode(ERROR_LED_PIN, OUTPUT);
  
  leftBtn.setDebounceTime(50);
  rightBtn.setDebounceTime(50);

#ifdef DEBUG
  Serial.begin(9600);
  while (!Serial);
#endif

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

  display.clearDisplay();
  display.setCursor(0, 0);

  if (isnan(mlx.readAmbientTempC())) {
    digitalWrite(ERROR_LED_PIN, HIGH);
    while(1);
  }

  constexpr int y = (32 / 2) - (16 / 2);

  display.drawChar(10, y, '<', WHITE, WHITE, 2);
  display.drawChar(128 - 10 - 12, y, '>', WHITE, WHITE, 2);

  String text = "";
  // text += static_cast<int>(mlx.readObjectTempC());
  text += 20;
  text += (char)247;
  text += "C";

  /*
  Parameters:
    s – Desired text size. 1 is default 6x8, 2 is 12x16, 3 is 18x24, etc
  */

  const int x = (SCREEN_WIDTH / 2) - (text.length() * 12 / 2);

  display.setCursor(x, y);
  display.setTextSize(2);
  display.print(text);
  display.setTextSize(1);

  display.display();
}
