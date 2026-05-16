#include "oled.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_W 128
#define SCREEN_H 64
#define OLED_ADDR 0x3C

static Adafruit_SSD1306 _dsp(SCREEN_W, SCREEN_H, &Wire, -1);

OledDisplay oled;

// 설정 항목 이름 목록
static const char *SETTING_NAMES[] = {
    "Bottom (sec)",
    "Zero   (sec)",
    "Top    (sec)",
    "Limit  (sec)",
    "OverCur  (A)",
    "AutoTimeout",
    "Slave ID",
    "Baudrate",
    "Auto Setup",
    "Factory Reset"
};
const int SETTING_COUNT = 10;

OledDisplay::OledDisplay() {}

void OledDisplay::begin() {
    Wire.setSDA(PIN_I2C_SDA);
    Wire.setSCL(PIN_I2C_SCL);
    Wire.begin();
    _dsp.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
    _dsp.setTextColor(SSD1306_WHITE);
    _dsp.clearDisplay();
    _dsp.display();
}

void OledDisplay::showNormal(float pos_sec) {
    _dsp.clearDisplay();

    // 상단 작은 글씨 라벨
    _dsp.setTextSize(1);
    _dsp.setCursor(38, 2);
    _dsp.print("Position (sec)");

    // 중앙 큰 글씨 위치값
    _dsp.setTextSize(3);
    char buf[16];
    if (pos_sec >= 0) {
        snprintf(buf, sizeof(buf), "%5.1f", pos_sec);
    } else {
        snprintf(buf, sizeof(buf), "%5.1f", pos_sec);
    }
    int16_t x1, y1; uint16_t w, h;
    _dsp.getTextBounds(buf, 0, 0, &x1, &y1, &w, &h);
    _dsp.setCursor((SCREEN_W - w) / 2, (SCREEN_H - h) / 2 + 4);
    _dsp.print(buf);

    _dsp.display();
}

void OledDisplay::showOvercurrentError(float current_a) {
    _dsp.clearDisplay();
    _dsp.setTextSize(1);

    _dsp.setCursor(10, 4);
    _dsp.print("!! OVERCURRENT !!");

    _dsp.setCursor(0, 20);
    _dsp.print("Current: ");
    char buf[12];
    snprintf(buf, sizeof(buf), "%.1f A", current_a);
    _dsp.print(buf);

    _dsp.setCursor(0, 36);
    _dsp.print("Press push button");
    _dsp.setCursor(0, 48);
    _dsp.print("to reset.");

    _dsp.display();
}

