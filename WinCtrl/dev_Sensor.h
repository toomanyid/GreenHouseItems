#pragma once
#include <stdint.h>
#include "Types.h"
#include "ctl_Safety.h"

// ============================================================
//  1층: 원점 센서 on/off 입력 (포토커플러 경유, ON이면 LOW).
//  판정 규칙은 2층의 SensorFilter에 있다.
// ============================================================
class HomeSensor {
public:
    void begin(uint8_t pin, uint32_t now);
    SensorState update(uint32_t now);               // 10ms마다
    SensorState value() const { return _f.value(); }
private:
    uint8_t      _pin = 0;
    SensorFilter _f;
};
