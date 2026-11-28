#include <Arduino.h>
#include <Wire.h>
#include "dev_Rtc.h"

static uint8_t bcd(uint8_t v)   { return (uint8_t)(((v / 10) << 4) | (v % 10)); }
static uint8_t unbcd(uint8_t v) { return (uint8_t)((v >> 4) * 10 + (v & 0x0F)); }

bool Rtc::begin() {
    Wire.beginTransmission(ADDR);
    _present = Wire.endTransmission() == 0;
    return _present;
}

bool Rtc::readRegs(uint8_t reg, uint8_t* buf, uint8_t n) {
    Wire.beginTransmission(ADDR);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom((uint8_t)ADDR, n) != n) return false;
    for (uint8_t i = 0; i < n; i++) buf[i] = (uint8_t)Wire.read();
    return true;
}

bool Rtc::writeReg(uint8_t reg, const uint8_t* buf, uint8_t n) {
    Wire.beginTransmission(ADDR);
    Wire.write(reg);
    for (uint8_t i = 0; i < n; i++) Wire.write(buf[i]);
    return Wire.endTransmission() == 0;
}

bool Rtc::read(DateTime& t) {
    uint8_t r[7], st;
    if (!readRegs(0x00, r, 7) || !readRegs(0x0F, &st, 1)) { _present = false; return false; }
    _present = true;
    _stopped = (st & 0x80) != 0;                 // OSF: 시계가 멈췄던 적이 있음
    t.second = unbcd(r[0] & 0x7F);
    t.minute = unbcd(r[1] & 0x7F);
    t.hour   = (r[2] & 0x40) ? (uint8_t)(unbcd(r[2] & 0x1F) % 12 + ((r[2] & 0x20) ? 12 : 0))   // 12시간제로 들어 있으면
                             : unbcd(r[2] & 0x3F);
    t.day    = unbcd(r[4] & 0x3F);
    t.month  = unbcd(r[5] & 0x1F);
    t.year   = (uint16_t)(2000 + unbcd(r[6]));
    return !_stopped && Clock::valid(t);
}

bool Rtc::write(const DateTime& t) {
    if (!Clock::valid(t)) return false;
    // 요일(1~7): 2000-01-01은 토요일
    uint8_t dow = (uint8_t)((Clock::toSeconds(t) / 86400 + 5) % 7 + 1);
    uint8_t r[7] = { bcd(t.second), bcd(t.minute), bcd(t.hour), dow, bcd(t.day), bcd(t.month),
                     bcd((uint8_t)(t.year - 2000)) };
    if (!writeReg(0x00, r, 7)) { _present = false; return false; }
    uint8_t st;
    if (readRegs(0x0F, &st, 1)) { st &= (uint8_t)~0x80; writeReg(0x0F, &st, 1); }   // 멈춤 표시를 지운다
    _present = true;
    _stopped = false;
    return true;
}
