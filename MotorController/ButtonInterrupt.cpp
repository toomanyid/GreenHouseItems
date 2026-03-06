#include "ButtonInterrupt.h"
#include "KeyQueue.h"

int ButtonInterrupt::pushPin = 0;
int ButtonInterrupt::cwPin = 0;
int ButtonInterrupt::ccwPin = 0;

// FALLING에서만 호출됨 → 핀 읽어서 LOW인 키만 push (어느 핀에서 떨어졌는지 모르므로 세 핀 모두 확인)
void IRAM_ATTR ButtonInterrupt::isrHandler() {
  if (digitalRead(pushPin) == LOW) KeyQueue::push(Key::Push);
  if (digitalRead(cwPin) == LOW) KeyQueue::push(Key::Cw);
  if (digitalRead(ccwPin) == LOW) KeyQueue::push(Key::Ccw);
}

void ButtonInterrupt::init(int push, int cw, int ccw) {
  pushPin = push;
  cwPin = cw;
  ccwPin = ccw;

  pinMode(pushPin, INPUT_PULLUP);
  pinMode(cwPin, INPUT_PULLUP);
  pinMode(ccwPin, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(pushPin), &ButtonInterrupt::isrHandler, FALLING);
  attachInterrupt(digitalPinToInterrupt(cwPin), &ButtonInterrupt::isrHandler, FALLING);
  attachInterrupt(digitalPinToInterrupt(ccwPin), &ButtonInterrupt::isrHandler, FALLING);
}
