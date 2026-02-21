#include "ButtonInterrupt.h"
#include "KeyQueue.h"

int ButtonInterrupt::pushPin = 0;
int ButtonInterrupt::cwPin = 0;
int ButtonInterrupt::ccwPin = 0;

// 눌릴 때만 호출되도록 FALLING (HIGH→LOW), 뗄 때는 인터럽트 없음
static void IRAM_ATTR isrPush()  { KeyQueue::push(Key::Push); }
static void IRAM_ATTR isrCw()   { KeyQueue::push(Key::Cw); }
static void IRAM_ATTR isrCcw()  { KeyQueue::push(Key::Ccw); }

void ButtonInterrupt::init(int push, int cw, int ccw) {
  pushPin = push;
  cwPin = cw;
  ccwPin = ccw;

  pinMode(pushPin, INPUT_PULLUP);
  pinMode(cwPin, INPUT_PULLUP);
  pinMode(ccwPin, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(pushPin), isrPush, FALLING);
  attachInterrupt(digitalPinToInterrupt(cwPin), isrCw, FALLING);
  attachInterrupt(digitalPinToInterrupt(ccwPin), isrCcw, FALLING);
}
