// ============================================================
//  PCB 첫 연결 시험 (LCD, 엔코더, 모터 드라이버를 단 상태)
//  빌드와 업로드: pio run -e test_pcb -t upload, 시리얼 115200bps (COM 포트)
//
//  하는 일
//   1. 켜자마자 모터 PWM/EN, MC FET 핀을 LOW로 묶는다. 이 시험은 출력 핀을 HIGH로 만들지 않는다.
//   2. I2C 선 레벨과 장치 주소를 찾고, LCD에 꽉 찬 칸을 1초 띄운 뒤 측정값을 쪽마다 돌려 보인다.
//   3. 핀마다 내부 풀업/풀다운을 걸어 읽어서, 납땜 붙음(GND나 전원에 붙음)을 찾는다.
//   4. 1초마다 아날로그 값과 입력 핀을 시리얼에 찍는다.
//   5. 엔코더를 돌리거나 누르면 [enc]를 찍고, LCD에 3초 동안 엔코더 쪽을 보인다.
//  시리얼 명령: p 핀 시험 다시, i I2C 다시 찾기, b 삑 소리, l 상태 LED 켜고 끄기,
//             w 20초 동안 어느 핀이 바뀌는지 감시 (엔코더 선 찾기), h 도움말
//
//  본 펌웨어 층 규칙과 상관없는 시험용 파일이다. 본 빌드에는 들어가지 않는다.
// ============================================================
#include <Arduino.h>
#include <Wire.h>
#include "pins.h"
#include "dev_Compat.h"
#include "config.h"
#include "dev_Lcd.h"
#include "dev_Encoder.h"

static Lcd s_lcd;
static Encoder s_enc;
static int32_t s_encPos = 0;
static uint16_t s_encPress = 0;
static bool s_encDown = false;
static uint32_t s_encShowUntil = 0;
static bool s_led = false;

// 출력 핀: 어떤 경우에도 LOW로만 둔다
static const uint8_t OUT_PINS[] = {
    PIN_M1_RPWM, PIN_M1_LPWM, PIN_M1_EN, PIN_M2_RPWM, PIN_M2_LPWM, PIN_M2_EN,
    PIN_RELAY_FET_A, PIN_RELAY_FET_B,
};

static void outputsOff() {
    for (uint8_t p : OUT_PINS) { pinMode(p, OUTPUT); digitalWrite(p, LOW); }
}

// ---- 핀 풀업/풀다운 시험 ----
struct PinInfo { uint8_t pin; const char* name; const char* expect; };

// 입력 핀: 풀업과 풀다운 모두 걸어 본다
static const PinInfo IN_PINS[] = {
    { PIN_ESTOP_SENSE,  "ESTOP",  "0/0 (10k to GND)" },
    { PIN_TEMP_SENSE,   "TEMP",   "0/0 (120R to GND)" },
    { PIN_M1_CURRENT,   "M1_IS",  "0/0 (R_IS to GND)" },
    { PIN_M2_CURRENT,   "M2_IS",  "0/0 (R_IS to GND)" },
    { PIN_RELAY_SENSE,  "MC_V",   "0/0 (10k to GND)" },
    { PIN_S1,           "S1",     "1/1 (10k pull-up)" },
    { PIN_S2,           "S2",     "1/1 (10k pull-up)" },
    { PIN_MODEL_JUMPER, "JUMPER", "open 1/0, GND 0/0" },
    { PIN_ENC_A,        "ENC_A",  "no encoder: 1/0 or 1/1" },
    { PIN_ENC_B,        "ENC_B",  "no encoder: 1/0 or 1/1" },
    { PIN_ENC_SW,       "ENC_SW", "no encoder: 1/0 or 1/1" },
    { 18,               "GPIO18", "spare: 1/0" },
};
static const PinInfo OUTP_INFO[] = {
    { PIN_M1_RPWM, "M1_RPWM", "" }, { PIN_M1_LPWM, "M1_LPWM", "" }, { PIN_M1_EN, "M1_EN", "" },
    { PIN_M2_RPWM, "M2_RPWM", "" }, { PIN_M2_LPWM, "M2_LPWM", "" }, { PIN_M2_EN, "M2_EN", "" },
    { PIN_RELAY_FET_A, "FET_A", "" }, { PIN_RELAY_FET_B, "FET_B", "" },
};

