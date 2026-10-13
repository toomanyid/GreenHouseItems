#include <Arduino.h>
#include <Wire.h>
#include "dev_Lcd.h"

// PCF8574 핀: P0 RS, P1 RW, P2 EN, P3 백라이트, P4~P7 D4~D7
static const uint8_t BIT_RS = 0x01;
static const uint8_t BIT_EN = 0x04;
static const uint8_t BIT_BL = 0x08;

bool Lcd::probe(uint8_t addr) {
    Wire.beginTransmission(addr);
    return Wire.endTransmission() == 0;
}

bool Lcd::expander(uint8_t v) {
    Wire.beginTransmission(_addr);
    Wire.write(v | (_bl ? BIT_BL : 0));
    if (Wire.endTransmission() != 0) { _txErr = true; return false; }
    return true;
}

void Lcd::write4(uint8_t nibble, bool rs) {
    uint8_t v = (uint8_t)((nibble & 0x0F) << 4) | (rs ? BIT_RS : 0);
    expander(v | BIT_EN);
    delayMicroseconds(1);
    expander(v);
    delayMicroseconds(40);
}

void Lcd::send(uint8_t value, bool rs) {
    write4(value >> 4, rs);
    write4(value & 0x0F, rs);
}

bool Lcd::begin(uint8_t sda, uint8_t scl) {
    Wire.begin(sda, scl, 100000);
    Wire.setTimeOut(20);
    for (uint8_t r = 0; r < ROWS; r++)
        for (uint8_t c = 0; c < COLS; c++) { _want[r][c] = ' '; _shown[r][c] = 0; }
    reinit();
    return _ok;
}

void Lcd::reinit() {
    if (_addr == 0 || !_ok) {
        if (probe(0x27)) _addr = 0x27;
        else if (probe(0x3F)) _addr = 0x3F;
        else { _ok = false; return; }
    }
    _txErr = false;
    // 4비트 모드로 들어가는 순서 (HD44780 데이터시트)
    expander(0);
    delay(5);
    write4(0x03, false); delay(5);
    write4(0x03, false); delayMicroseconds(150);
    write4(0x03, false);
    write4(0x02, false);
    command(0x28);          // 4비트, 2줄, 5x8
    command(0x0C);          // 화면 켬, 커서 끔
    command(0x06);          // 오른쪽으로 진행
    command(0x01);          // 지움
    delay(2);
    for (uint8_t r = 0; r < ROWS; r++)
        for (uint8_t c = 0; c < COLS; c++) _shown[r][c] = 0;   // 전부 다시 쓰게
    _ok = !_txErr;
}

void Lcd::setLine(uint8_t row, const char* text) {
    if (row >= ROWS) return;
    uint8_t c = 0;
    for (; c < COLS && text[c]; c++) _want[row][c] = text[c];
    for (; c < COLS; c++) _want[row][c] = ' ';
}

void Lcd::flush() {
    if (!_ok) return;
    _txErr = false;
    for (uint8_t r = 0; r < ROWS; r++) {
        int8_t cursor = -1;
        for (uint8_t c = 0; c < COLS; c++) {
            if (_want[r][c] == _shown[r][c]) { cursor = -1; continue; }
            if (cursor != (int8_t)c) command((uint8_t)(0x80 | (r ? 0x40 : 0x00) | c));
            send((uint8_t)_want[r][c], true);
            if (_txErr) { _ok = false; return; }
            _shown[r][c] = _want[r][c];
            cursor = (int8_t)(c + 1);
        }
    }
}

void Lcd::setBacklight(bool on) {
    if (on == _bl) return;
    _bl = on;
    if (_ok) expander(0);
}
