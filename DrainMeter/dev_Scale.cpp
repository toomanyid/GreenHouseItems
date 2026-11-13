#include "dev_Scale.h"
#include <Arduino.h>
#include "pins.h"
#include "config.h"

void scaleBegin() {
    pinMode(PIN_HX1_DOUT, INPUT);
    pinMode(PIN_HX2_DOUT, INPUT);
    pinMode(PIN_HX_SCK, OUTPUT);
    digitalWrite(PIN_HX_SCK, LOW);
}

void scaleSleep() {
    digitalWrite(PIN_HX_SCK, LOW);
    digitalWrite(PIN_HX_SCK, HIGH);
    delayMicroseconds(100);   // 60µs 넘게 HIGH면 쉰다
}

static bool bothReady() {
    return digitalRead(PIN_HX1_DOUT) == LOW && digitalRead(PIN_HX2_DOUT) == LOW;
}

static int32_t signExtend24(uint32_t v) {
    if (v & 0x800000UL) v |= 0xFF000000UL;
    return (int32_t)v;
}

bool scaleReadRaw(ScaleRaw& out) {
    uint32_t t0 = millis();
    while (!bothReady()) {
        if (millis() - t0 > HX_READY_TIMEOUT_MS) return false;
        delay(1);
    }

    uint32_t va = 0, vb = 0;
    // 클럭 HIGH가 50µs를 넘으면 HX711이 쉬어 버리므로 읽는 동안 끼어들기를 막는다
    noInterrupts();
    for (uint8_t i = 0; i < 24; i++) {
        digitalWrite(PIN_HX_SCK, HIGH);
        delayMicroseconds(1);
        va = (va << 1) | (digitalRead(PIN_HX1_DOUT) ? 1 : 0);
        vb = (vb << 1) | (digitalRead(PIN_HX2_DOUT) ? 1 : 0);
        digitalWrite(PIN_HX_SCK, LOW);
        delayMicroseconds(1);
    }
    // 25번째 클럭: 다음 측정도 채널 A, 128배
    digitalWrite(PIN_HX_SCK, HIGH);
    delayMicroseconds(1);
    digitalWrite(PIN_HX_SCK, LOW);
    interrupts();

    out.a = signExtend24(va);
    out.b = signExtend24(vb);
    return true;
}

void scaleWake() {
    digitalWrite(PIN_HX_SCK, LOW);
    delay(HX_SETTLE_MS);
    ScaleRaw dummy;
    scaleReadRaw(dummy);   // 깨어난 직후 첫 값은 버린다
}

bool scaleReadAvg(ScaleRaw& out, uint8_t n) {
    if (n == 0) n = 1;
    int64_t sa = 0, sb = 0;
    for (uint8_t i = 0; i < n; i++) {
        ScaleRaw r;
        if (!scaleReadRaw(r)) return false;
        sa += r.a;
        sb += r.b;
    }
    out.a = (int32_t)(sa / n);
    out.b = (int32_t)(sb / n);
    return true;
}
