#pragma once
#include <stdint.h>
#include "Types.h"
#include "ctl_Safety.h"

// ============================================================
//  1층: 원점 센서 (리밋 스위치 NO, NC 접점). 닫히면 LOW.
//  판정 규칙은 2층의 SensorFilter에 있다.
// ============================================================
class HomeSensor {
public:
    void begin(uint8_t pinNo, uint8_t pinNc, uint32_t now);
    SensorState update(uint32_t now);               // 10ms마다
    SensorState value() const     { return _f.value(); }
    bool contactsValid() const    { return _f.contactsValid(); }
private:
    uint8_t      _no = 0, _nc = 0;
    SensorFilter _f;
};
