#pragma once
#include <stdint.h>

// ============================================================
//  1층: 1602 LCD (HD44780 + PCF8574 I2C 백팩, 주소 0x27 또는 0x3F).
//  2004 LCD는 백팩의 A0 패드를 이어 주소를 0x26으로 바꿔 구분한다.
//  지금은 2004에도 위 두 줄에 1602와 같은 16x2 글자를 그린다 (4줄 화면은 미정).
//  화면 태스크는 setLine()으로 그림자 버퍼에 쓰고, flush()가 바뀐 글자만 보낸다.
//  통신이 실패해도 제어에는 영향이 없다. 주기적으로 다시 초기화한다.
//  사용자 글자 8개(CGRAM)는 setGlyph()로 넣고, 글자 코드 8~15로 쓴다(0은 줄 끝이라 피함).
//  0xFF는 꽉 찬 글자다.
//  임시: LCD가 없고 I2C OLED(SSD1306/SH1106 128x64, 0x3C/0x3D)가 있으면
//  같은 16x2 글자를 OLED에 8x16 글꼴로 그린다. 사용자 글자도 키워서 그린다. 시험용 대체 화면이다.
// ============================================================
class Lcd {
public:
    static const uint8_t COLS = 16;
    static const uint8_t ROWS = 2;
    static const uint8_t GLYPHS = 8;
    static const uint8_t GLYPH_CODE = 8;           // 사용자 글자 0번의 글자 코드

    bool begin(uint8_t sda, uint8_t scl);
    void setLine(uint8_t row, const char* text);   // 16글자보다 짧으면 빈칸으로 채움
    void setGlyph(uint8_t slot, const uint8_t rows[8]);   // 사용자 글자 (5x8, 줄마다 아래 5비트). 바뀐 것만 보냄
    void flush();                                   // 바뀐 글자만 보냄
    void setBacklight(bool on);
    void reinit();                                  // 다시 초기화하고 전부 다시 씀
    bool ok() const { return _ok; }
    bool isOled() const { return _oled; }
    const char* kind() const;                       // "LCD1602", "LCD2004", "OLED"
    uint8_t addr() const { return _addr; }

private:
    void write4(uint8_t nibble, bool rs);
    void send(uint8_t value, bool rs);
    void command(uint8_t c) { send(c, false); }
    bool expander(uint8_t v);
    bool probe(uint8_t addr);
    bool oledCmd(uint8_t c);
    void oledInit();
    void oledRow(uint8_t row);
    void oledGlyph(uint8_t ch, uint8_t half, uint8_t col[8]) const;
    void writeGlyphs();

    bool    _oled = false;

    uint8_t _addr = 0;
    bool    _ok = false;
    bool    _bl = true;
    char    _want[ROWS][COLS];
    char    _shown[ROWS][COLS];
    uint8_t _glyph[GLYPHS][8] = {};
    uint8_t _glyphDirty = 0;        // 비트마다 보낼 사용자 글자
    bool    _txErr = false;
};
