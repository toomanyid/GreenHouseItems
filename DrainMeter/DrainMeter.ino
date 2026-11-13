// ============================================================
//  배액 측정기 펌웨어 0.9 (DrainMeter)
//  Seeed XIAO ESP32C6
//
//  지금은 뼈대만 있다. 부품을 하나씩 확인한 뒤 (hwtest 폴더)
//  README "0.9에서 할 일" 순서로 채운다.
// ============================================================
#include <Arduino.h>
#include <Wire.h>
#include "pins.h"
#include "config.h"
#include "dev_Scale.h"

void setup() {
    Serial.begin(115200);
    delay(1500);   // USB 시리얼이 붙을 시간
    Serial.printf("\n[drainmeter] version %s\n", DRAINMETER_VERSION);

    pinMode(PIN_BUTTON, INPUT);       // 외부 풀업
    pinMode(PIN_LATCH_REED, INPUT);   // 외부 1MΩ 풀업

    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    scaleBegin();
    scaleSleep();
}

void loop() {
    delay(1000);
}
