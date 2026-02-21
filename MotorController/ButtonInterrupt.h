#ifndef BUTTON_INTERRUPT_H
#define BUTTON_INTERRUPT_H

#include <Arduino.h>

// 버튼 인터럽트: 눌림 감지 시 KeyQueue에 직접 push, 메인 루프에서 pop → onKey(key)
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
};

#endif
