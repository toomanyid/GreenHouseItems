#ifndef BUTTON_INTERRUPT_H
#define BUTTON_INTERRUPT_H

#include <Arduino.h>
#include "KeyQueue.h"

// 버튼 인터럽트: FALLING에서만 핸들러 호출, 핀 읽어서 LOW인 키만 KeyQueue에 push
class ButtonInterrupt {
private:
  static int pushPin;
  static int cwPin;
  static int ccwPin;

  ButtonInterrupt() = delete;
  ButtonInterrupt(const ButtonInterrupt&) = delete;
  void operator=(const ButtonInterrupt&) = delete;

public:
  static void init(int push, int cw, int ccw);
  static void IRAM_ATTR isrHandler();
};

#endif
