#ifndef BUTTON_H
#define BUTTON_H

#include <Arduino.h>

// 단일 버튼 클래스 (일반 푸시버튼, 디바운싱)
// 누르고 있을 때 true, 손을 떼면 false
class Button {
private:
  int pin;
  const char* name;
  
  bool state;                   // 디바운싱된 현재 상태 (눌림=LOW=true)
  bool prev_state;              // 이전 상태 (엣지 감지용)
  int last_reading;
  unsigned long last_debounce_time;
  unsigned long debounce_delay;

public:
  Button(int pin, const char* name = "Button");
  
  void init();                    // 핀 설정 및 초기화
  void update();                  // 버튼 읽기 및 디바운싱 (loop에서 호출)
  
  bool isPressed();              // 현재 눌림 상태 (누르고 있으면 true)
  bool justPressed();            // 눌림 시작 순간에만 true (한 번만 트리거)
  
  void setDebounceDelay(unsigned long ms);  // 디바운스 시간 설정
  const char* getName();         // 버튼 이름 반환
};

#endif