static int readWith(uint8_t pin, uint8_t mode) {
    pinMode(pin, mode);
    delay(3);
    return digitalRead(pin);
}

static uint8_t s_pinFail = 0;

static void pinTest() {
    Serial.println("\n[pin] input pins: pull-up/pull-down read (1/1 = held HIGH, 0/0 = held LOW, 1/0 = floating)");
    for (const PinInfo& p : IN_PINS) {
        int u = readWith(p.pin, INPUT_PULLUP);
        int d = readWith(p.pin, INPUT_PULLDOWN);
        pinMode(p.pin, INPUT);
        Serial.printf("[pin] GPIO%-2u %-7s %d/%d   expect %s\n", p.pin, p.name, u, d, p.expect);
    }
    // 출력 핀은 풀다운만 건다 (풀업을 걸면 EN이 잠깐 HIGH가 될 수 있다).
    // 풀다운인데 1이면 전원 쪽에 붙어 있다.
    Serial.println("[pin] output pins: pull-down read only (expect 0; 1 = shorted to 3V3/5V)");
    s_pinFail = 0;
    for (const PinInfo& p : OUTP_INFO) {
        digitalWrite(p.pin, LOW);
        int d = readWith(p.pin, INPUT_PULLDOWN);
        pinMode(p.pin, OUTPUT);
        digitalWrite(p.pin, LOW);
        if (d) s_pinFail++;
        Serial.printf("[pin] GPIO%-2u %-7s %d %s\n", p.pin, p.name, d, d ? "<- CHECK" : "OK");
    }
    outputsOff();
}

// ---- I2C ----
static uint8_t s_i2cFound = 0;
static uint8_t s_i2cFirst = 0;

static void i2cScan() {
    pinMode(PIN_LCD_SDA, INPUT);
    pinMode(PIN_LCD_SCL, INPUT);
    delay(2);
    int sda = digitalRead(PIN_LCD_SDA), scl = digitalRead(PIN_LCD_SCL);
    Serial.printf("\n[i2c] idle level SDA(%u)=%d SCL(%u)=%d %s\n", PIN_LCD_SDA, sda, PIN_LCD_SCL, scl,
                  (sda && scl) ? "OK" : "<- should be 1: pull-up, level shifter power, short to GND?");
    Wire.begin(PIN_LCD_SDA, PIN_LCD_SCL, 100000);
    Wire.setTimeOut(20);
    s_i2cFound = 0;
    s_i2cFirst = 0;
    for (uint8_t a = 0x08; a < 0x78; a++) {
        Wire.beginTransmission(a);
        if (Wire.endTransmission() == 0) {
            Serial.printf("[i2c] device at 0x%02X\n", a);
            if (s_i2cFound == 0) s_i2cFirst = a;
            s_i2cFound++;
        }
    }
    if (s_i2cFound == 0) Serial.println("[i2c] no device (LCD 5V, SDA/SCL swapped, level shifter?)");
}

// ---- 소리, LED ----
static void beep() {
    pwmTone(PIN_BUZZER, LEDC_CH_BUZZER, 2000);
    delay(150);
    pwmTone(PIN_BUZZER, LEDC_CH_BUZZER, 0);
    Serial.printf("[buz] beep on GPIO%u (no sound: buzzer may be on GPIO%u, see pins.h)\n",
                  PIN_BUZZER, PIN_STATUS_LED);
}

