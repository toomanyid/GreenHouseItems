#include "AppStateSetup.h"
#include "AppState.h"
#include "AppStateIdle.h"
#include "MotorDriver.h"
#include "MotorPositionConfig.h"
#include "Button.h"
#include "RotaryEncoder.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

extern Adafruit_SH1106G display;
extern RotaryEncoder encoder;
extern MotorDriver& motor;
extern Button button_push;
extern Button button_cw;
extern Button button_ccw;
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

void AppStateSetup::onUpdate() {
  button_push.update();
  button_cw.update();
  button_ccw.update();
  
  // PUSH: 설정 저장 후 모터에 알리고 Idle로 복귀
  if (button_push.isPressed()) {
    MotorPositionConfigManager::save();
    motor.setPositionLimits(0, MotorPositionConfigManager::getInstance().max);
    appStateContext.setState(&AppStateIdle::getInstance());
    return;
  }
  
  MotorPositionConfig& config = MotorPositionConfigManager::getInstance();
  
  // 엔코더: 항목 선택
  int encVal = encoder.getValue();
  if (encVal != lastEncoderValue) {
    int delta = encVal - lastEncoderValue;
    selectedIndex += delta;
    if (selectedIndex < 0) selectedIndex = 0;
    if (selectedIndex >= MotorPositionConfig::FIELD_COUNT) selectedIndex = MotorPositionConfig::FIELD_COUNT - 1;
    lastEncoderValue = encVal;
  }
  
  // CW/CCW: 선택된 값 증감 (0.1 단위)
  float step = 0.1f;
  if (button_cw.isPressed()) {
    float* field = config.getField(selectedIndex);
    if (field != nullptr) *field += step;
  }
  if (button_ccw.isPressed()) {
    float* field = config.getField(selectedIndex);
    if (field != nullptr) {
      *field -= step;
      if (*field < 0) *field = 0;
    }
  }
  
  // 디스플레이
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.println("= Setup  =");
  display.println("");
  display.print("State: ");
  display.println(getName());
  display.print("Pos: ");
  display.print(motor.getPositionSec(), 1);
  display.println("s");
  display.println("");
  for (int i = 0; i < MotorPositionConfig::FIELD_COUNT; i++) {
    if (i == selectedIndex) display.print(">");
    else display.print(" ");
    display.print(MotorPositionConfig::getFieldName(i));
    display.print(": ");
    float* f = config.getField(i);
    if (f != nullptr) display.print(*f, 1);
    display.println("s");
  }
  display.display();
}
