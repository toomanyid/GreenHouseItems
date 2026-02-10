#ifndef BUTTON_INTERRUPT_H
#define BUTTON_INTERRUPT_H

#include <Arduino.h>

// 버튼 인터럽트 핸들러 (PUSH, UP, DOWN 통합)
class ButtonInterrupt {
private:
  static int pushPin;
  static int cwPin;
  static int ccwPin;
  
  static volatile bool pushState;
  static volatile bool cwState;
  static volatile bool ccwState;
  static volatile bool pushPrevState;
  static volatile bool cwPrevState;
  static volatile bool ccwPrevState;
  
  static volatile unsigned long lastInterruptTime;
  static const unsigned long DEBOUNCE_DELAY_MS = 50;
  
  static void IRAM_ATTR isrHandler();
  
  ButtonInterrupt() = delete;
  ButtonInterrupt(const ButtonInterrupt&) = delete;
  void operator=(const ButtonInterrupt&) = delete;

public:
  static void init(int push, int cw, int ccw);
  static void update();  // 메인 루프에서 호출, prev_state 업데이트
  
  static bool isPushPressed();
  static bool isCwPressed();
  static bool isCcwPressed();
  static bool isPushJustPressed();
  static bool isCwJustPressed();
  static bool isCcwJustPressed();
};

#endif
