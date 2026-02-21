#ifndef KEY_QUEUE_H
#define KEY_QUEUE_H

#include <Arduino.h>

// 키 입력 종류 (입력 확인 시 큐에 저장)
enum class Key : uint8_t {
  None = 0,
  Push,
  Cw,
  Ccw
};

// 인터럽트에서 push, 메인 루프에서 pop하여 현재 상태에 onKey(key) 전달
// push()는 ISR에서 호출 가능 (논블로킹)
class KeyQueue {
public:
  static const int CAPACITY = 16;

  static void IRAM_ATTR push(Key k);  // 인터럽트에서 직접 호출
  static Key pop();         // 큐 비어있으면 Key::None (메인 루프에서만 호출)
  static bool isEmpty();

private:
  static volatile Key queue[CAPACITY];
  static volatile int head;
  static volatile int tail;
  static volatile int count;

  KeyQueue() = delete;
};

#endif
