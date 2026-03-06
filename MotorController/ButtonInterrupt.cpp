#include "ButtonInterrupt.h"
#include "KeyQueue.h"

int ButtonInterrupt::pushPin = 0;
int ButtonInterrupt::cwPin = 0;
int ButtonInterrupt::ccwPin = 0;
volatile bool ButtonInterrupt::pushPrev = false;
volatile bool ButtonInterrupt::cwPrev = false;
volatile bool ButtonInterrupt::ccwPrev = false;

// 통합 핸들러: CHANGE 시 핀 읽어서 FALLING(눌림)일 때만 KeyQueue push
void IRAM_ATTR ButtonInterrupt::isrHandler() {
  bool pushNow = (digitalRead(pushPin) == LOW);
  bool cwNow = (digitalRead(cwPin) == LOW);
  bool ccwNow = (digitalRead(ccwPin) == LOW);

  if (pushNow && !pushPrev) KeyQueue::push(Key::Push);
  if (cwNow && !cwPrev) KeyQueue::push(Key::Cw);
  if (ccwNow && !ccwPrev) KeyQueue::push(Key::Ccw);

  pushPrev = pushNow;
  cwPrev = cwNow;
  ccwPrev = ccwNow;
}

void ButtonInterrupt::init(int push, int cw, int ccw) {
  pushPin = push;
  cwPin = cw;
  ccwPin = ccw;

  pinMode(pushPin, INPUT_PULLUP);
  pinMode(cwPin, INPUT_PULLUP);
  pinMode(ccwPin, INPUT_PULLUP);

  pushPrev = (digitalRead(pushPin) == LOW);
  cwPrev = (digitalRead(cwPin) == LOW);
  ccwPrev = (digitalRead(ccwPin) == LOW);

  attachInterrupt(digitalPinToInterrupt(pushPin), &ButtonInterrupt::isrHandler, CHANGE);
  attachInterrupt(digitalPinToInterrupt(cwPin), &ButtonInterrupt::isrHandler, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ccwPin), &ButtonInterrupt::isrHandler, CHANGE);
}
