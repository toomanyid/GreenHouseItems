#include "AppStateRun.h"
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
  float maxSec = MotorPositionConfigManager::getInstance().max;
  
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
    else motor.runCCW(50);
  }
  
  // 디스플레이
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("= Run  =");
  display.println("");
  display.print("State: ");
  display.println(getName());
  display.print("Target: ");
  display.print(targetPositionSec, 1);
  display.println("s");
  display.print("Pos: ");
  display.print(posSec, 1);
  display.println("s");
  display.println("MOTOR RUN");
  display.display();
}
