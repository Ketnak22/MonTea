#include <Arduino.h>
#include <Wire.h>

#include <Adafruit_MLX90614.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>

#define DEBUG

// Initialize I2C for pins 16 (SDA) and 17 (SCL)
MbedI2C i2c(p16, p17);

#define MLX_ADDRESS 0x5A

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
#define SCREEN_ADDRESS 0x3C

#define ERROR_LED_PIN 15

// Initialize MLX90614 sensor and OLED display
Adafruit_MLX90614 mlx;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &i2c, -1);

void setup() {
  pinMode(ERROR_LED_PIN, OUTPUT);

  i2c.begin();

#ifdef DEBUG
  Serial.begin(9600);
  while (!Serial);
#endif
  if (!mlx.begin(MLX_ADDRESS, &i2c)) {
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
  display.clearDisplay();
  display.setCursor(0, 0);
  
  if (isnan(mlx.readAmbientTempC())) {
    digitalWrite(ERROR_LED_PIN, HIGH);
    while(1);
  }
  
  display.print("Temperatura");

  char buff[50];
  display.setCursor(0, 12);
  sprintf(buff, "otoczenia: %d C", static_cast<int>(mlx.readAmbientTempC()));
  display.print(buff);
  
  display.setCursor(0, 24);
  sprintf(buff, "obiektu: %d C", static_cast<int>(mlx.readObjectTempC()));
  display.print(buff);
  
  display.display();
}