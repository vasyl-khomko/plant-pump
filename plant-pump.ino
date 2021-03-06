#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <Thread.h>
#include <OneButton.h>

#include "Array.h"
#include "Time.h"

#define PUMP_POWER_PIN      8
#define HUMIDITY_POWER_PIN  2
#define HUMIDITY_SENSOR_PIN A3

#define SENSOR_TIME       10   // 10s second
#define PUMP_TIME         60   // 60 seconds
#define PUMP_DELAY_TIME   5    // 5 seconds
#define SCREENS_COUNT     5

U8G2_SSD1306_128X64_NONAME_2_HW_I2C u8g2(U8G2_R0, SCL, SDA, U8X8_PIN_NONE);
OneButton screenButton(9, true);
OneButton menuButton(10, true);
OneButton plusButton(A1, true);
OneButton minusButton(A2, true);

Thread buttonsHandlerThread = Thread();
Thread minutelyThread = Thread();
Thread minutely30Thread = Thread();

unsigned long previusSensorMillis;
unsigned long previusPumpMillis;
unsigned long previusPumpStartMillis;

Array<unsigned long, 10> pumpMillis;
Array<byte, 124> humidityMinutelyStat;
Array<byte, 124> humidityMinutely30Stat;

unsigned int sensorTime = SENSOR_TIME;
unsigned int pumpTime = PUMP_TIME;
unsigned int pumpDelayTime = PUMP_DELAY_TIME;

int minHumidity = 20;
int turnOnHumidity = 50;
int currentHumidity = 0;
bool isPumpWorked = false;

unsigned long millisDelta = 0;
unsigned int hoursDelta = 0;
unsigned int minutesDelta = 0;
unsigned int secondsDelta = 0;

int screen = 0;
int menuItem = 0;
int increment = 0;

int getHumidity() {
  digitalWrite(HUMIDITY_POWER_PIN, HIGH);
  int sensorValue = analogRead(HUMIDITY_SENSOR_PIN);
  digitalWrite(HUMIDITY_POWER_PIN, LOW);
  sensorValue = map(511 - sensorValue, 0, 511, 0, 100);

  return sensorValue;
}

void draw() {
  u8g2.setFontRefHeightExtendedText();
  u8g2.setFontPosTop();
  u8g2.setFontDirection(0);

  Time ct = getTime(millis());
  char bufferStr[32];

  if (screen == 0) {
    int diff = currentHumidity - turnOnHumidity;
    if (diff < 0) {
      diff = 0;
    } if (diff > 10) {
      diff = 10;
    }
    u8g2.drawFrame(0, 0, 128, 6);
    u8g2.drawBox(2, 2, diff * 12.4, 2);

    u8g2.setFont(u8g2_font_8x13B_tf);
    sprintf_P(bufferStr, PSTR("Humidity: %02d/%02d"), currentHumidity, turnOnHumidity);
    u8g2.drawStr(0, 7, bufferStr);

    u8g2.setFont(u8g2_font_8x13_tf);

    sprintf_P(bufferStr, PSTR("Time: %02d:%02d:%02d"), ct.hour, ct.minute, ct.second);
    u8g2.drawStr(0, 24, bufferStr);
    
    int pTimerSeconds = (previusPumpMillis + pumpTime*1000 - millis()) / 1000;
    sprintf_P(bufferStr, PSTR("PT: %02d"), (pTimerSeconds < 0 ? 0 : pTimerSeconds));
    u8g2.drawStr(0, 39, bufferStr);

    if (isPumpWorked) {
      int dTimerSeconds = (previusPumpStartMillis + pumpDelayTime*1000 - millis()) / 1000;
      sprintf_P(bufferStr, PSTR("DT: %02d"), (dTimerSeconds < 0 ? 0 : dTimerSeconds));
      u8g2.drawStr(64, 39, bufferStr);
      u8g2.drawHLine(64, 51, 48);
    }

    int sTimerSeconds = (previusSensorMillis + sensorTime * 1000 - millis()) / 1000;
    sprintf_P(bufferStr, PSTR("ST: %02d"), (sTimerSeconds < 0 ? 0 : sTimerSeconds));
    u8g2.drawStr(0, 52, bufferStr);
  }

  if (screen == 1) {
    sprintf_P(bufferStr, PSTR("HmOn:%02d"), turnOnHumidity);
    u8g2.drawStr(0, 0, bufferStr);

    sprintf_P(bufferStr, PSTR("HmMin:%02d"), minHumidity);
    u8g2.drawStr(64, 0, bufferStr);

    sprintf_P(bufferStr, PSTR("Time: %02d:%02d:%02d"), ct.hour, ct.minute, ct.second);
    u8g2.drawStr(0, 16, bufferStr);
  
    sprintf_P(bufferStr, PSTR("PT:%02d"), pumpTime);
    u8g2.drawStr(0, 32, bufferStr);
    
    sprintf_P(bufferStr, PSTR("DT:%02d"), pumpDelayTime);
    u8g2.drawStr(64, 32, bufferStr);

    sprintf_P(bufferStr, PSTR("ST:%02d"), sensorTime);
    u8g2.drawStr(0, 48, bufferStr);

    if (menuItem == 0) {
      u8g2.drawHLine(0, 13, 56);
    }
    if (menuItem == 1) {
      u8g2.drawHLine(64, 13, 128);
    }
    if (menuItem == 2) {
      u8g2.drawHLine(48, 29, 16);
    }
    if (menuItem == 3) {
      u8g2.drawHLine(72, 29, 16);
    }
    if (menuItem == 4) {
      u8g2.drawHLine(96, 29, 16);
    }
    if (menuItem == 5) {
      u8g2.drawHLine(0, 45, 40);
    }
    if (menuItem == 6) {
      u8g2.drawHLine(64, 45, 40);
    }
    if (menuItem == 7) {
      u8g2.drawHLine(0, 58, 40);
    }
  }

  if (screen == 2) {
    for (int i = 0; i < 4; i++) {
      Time t = getTime(pumpMillis.elements[menuItem + i]); 
      sprintf_P(bufferStr, PSTR("%02d/10 %02d:%02d:%02d"), menuItem + i + 1, t.hour, t.minute, t.second);
      u8g2.drawStr(0, i * 16, bufferStr);
    }
  }

  if (screen == 3) {
    drawGraph(humidityMinutelyStat, 20);
  } 

  if (screen == 4) {
    drawGraph(humidityMinutely30Stat, 1);
  }
}

