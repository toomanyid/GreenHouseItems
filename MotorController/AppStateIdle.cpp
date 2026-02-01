#include "AppStateIdle.h"
#include "AppState.h"
#include <stdio.h>
#include <string.h>
#include "AppStateSetup.h"
#include "AppStateRun.h"
#include "MotorDriver.h"
#include "MotorPositionConfig.h"
#include "Button.h"
#include "RotaryEncoder.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

// 외부 참조 (MotorController.ino에서 정의)
extern Adafruit_SH1106G display;
extern RotaryEncoder encoder;
extern MotorDriver& motor;
extern Button button_push;
extern Button button_cw;
extern Button button_ccw;
extern AppStateContext appStateContext;
extern int lastEncoderValue;

AppStateIdle& AppStateIdle::getInstance() {
  static AppStateIdle instance;
  return instance;
}

#define IDLE_RUN_DELAY_MS 2000   // 2초 대기 후 Run 전환
#define IDLE_ENCODER_STEP 0.1f   // 엔코더 1클릭당 목표 변경 (초)

void AppStateIdle::onEnter() {
  motor.stop();
  lastEncoderValue = encoder.getValue();
  lastEncoderChangeMs = millis();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
}

void AppStateIdle::onUpdate() {
  // 버튼 읽기
  button_push.update();
  button_cw.update();
  button_ccw.update();
  
  int currentValue = encoder.getValue();
  unsigned long now = millis();
  
  // 로터리 회전: 목표값 변경, 2초 타이머 리셋
  if (currentValue != lastEncoderValue) {
    int delta = currentValue - lastEncoderValue;
    float& target = MotorPositionConfigManager::getInstance().target;
    target += delta * IDLE_ENCODER_STEP;
    float maxSec = MotorPositionConfigManager::getInstance().max;
    if (target < 0) target = 0;
    if (target > maxSec) target = maxSec;
    lastEncoderValue = currentValue;
    lastEncoderChangeMs = now;
  }
  
  // 2초 대기 후 Run 전환 (목표가 현재와 다를 때만)
  float target = MotorPositionConfigManager::getInstance().target;
  if ((now - lastEncoderChangeMs) >= IDLE_RUN_DELAY_MS &&
      abs(target - motor.getPositionSec()) > 0.05f) {
    appStateContext.setState(&AppStateRun::getInstance());
    return;
  }
  
  // PUSH: Setup 상태로 전환
  if (button_push.isPressed()) {
    appStateContext.setState(&AppStateSetup::getInstance());
    return;
  }
  
  // 버튼에 따른 모터 제어 (0~max 범위 제한)
  float posSec = motor.getPositionSec();
  float maxSec = MotorPositionConfigManager::getInstance().max;
  if (button_cw.isPressed()) {
    if (posSec < maxSec) motor.runCW(100);
    else motor.stop();
  } else if (button_ccw.isPressed()) {
    if (posSec > 0) motor.runCCW(50);
    else motor.stop();
  } else {
    motor.stop();
  }
  
  
  // 디스플레이: 로터리 회전 시 목표값, 아니면 현재위치 (숫자만)
  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);
  
  float val;  // 표시할 값 (초)
  if ((now - lastEncoderChangeMs) < IDLE_RUN_DELAY_MS) {
    val = MotorPositionConfigManager::getInstance().target;  // 로터리 회전 시 목표값
  } else {
    val = motor.getPositionSec();  // 현재위치
  }
  
  char buf[12];
  snprintf(buf, sizeof(buf), "%.1f", val);
  int len = strlen(buf);
  int charW = 18;  // size 3
  int totalW = len * charW;
  int x = (128 - totalW) / 2;
  display.setTextSize(3);
  display.setCursor(x, 20);
  display.print(buf);
  display.display();
}
