/*
#include <Arduino.h>

#include <ezButton.h>

#define CLK_PIN 19
#define DATA_PIN 18
#define SW_PIN 20

ezButton btn(SW_PIN);

int CLK_value = 0;
int prev_CLK_value = 0;

void setup() {
  Serial.begin(9600);
  while (!Serial);

  pinMode(CLK_PIN, INPUT);
  pinMode(DATA_PIN, INPUT);
  pinMode(SW_PIN, INPUT);

  btn.setDebounceTime(50);

  prev_CLK_value = digitalRead(CLK_PIN);
}

void loop() {
  btn.loop();

  CLK_value = digitalRead(CLK_PIN);
  
  if (CLK_value != prev_CLK_value && CLK_value == HIGH) {
    if (digitalRead(DATA_PIN) == HIGH) {
      Serial.println("Turned left");
    } else {
      Serial.println("Turned right");
    }
  }

  prev_CLK_value = CLK_value;

  if (btn.isPressed()) {
    Serial.println("Button is pressed");
  }
}

*/

#include <Arduino.h>

#include <ezButton.h>

#include <Wire.h>
#include <Adafruit_MLX90614.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>

// Initialize I2C for pins 16 (SDA) and 17 (SCL)
MbedI2C i2c(p16, p17);

#define MLX_ADDRESS 0x5A

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
#define SCREEN_ADDRESS 0x3C

#define ENCODER_CLK_PIN 14
#define ENCODER_DATA_PIN 15
#define ENCODER_SW_PIN 13

Adafruit_MLX90614 mlx;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &i2c, -1);

ezButton encoder_btn(ENCODER_SW_PIN);
int CLK_value = 0;
int prev_CLK_value = 0;

void setup() {
  i2c.begin();
  // Serial.begin(9600);
  // while (!Serial);

  if (!mlx.begin(MLX_ADDRESS, &i2c)) {
    // Serial.println("Error connecting to MLX sensor. Check wiring.");
    while (1);
  }

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    // Serial.println(F("SSD1306 allocation failed"));
    while (1);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  encoder_btn.setDebounceTime(50);
}

void loop() {
  encoder_btn.loop();

  CLK_value = digitalRead(ENCODER_CLK_PIN);
  
  if (CLK_value != prev_CLK_value && CLK_value == HIGH) {
    if (digitalRead(ENCODER_DATA_PIN) == HIGH) {
      Serial.println("Turned left");
    } else {
      Serial.println("Turned right");
    }
  }

  prev_CLK_value = CLK_value;

  if (encoder_btn.isPressed()) {
    Serial.println("Button is pressed");
  }

  delay(500);
}