void drawGraph(Array<byte, 124> &array, int interval) {
  char bufferStr[32];
  u8g2.setFont(u8g2_font_p01type_tn );
  u8g2.drawHLine(13, 54, 114);
  u8g2.drawVLine(13, 0, 54);

  for (int i = 0; i < 5; i++) {
    sprintf_P(bufferStr, PSTR("%d"), 100 - i * 20);
    u8g2.drawStr(0, i * 10, bufferStr);
    u8g2.drawHLine(10, 4 + i * 10, 3);
  }

  for (int i = 1; i <= 5; i++) {
    sprintf_P(bufferStr, PSTR("%02d"), i * interval);
    u8g2.drawStr(9 + i * 20, 58, bufferStr);
    u8g2.drawVLine(13 + i * 20, 54, 3);
  }

  for (int i = 1; i < 114; i++) {
    auto x1 = 13 + i - 1;
    auto x2 = 13 + i;
    auto y1 = 54 - array.elements[i - 1] / 2;
    auto y2 = 54 - array.elements[i] / 2;
    u8g2.drawLine(x1, y1, x2, y2);
  }
}

void setup() {
  u8g2.begin();
  
  pinMode(PUMP_POWER_PIN, OUTPUT);
  pinMode(HUMIDITY_POWER_PIN, OUTPUT);

  buttonsHandlerThread.enabled = false;
  buttonsHandlerThread.setInterval(200);
  buttonsHandlerThread.onRun([]() {
    incrementSelectedProperty(increment);
  });

  screenButton.attachClick(onClickScreenButton);
  menuButton.attachClick(onClickMenuButton);
  plusButton.attachClick(onClickPlusButton);
  plusButton.attachLongPressStart(onLongPressStartPlusButton);
  plusButton.attachLongPressStop(onLongPressStopPlusButton);
  minusButton.attachClick(onClickMinusButton);
  minusButton.attachLongPressStart(onLongPressStartMinusButton);
  minusButton.attachLongPressStop(onLongPressStopMinusButton);
  
  previusSensorMillis = millis();
  previusPumpMillis = millis();
  previusPumpStartMillis = millis();
  currentHumidity = getHumidity();

  minutelyThread.setInterval(60000UL); // every minute
  minutelyThread.onRun([]() {
    humidityMinutelyStat.addFirst(currentHumidity);
  });

  minutely30Thread.setInterval(1800000UL); // every 30 minutes
  minutely30Thread.onRun([]() {
    humidityMinutely30Stat.addFirst(currentHumidity);
    screen = -1; // Sleep
  });
}