// ---- 측정 ----
struct Reading {
    int32_t estopMv, tempMv, m1Mv, m2Mv, mcMv;   // 핀 전압 mV
    int s1, s2, jumper, encA, encB, encSw;
};

static Reading readAll() {
    Reading r;
    r.estopMv = readMv(PIN_ESTOP_SENSE, 8);
    r.tempMv  = readMv(PIN_TEMP_SENSE, 8);
    r.m1Mv    = readMv(PIN_M1_CURRENT, 8);
    r.m2Mv    = readMv(PIN_M2_CURRENT, 8);
    r.mcMv    = readMv(PIN_RELAY_SENSE, 8);
    r.s1 = digitalRead(PIN_S1);
    r.s2 = digitalRead(PIN_S2);
    r.jumper = digitalRead(PIN_MODEL_JUMPER);
    r.encA = digitalRead(PIN_ENC_A);
    r.encB = digitalRead(PIN_ENC_B);
    r.encSw = digitalRead(PIN_ENC_SW);
    return r;
}

// 분압 100k/10k 앞쪽(24V 쪽) 전압을 "12.3" 꼴로
static void volts24(char* out, size_t n, int32_t pinMv) {
    int32_t mv = pinMv * 11;
    snprintf(out, n, "%ld.%ld", (long)(mv / 1000), (long)((mv % 1000) / 100));
}

// 전류 환산 (본 펌웨어 dev_MotorDriver와 같은 식): 부하 mA = 감지 mV × KILIS ÷ 감지 저항
// 감지 저항은 모델 점퍼로 바뀐다: 50W 2.2kΩ, 300W 817Ω (2.2kΩ ∥ 1.3kΩ)
static int32_t senseOhm(const Reading& r) { return r.jumper == LOW ? SENSE_OHM_50W : SENSE_OHM_300W; }
static int32_t loadMa(int32_t mv, int32_t ohm) { return (int32_t)((int64_t)mv * BTS_KILIS / ohm); }

static void printReading(const Reading& r) {
    char es[12], mc[12];
    volts24(es, sizeof es, r.estopMv);
    volts24(mc, sizeof mc, r.mcMv);
    Serial.printf("[live] %lus  ESTOP %ldmV(%sV)  MC %ldmV(%sV)  TEMP %ldmV  I1 %ldmV  I2 %ldmV  "
                  "S1=%d S2=%d JP=%d(%s)  A=%d B=%d SW=%d\n",
                  (unsigned long)(millis() / 1000), (long)r.estopMv, es, (long)r.mcMv, mc,
                  (long)r.tempMv, (long)r.m1Mv, (long)r.m2Mv, r.s1, r.s2, r.jumper,
                  r.jumper == LOW ? "50W" : "300W", r.encA, r.encB, r.encSw);
    int32_t ohm = senseOhm(r);
    Serial.printf("[cur]  R_IS %ldohm  I1 %ldmV = %ldmA%s  I2 %ldmV = %ldmA%s\n", (long)ohm,
                  (long)r.m1Mv, (long)loadMa(r.m1Mv, ohm), r.m1Mv >= DRIVER_FAULT_MV ? " (FAULT level)" : "",
                  (long)r.m2Mv, (long)loadMa(r.m2Mv, ohm), r.m2Mv >= DRIVER_FAULT_MV ? " (FAULT level)" : "");
}

