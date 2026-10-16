#include <Arduino.h>
#include <Wire.h>
#include <string.h>
#include "dev_Lcd.h"
#include "dev_OledFont.h"

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
        _oled = false;
        if (probe(0x27)) _addr = 0x27;
        else if (probe(0x3F)) _addr = 0x3F;
        else if (probe(0x3C)) { _addr = 0x3C; _oled = true; }
        else if (probe(0x3D)) { _addr = 0x3D; _oled = true; }
        else { _ok = false; return; }
    }
    _txErr = false;
    if (_oled) {
        oledInit();
        for (uint8_t r = 0; r < ROWS; r++)
            for (uint8_t c = 0; c < COLS; c++) _shown[r][c] = 0;
        _ok = !_txErr;
        return;
    }
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
    if (_oled) {
        for (uint8_t r = 0; r < ROWS; r++) {
            if (memcmp(_want[r], _shown[r], COLS) == 0) continue;
            oledRow(r);
            if (_txErr) { _ok = false; return; }
            memcpy(_shown[r], _want[r], COLS);
        }
        return;
    }
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
    if (!_ok) return;
    if (_oled) oledCmd(on ? 0xAF : 0xAE);   // OLED는 화면 켬/끔
    else expander(0);
}

// ---------------- 임시 OLED (SSD1306 / SH1106) ----------------
// 페이지 주소 방식만 써서 두 칩 모두에서 돈다. SH1106은 2열 밀려 보일 수 있다.
bool Lcd::oledCmd(uint8_t c) {
    Wire.beginTransmission(_addr);
    Wire.write(0x00);                  // 명령
    Wire.write(c);
    if (Wire.endTransmission() != 0) { _txErr = true; return false; }
    return true;
}

void Lcd::oledInit() {
    static const uint8_t seq[] = {
        0xAE,             // 끔
        0xD5, 0x80,       // 클럭
        0xA8, 0x3F,       // 64줄
        0xD3, 0x00,       // 화면 위치
        0x40,             // 시작 줄 0
        0x8D, 0x14,       // 내부 승압 켬 (SSD1306)
        0x20, 0x02,       // 페이지 주소 방식
        0xA1, 0xC8,       // 좌우, 상하 뒤집기 (일반 모듈 방향)
        0xDA, 0x12,
        0x81, 0xCF,       // 밝기
        0xD9, 0xF1,
        0xDB, 0x40,
        0xA4, 0xA6,       // 화면 내용 따름, 정상 표시
    };
    for (uint8_t i = 0; i < sizeof(seq); i++) oledCmd(seq[i]);
    // 8페이지 모두 지움. SH1106은 메모리가 132열이라 132열까지 지운다.
    // (지우지 않으면 오른쪽 끝에 세로 줄무늬가 남는다. SSD1306은 넘친 4바이트가 0열로 돌아가 다시 0을 쓴다.)
    for (uint8_t p = 0; p < 8; p++) {
        oledCmd((uint8_t)(0xB0 | p)); oledCmd(0x00); oledCmd(0x10);
        for (uint8_t x = 0; x < 132; x += 12) {
            Wire.beginTransmission(_addr);
            Wire.write(0x40);
            for (uint8_t k = 0; k < 12; k++) Wire.write(0);
            if (Wire.endTransmission() != 0) _txErr = true;
        }
    }
    oledCmd(_bl ? 0xAF : 0xAE);
}

// 글자 줄 하나 = 8x16 글자 16개 = 페이지 2개. 0번 줄은 1~2페이지, 1번 줄은 4~5페이지.
void Lcd::oledRow(uint8_t row) {
    uint8_t page0 = row == 0 ? 1 : 4;
    for (uint8_t half = 0; half < 2; half++) {
        oledCmd((uint8_t)(0xB0 | (page0 + half))); oledCmd(0x00); oledCmd(0x10);
        for (uint8_t c = 0; c < COLS; c += 2) {      // 한 번에 글자 2개(16바이트)
            Wire.beginTransmission(_addr);
            Wire.write(0x40);                         // 데이터
            for (uint8_t k = 0; k < 2; k++) {
                uint8_t ch = (uint8_t)_want[row][c + k];
                if (ch < 32 || ch > 126) ch = '?';
                const uint8_t* g = OLED_FONT_8X16[ch - 32] + half * 8;
                for (uint8_t x = 0; x < 8; x++) Wire.write(g[x]);
            }
            if (Wire.endTransmission() != 0) { _txErr = true; return; }
        }
    }
}