void OledDisplay::showSettings(const Settings &cfg, int selected_item,
                                float edit_val, bool editing) {
    _dsp.clearDisplay();
    _dsp.setTextSize(1);

    // 상단 타이틀
    _dsp.fillRect(0, 0, SCREEN_W, 10, SSD1306_WHITE);
    _dsp.setTextColor(SSD1306_BLACK);
    _dsp.setCursor(28, 1);
    _dsp.print("[ SETTINGS ]");
    _dsp.setTextColor(SSD1306_WHITE);

    // 항목 표시 (3개씩)
    int start = (selected_item / 3) * 3;
    for (int i = 0; i < 3 && (start + i) < SETTING_COUNT; i++) {
        int idx = start + i;
        int y   = 13 + i * 17;

        if (idx == selected_item) {
            _dsp.fillRect(0, y - 1, SCREEN_W, 15, SSD1306_WHITE);
            _dsp.setTextColor(SSD1306_BLACK);
        }

        _dsp.setCursor(2, y + 2);
        _dsp.print(SETTING_NAMES[idx]);

        // 현재 값 표시
        char vbuf[12];
        if (editing && idx == selected_item) {
            snprintf(vbuf, sizeof(vbuf), ">%.1f<", edit_val);
        } else {
            switch (idx) {
                case 0: snprintf(vbuf, sizeof(vbuf), "%.1f", cfg.bottom_sec < 0 ? 0 : cfg.bottom_sec); break;
                case 1: snprintf(vbuf, sizeof(vbuf), "%.1f", cfg.zero_sec   < 0 ? 0 : cfg.zero_sec);   break;
                case 2: snprintf(vbuf, sizeof(vbuf), "%.1f", cfg.top_sec    < 0 ? 0 : cfg.top_sec);    break;
                case 3: snprintf(vbuf, sizeof(vbuf), "%.1f", cfg.limit_sec <= 0 ? cfg.top_sec : cfg.limit_sec); break;
                case 4: snprintf(vbuf, sizeof(vbuf), "%.1f", cfg.overcurrent_a); break;
                case 5: snprintf(vbuf, sizeof(vbuf), "%lu",  cfg.auto_timeout_s); break;
                case 6: snprintf(vbuf, sizeof(vbuf), "%d",   cfg.slave_id); break;
                case 7: {
                    const uint32_t b[] = {9600,19200,38400,57600,115200};
                    snprintf(vbuf, sizeof(vbuf), "%lu", b[cfg.baud_code]); break;
                }
                case 8: snprintf(vbuf, sizeof(vbuf), ">>"); break;
                case 9: snprintf(vbuf, sizeof(vbuf), ">>"); break;
            }
        }
        _dsp.setCursor(80, y + 2);
        _dsp.print(vbuf);

        _dsp.setTextColor(SSD1306_WHITE);
    }

    // 스크롤 인디케이터
    if (SETTING_COUNT > 3) {
        int bar_h = (SCREEN_H - 12) * 3 / SETTING_COUNT;
        int bar_y = 12 + (SCREEN_H - 12) * start / SETTING_COUNT;
        _dsp.fillRect(SCREEN_W - 3, bar_y, 2, bar_h, SSD1306_WHITE);
    }

    _dsp.display();
}

void OledDisplay::showAutoSetup(int step, float elapsed_sec) {
    _dsp.clearDisplay();
    _dsp.setTextSize(1);

    _dsp.setCursor(10, 0);
    _dsp.print("[ AUTO SETUP ]");

    if (step == 0) {
        _dsp.setCursor(0, 20);
        _dsp.print("Finding bottom...");
        _dsp.setCursor(0, 36);
        _dsp.print("Reversing...");
    } else {
        _dsp.setCursor(0, 20);
        _dsp.print("Finding top...");
        _dsp.setCursor(0, 36);
        _dsp.print("Forwarding...");

        // 경과 시간 표시
        char buf[16];
        int m = (int)elapsed_sec / 60;
        int s = (int)elapsed_sec % 60;
        snprintf(buf, sizeof(buf), "Time: %02d:%02d", m, s);
        _dsp.setCursor(0, 50);
        _dsp.print(buf);
    }

    _dsp.display();
}

void OledDisplay::showBaudChanged() {
    _dsp.clearDisplay();
    _dsp.setTextSize(1);
    _dsp.setCursor(4, 10);
    _dsp.print("Baudrate changed.");
    _dsp.setCursor(4, 26);
    _dsp.print("Please restart");
    _dsp.setCursor(4, 38);
    _dsp.print("the device.");
    _dsp.display();
}

void OledDisplay::showNotConfigured(int selected_item) {
    _dsp.clearDisplay();
    _dsp.setTextSize(1);
    _dsp.setCursor(4, 0);
    _dsp.print("Setup required!");
    _dsp.setCursor(4, 14);
    _dsp.print("Please configure");
    _dsp.setCursor(4, 26);
    _dsp.print("all required");
    _dsp.setCursor(4, 38);
    _dsp.print("settings.");
    _dsp.setCursor(4, 52);
    char buf[20];
    snprintf(buf, sizeof(buf), "Item: %s", SETTING_NAMES[selected_item]);
    _dsp.print(buf);
    _dsp.display();
}
