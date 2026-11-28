#pragma once
#include <stdint.h>
#include "Clock.h"

// ============================================================
//  1층: RTC DS3231 (I2C 0x68, LCD와 같은 선, 2026-11-28).
//  Wire는 LCD가 begin()한 것을 같이 쓴다. 화면 태스크만 부른다.
//  현지 시각을 24시간제로 넣는다. 배터리가 빠져 시계가 멈췄던 적이 있으면(OSF) 시각을 믿지 않는다.
// ============================================================
class Rtc {
public:
    static const uint8_t ADDR = 0x68;
    bool begin();                         // 장치가 있는지 본다
    bool present() const { return _present; }
    bool read(DateTime& t);               // 없거나 시계가 멈췄던 적이 있으면 false
    bool write(const DateTime& t);        // 초까지 넣고 멈춤 표시를 지운다
    bool stopped() const { return _stopped; }   // 마지막 읽기에서 시계가 멈췄던 적이 있었음
private:
    bool readRegs(uint8_t reg, uint8_t* buf, uint8_t n);
    bool writeReg(uint8_t reg, const uint8_t* buf, uint8_t n);
    bool _present = false;
    bool _stopped = false;
};
