// ============================================================
//  부품 시험: RTC (DS3231) 와 I2C 장치 찾기
//  pio run -e test_rtc -t upload, 그다음 pio device monitor
//
//  켜면 I2C 선에 붙은 장치 주소를 모두 찍는다 (DS3231은 0x68, 모듈의 EEPROM은 0x57).
//  1초마다 시각과 RTC 칩 온도를 찍는다.
//  시리얼 명령 (끝에 엔터):
//    set 2026-11-13 21:30:00    시각 맞추기
//  시각은 한 번만 맞추면 RTC 전지(CR2032)로 유지된다.
// ============================================================
#include <Arduino.h>
#include <Wire.h>
#include <RTClib.h>
#include "pins.h"
#include "config.h"

static RTC_DS3231 rtc;
static bool rtcOk = false;

static void scanI2C() {
    Serial.println("[i2c] 장치 찾는 중...");
    uint8_t found = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            const char* name = "";
            if (addr == I2C_ADDR_DS3231) name = " (DS3231)";
            else if (addr == 0x57) name = " (모듈 EEPROM)";
            else if (addr >= 0x20 && addr <= 0x27) name = " (PCF8574)";
            Serial.printf("[i2c] 0x%02X%s\n", addr, name);
            found++;
        }
    }
    if (found == 0) {
        Serial.println("[i2c] 아무것도 없음. SDA/SCL 배선과 3.3V 전원, 풀업을 확인하세요.");
    }
}

static void handleCommand(String line) {
    line.trim();
    if (!line.startsWith("set ")) {
        Serial.println("명령: set YYYY-MM-DD HH:MM:SS");
        return;
    }
    int y, mo, d, h, mi, s;
    if (sscanf(line.c_str() + 4, "%d-%d-%d %d:%d:%d", &y, &mo, &d, &h, &mi, &s) != 6) {
        Serial.println("[rtc] 형식이 틀렸습니다. 예: set 2026-11-13 21:30:00");
        return;
    }
    rtc.adjust(DateTime(y, mo, d, h, mi, s));
    Serial.println("[rtc] 시각을 맞췄습니다");
}

void setup() {
    Serial.begin(115200);
    delay(1500);
    Serial.printf("\n[test_rtc] %s\n", DRAINMETER_VERSION);

    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    scanI2C();

    rtcOk = rtc.begin(&Wire);
    if (!rtcOk) {
        Serial.println("[rtc] DS3231을 찾지 못했습니다");
        return;
    }
    // 전기를 아끼려고 32kHz 출력과 사각파 출력을 끈다 (나중에 알람 깨우기에 SQW 핀을 쓴다)
    rtc.disable32K();
    rtc.writeSqwPinMode(DS3231_OFF);

    if (rtc.lostPower()) {
        Serial.println("[rtc] 시각이 지워져 있습니다. set 명령으로 맞춰 주세요.");
    }
    Serial.println("명령: set YYYY-MM-DD HH:MM:SS");
}

void loop() {
    if (Serial.available()) {
        handleCommand(Serial.readStringUntil('\n'));
    }
    static uint32_t last = 0;
    if (millis() - last < 1000) {
        delay(5);
        return;
    }
    last = millis();
    if (!rtcOk) return;

    DateTime now = rtc.now();
    Serial.printf("[rtc] %04d-%02d-%02d %02d:%02d:%02d  칩 온도 %.2f°C\n",
                  now.year(), now.month(), now.day(),
                  now.hour(), now.minute(), now.second(),
                  rtc.getTemperature());
}