static void showPage(uint8_t page, const Reading& r) {
    char a[24], b[24], v1[12], v2[12];
    switch (page) {
    case 0:
        snprintf(a, sizeof a, "PCB TEST %s", s_lcd.kind());
        snprintf(b, sizeof b, "I2C %u 0x%02X %lus", s_i2cFound, s_lcd.addr(), (unsigned long)(millis() / 1000));
        break;
    case 1:
        volts24(v1, sizeof v1, r.estopMv);
        volts24(v2, sizeof v2, r.mcMv);
        snprintf(a, sizeof a, "ESTOP %sV", v1);
        snprintf(b, sizeof b, "MC    %sV", v2);
        break;
    case 2:
        snprintf(a, sizeof a, "TEMP %ldmV", (long)r.tempMv);
        snprintf(b, sizeof b, "R_IS %ldohm", (long)senseOhm(r));
        break;
    case 3: {   // 전류: 감지 mV와 환산 mA
        int32_t ohm = senseOhm(r);
        snprintf(a, sizeof a, "I1 %4ldmV %5ldmA", (long)r.m1Mv, (long)loadMa(r.m1Mv, ohm));
        snprintf(b, sizeof b, "I2 %4ldmV %5ldmA", (long)r.m2Mv, (long)loadMa(r.m2Mv, ohm));
        break;
    }
    case 9:   // 엔코더
        snprintf(a, sizeof a, "ENC pos %ld", (long)s_encPos);
        snprintf(b, sizeof b, "SW %s n%u", s_encDown ? "DOWN" : "up  ", s_encPress);
        break;
    default:
        snprintf(a, sizeof a, "S1 %d S2 %d %s", r.s1, r.s2, r.jumper == LOW ? "50W" : "300W");
        snprintf(b, sizeof b, "A%d B%d SW%d OUT%s", r.encA, r.encB, r.encSw, s_pinFail ? "!!" : "ok");
        break;
    }
    s_lcd.setLine(0, a);
    s_lcd.setLine(1, b);
    s_lcd.flush();
}

// ---- 핀 감시: 엔코더 선이 어느 핀으로 들어오는지 찾는다 ----
// 출력 핀, I2C, 플래시/PSRAM(26~37), USB(19, 20), UART(43, 44), 피에조는 뺀다.
static const uint8_t WATCH_PINS[] = { 0, 1, 2, 3, 4, 5, 6, 7, 10, 18, 39, 40, 41, 42, 45, 46, 47, 48 };

static void watchPins(uint32_t ms) {
    const uint8_t N = sizeof WATCH_PINS;
    uint16_t changes[N] = {};
    int last[N];
    detachInterrupt(digitalPinToInterrupt(PIN_ENC_A));
    detachInterrupt(digitalPinToInterrupt(PIN_ENC_B));
    for (uint8_t i = 0; i < N; i++) { pinMode(WATCH_PINS[i], INPUT_PULLUP); }
    delay(5);
    for (uint8_t i = 0; i < N; i++) last[i] = digitalRead(WATCH_PINS[i]);
    Serial.printf("[watch] %lus: turn the encoder now\n", (unsigned long)(ms / 1000));
    uint32_t start = millis();
    while (millis() - start < ms) {
        for (uint8_t i = 0; i < N; i++) {
            int v = digitalRead(WATCH_PINS[i]);
            if (v != last[i]) {
                last[i] = v;
                if (changes[i] < 5) Serial.printf("[watch] GPIO%u -> %d\n", WATCH_PINS[i], v);
                changes[i]++;
            }
        }
        delayMicroseconds(200);
    }
    Serial.println("[watch] result (pins that changed):");
    bool any = false;
    for (uint8_t i = 0; i < N; i++) {
        if (changes[i]) { Serial.printf("[watch] GPIO%u changed %u times\n", WATCH_PINS[i], changes[i]); any = true; }
    }
    if (!any) Serial.println("[watch] nothing changed");
    for (uint8_t i = 0; i < N; i++) pinMode(WATCH_PINS[i], INPUT);
    s_enc.begin(PIN_ENC_A, PIN_ENC_B, PIN_ENC_SW);
}

static void help() {
    Serial.println("[help] p: pin test   i: I2C scan   b: beep   l: status LED on/off   w: watch pins 20s   h: help");
}

