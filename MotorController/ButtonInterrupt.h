#ifndef BUTTON_INTERRUPT_H
#define BUTTON_INTERRUPT_H

#include <Arduino.h>
#include "KeyQueue.h"

// 버튼 인터럽트: 하나의 핸들러로 통합, 핀 읽어서 눌림(FALLING)일 때만 KeyQueue에 push
class ButtonInterrupt {
private:
  static int pushPin;
  static int cwPin;
  static int ccwPin;
  static volatile bool pushPrev;  // 이전 상태 (true=LOW), 엣지 검출용
  static volatile bool cwPrev;
  static volatile bool ccwPrev;

  ButtonInterrupt() = delete;
  ButtonInterrupt(const ButtonInterrupt&) = delete;
  void operator=(const ButtonInterrupt&) = delete;

public:
  static void init(int push, int cw, int ccw);
  static void IRAM_ATTR isrHandler();
};

#endif
