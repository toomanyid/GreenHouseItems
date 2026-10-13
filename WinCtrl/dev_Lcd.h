#pragma once
#include <stdint.h>

// ============================================================
//  1층: 1602 LCD (HD44780 + PCF8574 I2C 백팩, 주소 0x27 또는 0x3F).
//  화면 태스크는 setLine()으로 그림자 버퍼에 쓰고, flush()가 바뀐 글자만 보낸다.
//  통신이 실패해도 제어에는 영향이 없다. 주기적으로 다시 초기화한다.
// ============================================================
class Lcd {
public:
    static const uint8_t COLS = 16;
    static const uint8_t ROWS = 2;

    bool begin(uint8_t sda, uint8_t scl);
    void setLine(uint8_t row, const char* text);   // 16글자보다 짧으면 빈칸으로 채움
    void flush();                                   // 바뀐 글자만 보냄
    void setBacklight(bool on);
    void reinit();                                  // 다시 초기화하고 전부 다시 씀
    bool ok() const { return _ok; }

private:
    void write4(uint8_t nibble, bool rs);
    void send(uint8_t value, bool rs);
    void command(uint8_t c) { send(c, false); }
    bool expander(uint8_t v);
    bool probe(uint8_t addr);

    uint8_t _addr = 0;
    bool    _ok = false;
    bool    _bl = true;
    char    _want[ROWS][COLS];
    char    _shown[ROWS][COLS];
    bool    _txErr = false;
};
