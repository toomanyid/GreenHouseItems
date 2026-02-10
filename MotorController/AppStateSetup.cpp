#include "AppStateSetup.h"
#include "AppState.h"
#include "AppStateIdle.h"
#include "MotorDriver.h"
#include "MotorPositionConfig.h"
#include "ButtonInterrupt.h"
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

void AppStateSetup::onUpdate() {
  // PUSH: 설정 저장 후 모터에 알리고 Idle로 복귀
  if (ButtonInterrupt::isPushPressed()) {
    MotorPositionConfigManager::save();
    motor.setPositionLimits(0, MotorPositionConfigManager::getInstance().top);
    appStateContext.setState(&AppStateIdle::getInstance());
    return;
  }
  
  MotorPositionConfig& config = MotorPositionConfigManager::getInstance();
  
  // CW/CCW: Up/Down - 항목 간 이동 (한 번만, 두 번씩 입력 방지)
  if (ButtonInterrupt::isCwJustPressed()) {
    selectedIndex--;
    if (selectedIndex < 0) selectedIndex = 0;
  }
  if (ButtonInterrupt::isCcwJustPressed()) {
    selectedIndex++;
    if (selectedIndex >= MotorPositionConfig::FIELD_COUNT) selectedIndex = MotorPositionConfig::FIELD_COUNT - 1;
  }
  
  // 로터리: 선택된 값 증감 (위치값 1초 단위, maxCurrent 0.1 단위 1~10)
  int encVal = encoder.getValue();
  if (encVal != lastEncoderValue) {
    int delta = encVal - lastEncoderValue;
    float* field = config.getField(selectedIndex);
    if (field != nullptr) {
      if (selectedIndex == 4) {  // maxCurrent: 0.1 단위, 범위 1~10
        *field += delta * 0.1f;
        if (*field < 1.0f) *field = 1.0f;
        if (*field > 10.0f) *field = 10.0f;
      } else {
        *field += delta * 1.0f;
        if (*field < 0) *field = 0;
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
    float* f = config.getField(i);
    if (f != nullptr) {
      display.print(*f, 1);
      display.println((i == 4) ? "A" : "s");
    } else display.println();
  }
  display.display();
}
