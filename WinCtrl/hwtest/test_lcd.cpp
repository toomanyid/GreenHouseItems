// ============================================================
//  LCD 시험 (화면이 안 보일 때 어느 쪽 문제인지 가른다)
//  빌드와 업로드: pio run -e test_lcd -t upload, 시리얼 115200bps (COM 포트)
//
//  켜자마자 모터 PWM/EN, MC FET 핀을 모두 LOW로 묶고 다시 건드리지 않는다 (모터가 달려 있어도 안전).
//
//  가르는 순서 (켤 때 자동으로 한 번, r 명령으로 다시)
//   1. I2C에 백팩(PCF8574)이 응답하는가          -> 안 하면 I2C 선, 백팩 5V
//   2. LCD 컨트롤러(HD44780)가 응답하는가         -> PCF8574로 LCD 메모리를 되읽는다.
//      쓴 글자가 그대로 읽히면 LCD는 글자를 받고 있다 -> 안 보이면 대비(백팩 파란 가변저항) 또는 유리/백라이트.
//      읽은 값이 FF뿐이면 LCD가 데이터 선을 움직이지 않는다 -> 백팩과 LCD 사이 16핀 납땜, LCD 5V.
//   3. 백라이트를 0.5초씩 3번 깜빡인다           -> 안 깜빡이면 백라이트 점퍼(백팩 LED 핀)나 백팩 배선 순서
//   4. 네 줄 모두 꽉 찬 칸 3초, 그다음 네 줄 글자와 초 세기
//
//  시리얼 명령: r 되읽기 진단, f 꽉 찬 칸(4줄), t 글자(4줄), c 지우기, k 백라이트 깜빡임,
//             b 백라이트 켜고 끄기, s I2C 다시 찾기, d 본 펌웨어 LCD 드라이버로 그리기,
//             e 백팩 핀 P0~P7을 하나씩 3초 HIGH (테스터로 LCD 핀 확인), h 도움말
//
//  본 펌웨어 층 규칙과 상관없는 시험용 파일이다. 본 빌드에는 들어가지 않는다.
// ============================================================
#include <Arduino.h>
#include <Wire.h>
#include <string.h>
#include "pins.h"
#include "dev_Lcd.h"

static const uint8_t OUT_PINS[] = {
    PIN_M1_RPWM, PIN_M1_LPWM, PIN_M1_EN, PIN_M2_RPWM, PIN_M2_LPWM, PIN_M2_EN,
    PIN_RELAY_FET_A, PIN_RELAY_FET_B,
};

// PCF8574 핀: P0 RS, P1 RW, P2 EN, P3 백라이트, P4~P7 D4~D7 (흔한 백팩 배선, 본 드라이버와 같음)
static const uint8_t BIT_RS = 0x01;
static const uint8_t BIT_RW = 0x02;
static const uint8_t BIT_EN = 0x04;
static const uint8_t BIT_BL = 0x08;
static const uint8_t ROW_ADDR[4] = { 0x00, 0x40, 0x14, 0x54 };   // 2004 줄 시작 주소

static uint8_t s_addr = 0;
static bool s_bl = true;
static uint16_t s_txErr = 0;
static Lcd s_drv;

// ---- PCF8574 ----
static void pcfWrite(uint8_t v) {
    if (!s_addr) return;
    Wire.beginTransmission(s_addr);
    Wire.write((uint8_t)(v | (s_bl ? BIT_BL : 0)));
    if (Wire.endTransmission() != 0) s_txErr++;
}

static int pcfRead() {
    if (!s_addr) return -1;
    if (Wire.requestFrom(s_addr, (uint8_t)1) != 1) { s_txErr++; return -1; }
    return Wire.read();
}

static void write4(uint8_t nib, bool rs) {
    uint8_t v = (uint8_t)((nib & 0x0F) << 4) | (rs ? BIT_RS : 0);
    pcfWrite(v | BIT_EN);
    delayMicroseconds(1);
    pcfWrite(v);
    delayMicroseconds(50);
}

static void send(uint8_t v, bool rs) { write4(v >> 4, rs); write4(v & 0x0F, rs); }
static void cmd(uint8_t c) { send(c, false); }

// RW=1로 반 바이트 읽기. 데이터 핀은 HIGH(약한 풀업)로 두어 LCD가 끌어내리게 한다.
static int read4(bool rs) {
    uint8_t v = (uint8_t)(0xF0 | BIT_RW | (rs ? BIT_RS : 0));
    pcfWrite(v);
    pcfWrite(v | BIT_EN);
    delayMicroseconds(2);
    int r = pcfRead();
    pcfWrite(v);
    delayMicroseconds(2);
    return r < 0 ? -1 : (r >> 4) & 0x0F;
}

