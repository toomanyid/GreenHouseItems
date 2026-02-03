#include "AppStateRun.h"
#include "AppState.h"
#include "AppStateIdle.h"
#include "MotorDriver.h"
#include "MotorPositionConfig.h"
#include "Button.h"
#include "RotaryEncoder.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <stdio.h>
#include <string.h>

extern Adafruit_SH1106G display;
extern RotaryEncoder encoder;
extern MotorDriver& motor;
extern Button button_push;
extern Button button_cw;
extern Button button_ccw;
extern AppStateContext appStateContext;

AppStateRun& AppStateRun::getInstance() {
  static AppStateRun instance;
  return instance;
}

AppStateRun::AppStateRun()
  : targetPositionSec(0), lastEncoderValue(0) {
}

void AppStateRun::onEnter() {
  targetPositionSec = MotorPositionConfigManager::getInstance().target;
  lastEncoderValue = encoder.getValue();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
}

void AppStateRun::onUpdate() {
  button_push.update();
  button_cw.update();
  button_ccw.update();
  
  unsigned long now = millis();
  float posSec = motor.getPositionSec();
  MotorPositionConfig& config = MotorPositionConfigManager::getInstance();
  float readySec = config.ready;
  float topSec = config.top;
  
  // PUSH: Idle로 복귀 (취소)
  if (button_push.isPressed()) {
    motor.stop();
    appStateContext.setState(&AppStateIdle::getInstance());
    return;
  }
  
  // 로터리 회전: 모터 정지 → Idle로 복귀
  int encVal = encoder.getValue();
  if (encVal != lastEncoderValue) {
    motor.stop();
    appStateContext.setState(&AppStateIdle::getInstance());
    return;
  }
  
  // UP/DOWN: 로터리와 동일하게 모터 정지 → Idle로 복귀
  if (button_cw.justPressed() || button_ccw.justPressed()) {
    motor.stop();
    appStateContext.setState(&AppStateIdle::getInstance());
    return;
  }
  
  // 모터 구동
  {
    // 목표 도달 체크
    float diff = targetPositionSec - posSec;
    if (abs(diff) < RUN_ARRIVAL_THRESHOLD) {
      motor.stop();
      appStateContext.setState(&AppStateIdle::getInstance());
      return;
    }
    // 모터 구동 (CW/CCW)
    if (diff > 0) motor.runCW(100);
    else motor.runCCW(100);  // 내려갈 때도 올라갈 때와 동일 속도
  }
  
  // 표시용 % (ready=1%, top=100%, ready 미만=0%)
  float pctTarget = MotorPositionConfig::positionToPercent(targetPositionSec, readySec, topSec);
  float pctCurrent = MotorPositionConfig::positionToPercent(posSec, readySec, topSec);
  
  // 디스플레이: 왼쪽위 "->목표%", 중앙 현재%
  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);
  
  display.setTextSize(1);
  display.setCursor(0, 0);
  char leftBuf[16];
  snprintf(leftBuf, sizeof(leftBuf), "->%.1f", pctTarget);
  display.print(leftBuf);
  
  char buf[12];
  snprintf(buf, sizeof(buf), "%.1f", pctCurrent);
  int len = strlen(buf);
  int charW = 18;
  int totalW = len * charW;
  int x = (128 - totalW) / 2;
  display.setTextSize(3);
  display.setCursor(x, 20);
  display.print(buf);
  display.display();
}
