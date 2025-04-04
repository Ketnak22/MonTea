#include <Arduino.h>
#include <Wire.h>

#include <Adafruit_MLX90614.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>

#include <encoder.h>

// Initialize I2C for pins 16 (SDA) and 17 (SCL)
MbedI2C i2c(p16, p17);

#define MLX_ADDRESS 0x5A

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
#define SCREEN_ADDRESS 0x3C

#define ENCODER_CLK_PIN 14
#define ENCODER_DATA_PIN 15
#define ENCODER_SW_PIN 13

// Initialize MLX90614 sensor and OLED display
Adafruit_MLX90614 mlx;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &i2c, -1);

RotaryEncoder encoder(ENCODER_CLK_PIN, ENCODER_DATA_PIN, ENCODER_SW_PIN);

void setup() {
  i2c.begin();
  Serial.begin(9600);
  while (!Serial);

  if (!mlx.begin(MLX_ADDRESS, &i2c)) {
    Serial.println(F("Error connecting to MLX sensor. Check wiring."));
    while (1);
  }

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    while (1);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
}
bool ambientTemp = true;
void loop() {
  encoder.loop();

  if (encoder.isPressed()) {
    ambientTemp = !ambientTemp;
  }

  char buff[50];
  if (ambientTemp) {
    sprintf(buff, "otoczenia: %d C", static_cast<int>(mlx.readAmbientTempC()));
  } else {
    sprintf(buff, "obiektu: %d C", static_cast<int>(mlx.readObjectTempC()));
  }

  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("Temperatura");
  display.setCursor(0, 16);
  display.print(buff);

  display.display();

}
