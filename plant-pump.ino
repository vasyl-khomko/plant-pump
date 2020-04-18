#include <Arduino.h>
#include <U8g2lib.h>
#include <OneButton.h>

#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif

#define PUMP_POWER_PIN      3
#define HUMIDITY_POWER_PIN  2
#define HUMIDITY_SENSOR_PIN A0

#define SENSOR_TIME       10000   // 10s second
#define PUMP_TIME         60000   // 60 seconds
#define PUMP_DELAY_TIME   5000    // 5 seconds

#define MIN_ALLOWED_HUMIDITY 10     // 70% of 100%

U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, SCL, SDA, U8X8_PIN_NONE);

unsigned long previusSensorMillis;
unsigned long previusPumpMillis;
unsigned long previusPumpStartMillis;
int humidityMin;
int humidity;

int getHumidity() {
  digitalWrite(HUMIDITY_POWER_PIN, HIGH);
  int sensorValue = analogRead(HUMIDITY_SENSOR_PIN);
  digitalWrite(HUMIDITY_POWER_PIN, LOW);
  sensorValue = map(511 - sensorValue, 0, 511, 0, 100);

  return sensorValue;
}

void draw() {
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setFontRefHeightExtendedText();
  u8g2.setDrawColor(1);
  u8g2.setFontPosTop();
  u8g2.setFontDirection(0);

  u8g2.drawStr(0, 0, "Plant Pump 1.0.0");
  
  char humidityStr[32];
  sprintf(humidityStr, "Humidity: %d; Min: %d", humidity, humidityMin);
  u8g2.drawStr(0, 20, humidityStr);
  
  char timerStr[32];
  int seconds = (previusPumpMillis + PUMP_TIME - millis()) / 1000;
  sprintf(timerStr, "Timer: %d", seconds);
  u8g2.drawStr(0, 40, timerStr);
}

void setup() {
  //Serial.begin(9600);
  u8g2.begin();
  
  pinMode(PUMP_POWER_PIN, OUTPUT);
  pinMode(HUMIDITY_POWER_PIN, OUTPUT);
  
  previusSensorMillis = millis();
  previusPumpMillis = millis();
  previusPumpStartMillis = millis();
  humidity = getHumidity();
}

void loop() {
  humidityMin = analogRead(A1);
  humidityMin = map(humidityMin, 0, 1023, 0, 100);

  // This number will overflow (go back to zero), after approximately 50 days.
  if (millis() < previusSensorMillis) {
    previusSensorMillis = millis();
    previusPumpMillis = millis();
    previusPumpStartMillis = millis();
  }
  
  if (millis() - previusSensorMillis >= SENSOR_TIME) {
    humidity = getHumidity();
    previusSensorMillis = millis();
  }

  if (millis() - previusPumpMillis >= PUMP_TIME) {
    if (humidity < humidityMin && humidity > MIN_ALLOWED_HUMIDITY) {
      digitalWrite(PUMP_POWER_PIN, HIGH);
      previusPumpStartMillis = millis();
    }
    previusPumpMillis = millis();
  }

  if (millis() - previusPumpStartMillis >= PUMP_DELAY_TIME) {
    digitalWrite(PUMP_POWER_PIN, LOW);
  }

  u8g2.clearBuffer();
  draw();
  u8g2.sendBuffer();
}
