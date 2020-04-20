#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <Thread.h>
#include <OneButton.h>

#include "Array.h"
#include "Time.h"

#define PUMP_POWER_PIN      3
#define HUMIDITY_POWER_PIN  2
#define HUMIDITY_SENSOR_PIN A0

#define SENSOR_TIME       10   // 10s second
#define PUMP_TIME         60   // 60 seconds
#define PUMP_DELAY_TIME   5    // 5 seconds

U8G2_SSD1306_128X64_NONAME_2_HW_I2C u8g2(U8G2_R0, SCL, SDA, U8X8_PIN_NONE);
OneButton screenButton(4, true);
OneButton menuButton(5, true);
OneButton plusButton(6, true);

Thread thread = Thread();
Thread minutelyThread = Thread();
Thread minutely20Thread = Thread();

unsigned long previusSensorMillis;
unsigned long previusPumpMillis;
unsigned long previusPumpStartMillis;

Array<unsigned long, 10> pumpMillis;
Array<byte, 124> humidityMinutelyStat;
Array<byte, 124> humidityMinutely20Stat;

unsigned int sensorTime = SENSOR_TIME;
unsigned int pumpTime = PUMP_TIME;
unsigned int pumpDelayTime = PUMP_DELAY_TIME;

int minHumidity = 20;
int turnOnHumidity = 50;
int currentHumidity = 0;

unsigned int hoursDelta = 0;
unsigned int minutesDelta = 0;
unsigned int secondsDelta = 0;

byte screen = 0;
byte menuItem = 0;

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
    u8g2.drawFrame(0, 0, 128, 16);
    u8g2.drawBox(2, 2, 62, 12);
    u8g2.setFont(u8g2_font_8x13B_tf);
    u8g2.setDrawColor(0);

    sprintf_P(bufferStr, PSTR("%02d/%02d"), currentHumidity, turnOnHumidity);
    u8g2.drawStr(2, 2, bufferStr);

    u8g2.setDrawColor(1);
    u8g2.setFont(u8g2_font_8x13_tf);

    sprintf_P(bufferStr, PSTR("Time: %02d:%02d:%02d"), ct.hour, ct.minute, ct.second);
    u8g2.drawStr(0, 20, bufferStr);
    
    int pTimerSeconds = (previusPumpMillis + pumpTime*1000 - millis()) / 1000;
    sprintf_P(bufferStr, PSTR("PT: %02d"), pTimerSeconds);
    u8g2.drawStr(0, 36, bufferStr);

    if (millis() - previusPumpStartMillis <= pumpDelayTime * 1000) {
      int dTimerSeconds = (previusPumpStartMillis + pumpDelayTime*1000 - millis()) / 1000;
      sprintf_P(bufferStr, PSTR("DT: %02d"), dTimerSeconds);
      u8g2.drawStr(64, 36, bufferStr);
      u8g2.drawHLine(64, 49, 48);
    }

    int sTimerSeconds = (previusSensorMillis + sensorTime * 1000 - millis()) / 1000;
    sprintf_P(bufferStr, PSTR("ST: %02d"), sTimerSeconds);
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
    sprintf_P(bufferStr, PSTR("Screen 3"));
    u8g2.drawStr(0, 0, bufferStr);
    for (int i = 1; i < humidityMinutelyStat.size; i++) {
      auto x1 = 128 - i;
      auto x2 = 127 - i;
      auto y1 = 63 - humidityMinutelyStat.elements[i - 1] / 2;
      auto y2 = 63 - humidityMinutelyStat.elements[i] / 2;
      u8g2.drawLine(x1, y1, x2, y2);
    }
  }
}

void setup() {
  Serial.begin(9600);
  u8g2.begin();
  
  pinMode(PUMP_POWER_PIN, OUTPUT);
  pinMode(HUMIDITY_POWER_PIN, OUTPUT);

  thread.enabled = false;
  thread.setInterval(200);
  thread.onRun([]() {
    incrementSelectedProperty();
  });

  screenButton.attachClick(onClickScreenButton);
  menuButton.attachClick(onClickMenuButton);
  plusButton.attachClick(onClickPlusButton);
  plusButton.attachLongPressStart(onLongPressStartPlusButton);
  plusButton.attachLongPressStop(onLongPressStopPlusButton);
  
  previusSensorMillis = millis();
  previusPumpMillis = millis();
  previusPumpStartMillis = millis();
  currentHumidity = getHumidity();

  minutelyThread.setInterval(60000UL); // every minute
  minutelyThread.onRun([]() {
    humidityMinutelyStat.addFirst(currentHumidity);
  });

  minutely20Thread.setInterval(1200000UL); // every 20 minutes
  minutely20Thread.onRun([]() {
    humidityMinutely20Stat.addFirst(currentHumidity);
  });
}

void loop() {
  //currentHumidity = analogRead(A1);
  //currentHumidity = map(turnOnHumidity, 0, 1023, 0, 100);

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
    }
    previusPumpMillis = millis();
  }

  if (millis() - previusPumpStartMillis >= pumpDelayTime*1000) {
    digitalWrite(PUMP_POWER_PIN, LOW);
  }

  if(thread.shouldRun()) {
    thread.run();
  }

  if(minutelyThread.shouldRun()) {
    minutelyThread.run();
  }

  screenButton.tick();
  menuButton.tick();
  plusButton.tick();
  
  u8g2.firstPage();
  do {
    draw();
  } while (u8g2.nextPage());
}

Time getTime(unsigned long millis) {
  unsigned long actualSeconds = millis / 1000;
  actualSeconds += ((unsigned long)hoursDelta) * 60 * 60;
  actualSeconds += minutesDelta * 60;
  actualSeconds += secondsDelta;
  
  Time time;
  time.hour = actualSeconds / (60 * 60) % 24;
  time.minute = actualSeconds / 60 % 60;
  time.second = actualSeconds % 60;

  return time;
}

void incrementSelectedProperty() {
  
  if (menuItem == 0) {
    turnOnHumidity = ++turnOnHumidity % 100;
  }
  if (menuItem == 1) {
    minHumidity = ++minHumidity % 100;
  }
  if (menuItem == 2) {
    hoursDelta = ++hoursDelta % 24;
  }
  if (menuItem == 3) {
    minutesDelta = ++minutesDelta % 60;
  }
  if (menuItem == 4) {
    secondsDelta = ++secondsDelta % 60;
  }
  if (menuItem == 5) {
    pumpTime = ++pumpTime % 100;
  }
  if (menuItem == 6) {
    pumpDelayTime = ++pumpDelayTime % 100;
  }
}

void onClickScreenButton() {
  screen = ++screen % 4;
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
    incrementSelectedProperty();
  }
}

void onLongPressStartPlusButton(){
  if (screen == 1) {
    thread.enabled = true;
  }
}

void onLongPressStopPlusButton() {
  if (screen == 1) {
    thread.enabled = false;
  }
}
