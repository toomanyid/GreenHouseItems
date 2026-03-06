#include "AppStateSetup.h"
#include "AppState.h"
#include "AppStateIdle.h"
#include "MotorDriver.h"
#include "MotorPositionConfig.h"
#include "RotaryEncoder.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

extern Adafruit_SH1106G display;
extern RotaryEncoder encoder;
extern MotorDriver& motor;
extern AppStateContext appStateContext;

AppStateSetup& AppStateSetup::getInstance() {
  static AppStateSetup instance;
  return instance;
}

AppStateSetup::AppStateSetup()
  : selectedIndex(0), lastEncoderValue(0) {
}

void AppStateSetup::onEnter() {
  motor.stop();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  lastEncoderValue = encoder.getValue();
}

void AppStateSetup::onKey(Key key) {
  if (key == Key::Push) {
    MotorPositionConfigManager::save();
    motor.setPositionLimits(MotorPositionConfigManager::getInstance().getMinSec(), MotorPositionConfigManager::getInstance().getTopSec());
    appStateContext.setState(&AppStateIdle::getInstance());
    return;
  }
  if (key == Key::Cw) {
    selectedIndex--;
    if (selectedIndex < 0) selectedIndex = 0;
    return;
  }
  if (key == Key::Ccw) {
    selectedIndex++;
    if (selectedIndex >= MotorPositionConfig::FIELD_COUNT) selectedIndex = MotorPositionConfig::FIELD_COUNT - 1;
  }
}

void AppStateSetup::onUpdate() {
  MotorPositionConfig& config = MotorPositionConfigManager::getInstance();
  
  // 로터리: 선택된 값 증감 (위치값 1초=10단위, 전류 0.1A=1단위, 범위 10~100)
  int encVal = encoder.getValue();
  if (encVal != lastEncoderValue) {
    int delta = encVal - lastEncoderValue;
    if (selectedIndex == 4) {
      int* f = config.getMaxCurrentPtr();
      if (f != nullptr) {
        *f += delta;  // 1클릭 = 0.1A
        if (*f < 10) *f = 10;
        if (*f > 100) *f = 100;
      }
    } else {
      int* f = config.getTimeField_x10(selectedIndex);
      if (f != nullptr) {
        *f += delta * 10;  // 1클릭 = 1초 = 10
        if (*f < 0) *f = 0;
      }
    }
    lastEncoderValue = encVal;
  }
  
  // 디스플레이
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.println("= Setup =");
  display.println("");
  for (int i = 0; i < MotorPositionConfig::FIELD_COUNT; i++) {
    if (i == selectedIndex) display.print(">");
    else display.print(" ");
    display.print(MotorPositionConfig::getFieldName(i));
    display.print(": ");
    if (i == 4) {
      display.print(config.getMaxCurrent(), 1);
      display.println("A");
    } else {
      int* tx = config.getTimeField_x10(i);
      if (tx != nullptr) {
        display.print((*tx) / 10.0f, 1);
        display.println("s");
      } else display.println();
    }
  }
  display.display();
}
