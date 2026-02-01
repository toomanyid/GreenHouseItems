#include "Button.h"

Button::Button(int pin, const char* name)
  : pin(pin), name(name),
    state(false), prev_state(false), last_reading(HIGH),
    last_debounce_time(0), debounce_delay(50) {
}

void Button::init() {
  pinMode(pin, INPUT_PULLUP);
  last_reading = digitalRead(pin);
  state = (last_reading == LOW);  // 풀업: LOW = 눌림
  prev_state = state;
}

void Button::update() {
  unsigned long current_time = millis();
  int reading = digitalRead(pin);
  
  // 상태 변화 감지 시 디바운스 타이머 리셋
  if (reading != last_reading) {
    last_debounce_time = current_time;
  }
  last_reading = reading;
  
  // 디바운스 시간 경과 후: 안정화된 값을 state에 반영 (풀업: LOW=눌림=true)
  prev_state = state;
  if ((current_time - last_debounce_time) > debounce_delay) {
    state = (reading == LOW);
  }
}

bool Button::isPressed() {
  return state;  // 누르고 있으면 true, 손 떼면 false
}

bool Button::justPressed() {
  return state && !prev_state;  // 눌림 시작 순간에만 true
}

void Button::setDebounceDelay(unsigned long ms) {
  debounce_delay = ms;
}

const char* Button::getName() {
  return name;
}
