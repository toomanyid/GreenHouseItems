#pragma once
#include <stdint.h>

// ============================================================
//  1층: 4~20mA 온도 센서. 감지 저항 150Ω의 전압을 읽어 µA로 올린다.
//  온도로 바꾸는 것은 3층 TempControl::toTemp에서 한다.
// ============================================================
class TempSensor {
public:
    void    begin(uint8_t pin);
    int32_t readUa();            // 여러 번 읽은 평균 (µA)
private:
    uint8_t _pin = 0;
    int32_t _avg = 0;
    bool    _first = true;
};
