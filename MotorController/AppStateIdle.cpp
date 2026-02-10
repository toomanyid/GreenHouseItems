#include "AppStateIdle.h"
#include "AppState.h"
#include <stdio.h>
#include <string.h>
#include "AppStateSetup.h"
#include "AppStateRun.h"
#include "MotorDriver.h"
#include "MotorPositionConfig.h"
#include "ButtonInterrupt.h"
#include "RotaryEncoder.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

// 외부 참조 (MotorController.ino에서 정의)
extern Adafruit_SH1106G display;
extern RotaryEncoder encoder;
extern MotorDriver& motor;
extern AppStateContext appStateContext;
extern int lastEncoderValue;

AppStateIdle& AppStateIdle::getInstance() {
  static AppStateIdle instance;
  return instance;
}

#define IDLE_RUN_DELAY_MS 2000   // 2초 대기 후 Run 전환
#define IDLE_ENCODER_STEP_PCT 1.0f  // 엔코더 1틱당 1% 증감

void AppStateIdle::onEnter() {
  motor.stop();
  lastEncoderValue = encoder.getValue();
  lastEncoderChangeMs = millis();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
}

void AppStateIdle::onUpdate() {
  int currentValue = encoder.getValue();
  unsigned long now = millis();
  
  // 로터리 회전: 1%씩 증감, 2초 타이머 리셋 (목표는 초 단위로 유지)
  MotorPositionConfig& config = MotorPositionConfigManager::getInstance();
  float bottomSec = config.bottom;
  float readySec = config.ready;
  float topSec = config.top;
  float maxSec = config.max;
  if (currentValue != lastEncoderValue) {
    int delta = currentValue - lastEncoderValue;
    float& targetSec = config.target;
    float pct = MotorPositionConfig::positionToPercent(targetSec, bottomSec, readySec, maxSec);
    pct += delta * IDLE_ENCODER_STEP_PCT;
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    // % → 초 변환 (0%~1%: bottom~ready, 1%~100%: ready~max)
    if (pct <= 0) targetSec = bottomSec;
    else if (pct >= 100) targetSec = maxSec;
    else if (pct <= 1.0f) targetSec = bottomSec + pct * (readySec - bottomSec);
    else targetSec = readySec + (pct - 1.0f) / 99.0f * (maxSec - readySec);
    lastEncoderValue = currentValue;
    lastEncoderChangeMs = now;
  }
  
  // 2초 대기 후 Run 전환 (목표가 현재와 다를 때만)
  float target = config.target;
  if ((now - lastEncoderChangeMs) >= IDLE_RUN_DELAY_MS &&
      abs(target - motor.getPositionSec()) > 0.05f) {
    appStateContext.setState(&AppStateRun::getInstance());
    return;
  }
  
  // PUSH: Setup 상태로 전환
  if (ButtonInterrupt::isPushPressed()) {
    appStateContext.setState(&AppStateSetup::getInstance());
    return;
  }
  
  // UP/DOWN: 목표값만 변경 (로터리와 동일, top 이상 불가)
  float& targetRef = config.target;
  float positions[] = {config.bottom, config.ready, config.top, config.max};
  
  if (ButtonInterrupt::isCwJustPressed()) {
    float next = config.max + 1;
    for (int i = 0; i < 4; i++) {
      if (positions[i] > targetRef && positions[i] < next) next = positions[i];
    }
    if (next <= topSec) {
      targetRef = next;
      lastEncoderChangeMs = now;
    }
  } else if (ButtonInterrupt::isCcwJustPressed()) {
    float next = -1;
    for (int i = 0; i < 4; i++) {
      if (positions[i] < targetRef && positions[i] > next) next = positions[i];
    }
    if (next >= 0) {
      targetRef = next;
      lastEncoderChangeMs = now;
    }
  }
  
  // 표시용 % (ready=1% 고정, max=100%, ready 미만=0%), 소수점 1자리
  float posSec = motor.getPositionSec();
  float pctCurrent = MotorPositionConfig::positionToPercent(posSec, bottomSec, readySec, maxSec);
  float pctTarget  = MotorPositionConfig::positionToPercent(targetRef, bottomSec, readySec, maxSec);
  
  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);
  
  bool targetChanged = (now - lastEncoderChangeMs) < IDLE_RUN_DELAY_MS;
  float displayPct;
  if (targetChanged) {
    displayPct = pctTarget;
    display.setTextSize(1);
    display.setCursor(0, 0);
    char leftBuf[16];
    snprintf(leftBuf, sizeof(leftBuf), "%.1f->", pctCurrent);
    display.print(leftBuf);
  } else {
    displayPct = pctCurrent;
  }
  
  char buf[12];
  snprintf(buf, sizeof(buf), "%.1f", displayPct);
  int len = strlen(buf);
  int charW = 18;
  int totalW = len * charW;
  int x = (128 - totalW) / 2;
  display.setTextSize(3);
  display.setCursor(x, 20);
  display.print(buf);
  display.display();
}
