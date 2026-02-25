#ifndef BUTTON_INTERRUPT_H
#define BUTTON_INTERRUPT_H

#include <Arduino.h>
#include "KeyQueue.h"

// 버튼 인터럽트: 눌리면 KeyQueue에 push.
// - 디바운스: DEBOUNCE_MS 이내 중복 무시
// - 길게 눌렀다 놓을 때: 놓은 뒤 MIN_RELEASE_MS 이상 지나야 다음 눌림 인정 (놓을 때 바운스로 한 번 더 입력되는 것 방지)
class ButtonInterrupt {
private:
  static int pushPin;
  static int cwPin;
  static int ccwPin;
  static volatile unsigned long currentMillis;   // update()에서 갱신
  static volatile unsigned long lastPushTime;    // 마지막으로 큐에 넣은 시각
  static volatile unsigned long lastReleaseTime; // 마지막으로 핀이 HIGH(놓음)가 된 시각
  static const unsigned long DEBOUNCE_MS = 50;
  static const unsigned long MIN_RELEASE_MS = 30;  // 놓은 뒤 이 시간 지나야 다음 눌림 인정
  static void tryPush(Key k);  // 디바운스 + "놓음 후 경과" 통과 시에만 push

  ButtonInterrupt() = delete;
  ButtonInterrupt(const ButtonInterrupt&) = delete;
  void operator=(const ButtonInterrupt&) = delete;

public:
  static void init(int push, int cw, int ccw);
  static void update();  // 메인 루프에서 호출, currentMillis 갱신
  static void IRAM_ATTR isrPush();
  static void IRAM_ATTR isrCw();
  static void IRAM_ATTR isrCcw();
};

#endif
