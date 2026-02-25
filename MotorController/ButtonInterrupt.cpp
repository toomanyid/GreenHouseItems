#include "ButtonInterrupt.h"
#include "KeyQueue.h"

int ButtonInterrupt::pushPin = 0;
int ButtonInterrupt::cwPin = 0;
int ButtonInterrupt::ccwPin = 0;
volatile unsigned long ButtonInterrupt::currentMillis = 0;
volatile unsigned long ButtonInterrupt::lastPushTime = 0;
volatile unsigned long ButtonInterrupt::lastReleaseTime = 0;

// 디바운스 통과 + (놓은 지 MIN_RELEASE_MS 이상) 일 때만 push → 길게 눌렀다 놓을 때 추가 입력 방지
void ButtonInterrupt::tryPush(Key k) {
  unsigned long t = currentMillis;
  unsigned long lastPush = lastPushTime;
  unsigned long lastRelease = lastReleaseTime;
  bool debounceOk = (lastPush == 0 || (t - lastPush) >= DEBOUNCE_MS);
  bool releasedLongEnough = (lastRelease == 0 || (t - lastRelease) >= MIN_RELEASE_MS);
  if (debounceOk && releasedLongEnough) {
    KeyQueue::push(k);
    lastPushTime = t;
    // ISR에서 Serial 사용 시 스택 오버플로우/패닉 유발 → 로그는 메인 루프에서만
  }
}

// CHANGE: 눌림(LOW)일 때만 tryPush, 놓음(HIGH)일 때 lastReleaseTime 갱신
void IRAM_ATTR ButtonInterrupt::isrPush() {
  noInterrupts();
  if (digitalRead(pushPin) == LOW)
    tryPush(Key::Push);
  else
    lastReleaseTime = currentMillis;
  interrupts();
}
void IRAM_ATTR ButtonInterrupt::isrCw() {
  noInterrupts();
  if (digitalRead(cwPin) == LOW)
    tryPush(Key::Cw);
  else
    lastReleaseTime = currentMillis;
  interrupts();
}
void IRAM_ATTR ButtonInterrupt::isrCcw() {
  noInterrupts();
  if (digitalRead(ccwPin) == LOW)
    tryPush(Key::Ccw);
  else
    lastReleaseTime = currentMillis;
  interrupts();
}

void ButtonInterrupt::init(int push, int cw, int ccw) {
  pushPin = push;
  cwPin = cw;
  ccwPin = ccw;

  pinMode(pushPin, INPUT_PULLUP);
  pinMode(cwPin, INPUT_PULLUP);
  pinMode(ccwPin, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(pushPin), &ButtonInterrupt::isrPush, CHANGE);
  attachInterrupt(digitalPinToInterrupt(cwPin), &ButtonInterrupt::isrCw, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ccwPin), &ButtonInterrupt::isrCcw, CHANGE);
}

void ButtonInterrupt::update() {
  currentMillis = millis();
}
