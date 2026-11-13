// ============================================================
//  부품 시험: 전자잉크 2.9인치 흑백 (296x128)
//  pio run -e test_epd -t upload, 그다음 pio device monitor
//
//  1) 전체 갱신으로 예시 화면(한글 포함)을 그린다
//  2) 오른쪽 아래 숫자만 부분 갱신으로 5번 바꾼다
//  3) 걸린 시간을 시리얼에 찍고 패널을 재운다
//  다시 해 보려면 r 엔터.
//
//  화면이 깨지거나 아무것도 안 나오면 config.h의 EPD_DRIVER를 바꾼다 (README 표).
// ============================================================
#include <Arduino.h>
#include <SPI.h>
#include <GxEPD2_BW.h>
#include <U8g2_for_Adafruit_GFX.h>
#include "pins.h"
#include "config.h"

static GxEPD2_BW<EPD_DRIVER, EPD_DRIVER::HEIGHT> display(
    EPD_DRIVER(PIN_EPD_CS, PIN_EPD_DC, PIN_EPD_RST, PIN_EPD_BUSY));
static U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;

static void setText(const uint8_t* font) {
    u8g2Fonts.setFont(font);
    u8g2Fonts.setFontMode(1);
    u8g2Fonts.setForegroundColor(GxEPD_BLACK);
    u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
}

static void textAt(int16_t x, int16_t y, const char* s) {
    u8g2Fonts.setCursor(x, y);
    u8g2Fonts.print(s);
}

// 예시 화면: 실제 값 대신 고정된 숫자
static void drawSample() {
    display.fillScreen(GxEPD_WHITE);

    setText(u8g2_font_unifont_t_korean2);
    textAt(4, 16, "오늘 배액");
    textAt(204, 16, "11-13 21:30");

    setText(u8g2_font_logisoso32_tr);
    textAt(4, 58, "32.5 L");

    display.drawFastHLine(0, 66, display.width(), GxEPD_BLACK);

    setText(u8g2_font_unifont_t_korean2);
    textAt(4, 84, "최근 1회  5.4L / 27%");
    textAt(4, 104, "EC 2.10   pH 6.20");
    textAt(4, 124, "전지 4.41V");
}

static void drawCounter(int n) {
    display.fillScreen(GxEPD_WHITE);
    setText(u8g2_font_unifont_t_korean2);
    char buf[24];
    snprintf(buf, sizeof(buf), "갱신 %d", n);
    textAt(204, 124, buf);
}

static void runTest() {
    uint32_t t0 = millis();
    display.setFullWindow();
    display.firstPage();
    do {
        drawSample();
    } while (display.nextPage());
    Serial.printf("[epd] 전체 갱신 %lums\n", (unsigned long)(millis() - t0));

    for (int i = 1; i <= 5; i++) {
        delay(2000);
        t0 = millis();
        display.setPartialWindow(200, 108, 96, 20);
        display.firstPage();
        do {
            drawCounter(i);
        } while (display.nextPage());
        Serial.printf("[epd] 부분 갱신 %d: %lums\n", i, (unsigned long)(millis() - t0));
    }

    display.hibernate();   // 패널을 재운다. 화면은 그대로 남는다.
    Serial.println("[epd] 끝. 다시 하려면 r 엔터");
}

void setup() {
    Serial.begin(115200);
    delay(1500);
    Serial.printf("\n[test_epd] %s\n", DRAINMETER_VERSION);

    // 읽기선(MISO)은 쓰지 않는다. D9는 BUSY로 쓴다.
    SPI.begin(PIN_EPD_SCK, -1, PIN_EPD_DIN, -1);
    display.init(115200, true, 2, false);
    display.setRotation(1);   // 가로로 놓고 쓴다 (296x128)
    u8g2Fonts.begin(display);

    runTest();
}

void loop() {
    if (Serial.available()) {
        String line = Serial.readStringUntil('\n');
        line.trim();
        if (line == "r") {
            display.init(115200, false, 2, false);
            runTest();
        }
    }
    delay(10);
}
