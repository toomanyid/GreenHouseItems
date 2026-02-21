#include "ButtonInterrupt.h"
#include "KeyQueue.h"

int ButtonInterrupt::pushPin = 0;
int ButtonInterrupt::cwPin = 0;
int ButtonInterrupt::ccwPin = 0;

void IRAM_ATTR ButtonInterrupt::isrHandler() {
  // LOW = 눌림, 눌린 키만 큐에 push (CHANGE 시 한 번만 들어감)
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

  attachInterrupt(digitalPinToInterrupt(pushPin), isrHandler, CHANGE);
  attachInterrupt(digitalPinToInterrupt(cwPin), isrHandler, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ccwPin), isrHandler, CHANGE);
}