static int readByte(bool rs) {
    int h = read4(rs), l = read4(rs);
    if (h < 0 || l < 0) return -1;
    return (h << 4) | l;
}

static void lcdInit() {
    pcfWrite(0);
    delay(50);
    write4(0x03, false); delay(5);
    write4(0x03, false); delayMicroseconds(150);
    write4(0x03, false);
    write4(0x02, false);
    cmd(0x28);          // 4비트, 2줄(2004는 4줄 모두 켜짐), 5x8
    cmd(0x0C);          // 화면 켬, 커서 끔
    cmd(0x06);
    cmd(0x01);
    delay(3);
}

static void putRow(uint8_t row, const char* text) {
    cmd((uint8_t)(0x80 | ROW_ADDR[row & 3]));
    uint8_t i = 0;
    for (; i < 20 && text[i]; i++) send((uint8_t)text[i], true);
    for (; i < 20; i++) send(' ', true);
}

// ---- I2C ----
static void i2cScan() {
    pinMode(PIN_LCD_SDA, INPUT);
    pinMode(PIN_LCD_SCL, INPUT);
    delay(2);
    int sda = digitalRead(PIN_LCD_SDA), scl = digitalRead(PIN_LCD_SCL);
    Serial.printf("\n[i2c] idle level SDA(%u)=%d SCL(%u)=%d %s\n", PIN_LCD_SDA, sda, PIN_LCD_SCL, scl,
                  (sda && scl) ? "OK" : "<- should be 1");
    Wire.begin(PIN_LCD_SDA, PIN_LCD_SCL, 100000);
    Wire.setTimeOut(20);
    s_addr = 0;
    uint8_t n = 0;
    for (uint8_t a = 0x08; a < 0x78; a++) {
        Wire.beginTransmission(a);
        if (Wire.endTransmission() == 0) {
            Serial.printf("[i2c] device at 0x%02X\n", a);
            n++;
            if (!s_addr && ((a >= 0x20 && a <= 0x27) || (a >= 0x38 && a <= 0x3F))) s_addr = a;
        }
    }
    if (!s_addr) Serial.println("[i2c] no PCF8574 backpack (0x20-0x27, 0x38-0x3F) -> I2C wires, backpack 5V");
    else Serial.printf("[i2c] using backpack 0x%02X (%u device(s))\n", s_addr, n);
}

// ---- 진단 ----
static void diagnose() {
    Serial.println("\n[diag] ---- LCD readback test ----");
    if (!s_addr) { Serial.println("[diag] RESULT: no backpack on I2C"); return; }
    s_txErr = 0;
    int raw = pcfRead();
    Serial.printf("[diag] PCF8574 port read 0x%02X\n", raw & 0xFF);
    lcdInit();
    int st0 = readByte(false);
    Serial.printf("[diag] status after clear: 0x%02X (expect 0x00: busy 0, address 0)\n", st0 & 0xFF);
    const char* pat = "WinCtrl1";
    cmd(0x80);
    for (uint8_t i = 0; i < 8; i++) send((uint8_t)pat[i], true);
    int st1 = readByte(false);
    Serial.printf("[diag] status after 8 chars: 0x%02X (expect 0x08)\n", st1 & 0xFF);
    cmd(0x80);
    char got[9];
    uint8_t match = 0, ff = 0;
    for (uint8_t i = 0; i < 8; i++) {
        int c = readByte(true);
        got[i] = (c >= 32 && c < 127) ? (char)c : '.';
        if (c == (uint8_t)pat[i]) match++;
        if (c == 0xFF) ff++;
        Serial.printf("[diag] DDRAM[%u] wrote 0x%02X read 0x%02X\n", i, (uint8_t)pat[i], c & 0xFF);
    }
    got[8] = 0;
    Serial.printf("[diag] wrote \"%s\" read \"%s\"  match %u/8  I2C errors %u\n", pat, got, match, s_txErr);
    if (s_txErr) Serial.println("[diag] RESULT: I2C errors during test -> loose I2C wire or noise");
    else if (match == 8 && (st1 & 0x7F) == 8)
        Serial.println("[diag] RESULT: LCD controller OK, text is in LCD memory. "
                       "If nothing visible: turn contrast trimmer on backpack slowly end to end; "
                       "if still nothing, check LCD 5V at pin 2 and glass.");
    else if (ff >= 7)
        Serial.println("[diag] RESULT: LCD does not answer (reads all FF). "
                       "Check 16-pin solder between backpack and LCD, LCD 5V (pin 2) and GND (pin 1).");
    else
        Serial.println("[diag] RESULT: partial answer -> some D4-D7/RS/EN line bad, or backpack wiring differs. "
                       "Use 'e' to check backpack pins with a meter.");
}