void loop() {
  // This number will overflow (go back to zero), after approximately 50 days.
  if (millis() < previusSensorMillis) {
    previusSensorMillis = millis();
    previusPumpMillis = millis();
    previusPumpStartMillis = millis();
  }
  
  if (millis() - previusSensorMillis >= sensorTime*1000) {
    currentHumidity = getHumidity();
    previusSensorMillis = millis();
  }

  if (millis() - previusPumpMillis >= pumpTime*1000) {
    if (currentHumidity < turnOnHumidity && currentHumidity > minHumidity) {
      digitalWrite(PUMP_POWER_PIN, HIGH);
      pumpMillis.addFirst(millis());
      previusPumpStartMillis = millis();
      isPumpWorked = true;
    }
    previusPumpMillis = millis();
  }

  if (millis() - previusPumpStartMillis >= pumpDelayTime*1000) {
    digitalWrite(PUMP_POWER_PIN, LOW);
    isPumpWorked = false;
  }

  if (buttonsHandlerThread.shouldRun()) {
    buttonsHandlerThread.run();
  }

  if (minutelyThread.shouldRun()) {
    minutelyThread.run();
  }

  if (minutely30Thread.shouldRun()) {
    minutely30Thread.run();
  }

  screenButton.tick();
  menuButton.tick();
  plusButton.tick();
  minusButton.tick();
  
  u8g2.firstPage();
  do {
    draw();
  } while (u8g2.nextPage());
}

Time getTime(unsigned long millis) {
  unsigned long actualSeconds = (millis + millisDelta) / 1000;
  
  Time time;
  time.hour = actualSeconds / (60 * 60) % 24;
  time.minute = actualSeconds / 60 % 60;
  time.second = actualSeconds % 60;

  return time;
}

void updateMillisDelta() {
  millisDelta = (unsigned long) hoursDelta * 3600000UL;
  millisDelta += (unsigned long) minutesDelta * 60000UL;
  millisDelta += (unsigned long) secondsDelta * 1000UL;
}

int incrementProperty(int currentValue, int increment, int maxValue) {
  int value = currentValue + increment;

  if (value >= maxValue) {
    value = 0;  
  } else if (value < 0) {
    value = maxValue-1;
  }

  return value;
}

void incrementSelectedProperty(int increment) {
  
  if (menuItem == 0) {
    turnOnHumidity = incrementProperty(turnOnHumidity, increment, 100);
  }
  if (menuItem == 1) {
    minHumidity = incrementProperty(minHumidity, increment, 100);
  }
  if (menuItem == 2) {
    hoursDelta = incrementProperty(hoursDelta, increment, 24);
    updateMillisDelta();
  }
  if (menuItem == 3) {
    minutesDelta = incrementProperty(minutesDelta, increment, 60);
    updateMillisDelta();
  }
  if (menuItem == 4) {
    secondsDelta = incrementProperty(secondsDelta, increment, 60);
    updateMillisDelta();
  }
  if (menuItem == 5) {
    pumpTime = incrementProperty(pumpTime, increment, 100);
  }
  if (menuItem == 6) {
    pumpDelayTime = incrementProperty(pumpDelayTime, increment, 100);
  }
}

void onClickScreenButton() {
  screen = ++screen % SCREENS_COUNT;
  menuItem = 0;
}

void onClickMenuButton() {
  if (screen == 1) {
    menuItem = ++menuItem % 8;
  } else if(screen == 2) {
    menuItem = ++menuItem % 7;
  }
}

void onClickPlusButton() {
  if (screen == 1) {
    incrementSelectedProperty(1);
  }
}

void onClickMinusButton() {
  if (screen == 1) {
    incrementSelectedProperty(-1);
  }
}

void onLongPressStartPlusButton(){
  if (screen == 1) {
    buttonsHandlerThread.enabled = true;
    increment = 1;
  }
}

void onLongPressStopPlusButton() {
  if (screen == 1) {
    buttonsHandlerThread.enabled = false;
  }
}

void onLongPressStartMinusButton(){
  if (screen == 1) {
    buttonsHandlerThread.enabled = true;
    increment = -1;
  }
}

void onLongPressStopMinusButton() {
  if (screen == 1) {
    buttonsHandlerThread.enabled = false;
  }
}
