#pragma once
#include <stdint.h>

// ============================================================
//  1층: 수동형 피에조. tone(주파수, 길이)을 걸면 바로 돌아오고,
//  끝은 일회성 타이머가 끊는다. 소리를 내는 동안 기다리지 않는다.
// ============================================================
class Buzzer {
public:
    void begin(uint8_t pin);
    void tone(uint16_t hz, uint16_t ms);
    void off();
    bool busy() const { return _busy; }
    void onTimer();                 // 타이머 콜백에서만 부름
private:
    uint8_t _pin = 0;
    void*   _timer = nullptr;       // esp_timer_handle_t
    volatile bool _busy = false;
};