static void fullBlocks() {
    char row[21];
    memset(row, (char)0xFF, 20);
    row[20] = 0;
    for (uint8_t r = 0; r < 4; r++) putRow(r, row);
    Serial.println("[lcd] full blocks on 4 rows (no blocks with backlight on -> contrast)");
}

static void textRows() {
    putRow(0, "WINCTRL LCD TEST   ");
    putRow(1, "0123456789ABCDEFGHIJ");
    putRow(2, "abcdefghijklmnopqrst");
    putRow(3, "row4");
    Serial.println("[lcd] text on 4 rows");
}

static void blinkBacklight() {
    Serial.println("[lcd] backlight blink x3 (0.5s)");
    for (uint8_t i = 0; i < 3; i++) {
        s_bl = false; pcfWrite(0); delay(500);
        s_bl = true;  pcfWrite(0); delay(500);
    }
}

static void pinWalk() {
    static const char* NAMES[8] = { "P0 RS (LCD pin 4)", "P1 RW (LCD pin 5)", "P2 EN (LCD pin 6)",
                                    "P3 backlight", "P4 D4 (LCD pin 11)", "P5 D5 (LCD pin 12)",
                                    "P6 D6 (LCD pin 13)", "P7 D7 (LCD pin 14)" };
    Serial.println("[pin] each backpack pin HIGH for 3s, others LOW. Measure to LCD GND (pin 1): ~5V on the named pin");
    bool bl = s_bl;
    s_bl = false;
    for (uint8_t b = 0; b < 8; b++) {
        Serial.printf("[pin] %s HIGH\n", NAMES[b]);
        pcfWrite((uint8_t)(1 << b));
        delay(3000);
    }
    s_bl = bl;
    lcdInit();
    textRows();
    Serial.println("[pin] done, LCD re-initialized");
}

static void drawWithDriver() {
    if (!s_drv.begin(PIN_LCD_SDA, PIN_LCD_SCL)) { Serial.println("[drv] firmware driver: LCD not found"); return; }
    s_drv.setLine(0, "FW DRIVER LINE 1");
    s_drv.setLine(1, "FW DRIVER LINE 2");
    s_drv.flush();
    Serial.printf("[drv] firmware driver drew 2 lines on %s 0x%02X, ok=%d\n", s_drv.kind(), s_drv.addr(), s_drv.ok());
}

static void help() {
    Serial.println("[help] r readback diag, f full blocks, t text, c clear, k backlight blink, b backlight on/off,");
    Serial.println("[help] s I2C scan, d firmware driver draw, e backpack pin walk (3s each), h help");
}

static bool s_counter = true;

void setup() {
    for (uint8_t p : OUT_PINS) { pinMode(p, OUTPUT); digitalWrite(p, LOW); }
    Serial.begin(115200);
    delay(300);
    Serial.println("\n[boot] LCD TEST (motor/MC outputs held LOW)");
    i2cScan();
    diagnose();
    if (s_addr) {
        blinkBacklight();
        fullBlocks();
        delay(3000);
        textRows();
    }
    help();
}

void loop() {
    static uint32_t last = 0;
    uint32_t now = millis();
    if (Serial.available()) {
        int c = Serial.read();
        s_counter = true;
        if (c == 'r') { diagnose(); textRows(); }
        else if (c == 'f') { fullBlocks(); s_counter = false; }
        else if (c == 't') textRows();
        else if (c == 'c') { cmd(0x01); delay(3); s_counter = false; Serial.println("[lcd] cleared"); }
        else if (c == 'k') blinkBacklight();
        else if (c == 'b') { s_bl = !s_bl; pcfWrite(0); Serial.printf("[lcd] backlight %s\n", s_bl ? "ON" : "OFF"); }
        else if (c == 's') i2cScan();
        else if (c == 'd') { drawWithDriver(); s_counter = false; }
        else if (c == 'e') pinWalk();
        else if (c == 'h') help();
    }
    if (s_addr && s_counter && now - last >= 1000) {
        last = now;
        char b[21];
        snprintf(b, sizeof b, "row4 %lus err %u", (unsigned long)(now / 1000), s_txErr);
        putRow(3, b);
    }
    delay(5);
}