void setup() {
    outputsOff();                     // 가장 먼저
    pinMode(PIN_STATUS_LED, OUTPUT);
    digitalWrite(PIN_STATUS_LED, LOW);
    pwmAttach(PIN_BUZZER, LEDC_CH_BUZZER, 2000, 8);
    pwmTone(PIN_BUZZER, LEDC_CH_BUZZER, 0);
    Serial.begin(115200);
    delay(300);
    Serial.printf("\n[boot] WinCtrl PCB test  build %s %s\n", __DATE__, __TIME__);
    Serial.println("[boot] motor PWM/EN and MC FET pins are held LOW for the whole test");

    pinTest();
    i2cScan();
    s_enc.begin(PIN_ENC_A, PIN_ENC_B, PIN_ENC_SW);

    bool ok = s_lcd.begin(PIN_LCD_SDA, PIN_LCD_SCL);
    Serial.printf("[lcd] %s %s at 0x%02X\n", ok ? "found" : "NOT found", ok ? s_lcd.kind() : "-", s_lcd.addr());
    if (ok) {
        // 모든 칸이 꽉 차 보이는지 (명암 조절, 빠진 점 확인)
        char full[Lcd::COLS + 1];
        memset(full, 0xFF, Lcd::COLS);
        full[Lcd::COLS] = 0;
        s_lcd.setLine(0, full);
        s_lcd.setLine(1, full);
        s_lcd.flush();
        delay(1000);
        s_lcd.setLine(0, "0123456789ABCDEF");
        s_lcd.setLine(1, "abcdefghijklmnop");
        s_lcd.flush();
        delay(1000);
    }
    beep();
    help();
}

void loop() {
    static uint32_t lastPrint = 0, lastPage = 0, lastLcdTry = 0;
    static uint8_t page = 0;
    uint32_t now = millis();

    while (Serial.available() > 0) {
        int c = Serial.read();
        if (c == 'p') { pinTest(); s_enc.begin(PIN_ENC_A, PIN_ENC_B, PIN_ENC_SW); }
        else if (c == 'i') { i2cScan(); s_lcd.reinit(); }
        else if (c == 'b') beep();
        else if (c == 'l') {
            s_led = !s_led;
            digitalWrite(PIN_STATUS_LED, s_led ? HIGH : LOW);
            Serial.printf("[led] GPIO%u %s\n", PIN_STATUS_LED, s_led ? "HIGH" : "LOW");
        }
        else if (c == 'w') watchPins(20000);
        else if (c == 'h') help();
    }

    // 엔코더: 칸마다 위치 +-1 (시계 방향 +), 누름은 떨림 거른 값
    EncStep steps[16];
    uint8_t n = s_enc.take(steps, 16);
    for (uint8_t i = 0; i < n; i++) {
        s_encPos += steps[i].dir;
        Serial.printf("[enc] %s pos %ld  (edges %lu)\n", steps[i].dir > 0 ? "CW " : "CCW",
                      (long)s_encPos, (unsigned long)s_enc.edgeCount());
    }
    bool down = s_enc.pressed();
    if (down != s_encDown) {
        s_encDown = down;
        if (down) s_encPress++;
        Serial.printf("[enc] switch %s (count %u)\n", down ? "DOWN" : "UP", s_encPress);
    }
    if (n > 0 || down) s_encShowUntil = now + 3000;

    Reading r = readAll();
    if (now - lastPrint >= 1000) {
        lastPrint = now;
        printReading(r);
    }
    if (now - lastPage >= 2000) {
        lastPage = now;
        page = (page + 1) % 5;
    }
    if (!s_lcd.ok() && now - lastLcdTry >= 3000) {   // 나중에 꽂아도 잡히게
        lastLcdTry = now;
        s_lcd.reinit();
        if (s_lcd.ok()) Serial.printf("[lcd] found %s at 0x%02X\n", s_lcd.kind(), s_lcd.addr());
    }
    if (s_lcd.ok()) showPage((int32_t)(s_encShowUntil - now) > 0 ? 9 : page, r);
    delay(10);
}
