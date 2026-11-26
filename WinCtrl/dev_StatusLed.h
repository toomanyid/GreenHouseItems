#pragma once
#include <stdint.h>

// ============================================================
//  1층: 상태 LED 하나 (GPIO → 저항 → LED → GND, HIGH면 켜짐).
//  켜고 끄기만 한다. 깜빡임 방식은 3층(Dispatcher::statusLedOn)이 정한다.
// ============================================================
class StatusLed {
public:
    void begin(uint8_t pin);
    void set(bool on);              // 바뀔 때만 핀을 쓴다
private:
    uint8_t _pin = 0;
    bool    _on = false;
    bool    _ready = false;
};
