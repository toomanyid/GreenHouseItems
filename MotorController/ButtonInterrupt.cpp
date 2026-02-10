#include "ButtonInterrupt.h"

// 정적 멤버 변수 정의
int ButtonInterrupt::pushPin = 0;
int ButtonInterrupt::cwPin = 0;
int ButtonInterrupt::ccwPin = 0;

volatile bool ButtonInterrupt::pushState = false;
volatile bool ButtonInterrupt::cwState = false;
volatile bool ButtonInterrupt::ccwState = false;
volatile bool ButtonInterrupt::pushPrevState = false;
volatile bool ButtonInterrupt::cwPrevState = false;
volatile bool ButtonInterrupt::ccwPrevState = false;

volatile unsigned long ButtonInterrupt::lastInterruptTime = 0;

void IRAM_ATTR ButtonInterrupt::isrHandler() {
  // 디바운싱 비활성화 (사용자 요청)
  // unsigned long now = millis();
  // if (now - lastInterruptTime < DEBOUNCE_DELAY_MS) {
  //   return;
  // }
  // lastInterruptTime = now;
  
  // 모든 버튼 상태 읽기 (LOW = 눌림)
  pushState = (digitalRead(pushPin) == LOW);
  cwState = (digitalRead(cwPin) == LOW);
  ccwState = (digitalRead(ccwPin) == LOW);
}

void ButtonInterrupt::init(int push, int cw, int ccw) {
  pushPin = push;
  cwPin = cw;
  ccwPin = ccw;
  
  pinMode(pushPin, INPUT_PULLUP);
  pinMode(cwPin, INPUT_PULLUP);
  pinMode(ccwPin, INPUT_PULLUP);
  
  // 초기 상태 읽기
  pushState = (digitalRead(pushPin) == LOW);
  cwState = (digitalRead(cwPin) == LOW);
  ccwState = (digitalRead(ccwPin) == LOW);
  pushPrevState = pushState;
  cwPrevState = cwState;
  ccwPrevState = ccwState;
  
  // 모든 핀에 동일한 인터럽트 핸들러 연결 (FALLING: HIGH->LOW, RISING: LOW->HIGH)
  attachInterrupt(digitalPinToInterrupt(pushPin), isrHandler, CHANGE);
  attachInterrupt(digitalPinToInterrupt(cwPin), isrHandler, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ccwPin), isrHandler, CHANGE);
}

void ButtonInterrupt::update() {
  // prev_state 업데이트 (justPressed()용)
  pushPrevState = pushState;
  cwPrevState = cwState;
  ccwPrevState = ccwState;
}

bool ButtonInterrupt::isPushPressed() {
  return pushState;
}

bool ButtonInterrupt::isCwPressed() {
  return cwState;
}

bool ButtonInterrupt::isCcwPressed() {
  return ccwState;
}

bool ButtonInterrupt::isPushJustPressed() {
  return pushState && !pushPrevState;
}

bool ButtonInterrupt::isCwJustPressed() {
  return cwState && !cwPrevState;
}

bool ButtonInterrupt::isCcwJustPressed() {
  return ccwState && !ccwPrevState;
}
