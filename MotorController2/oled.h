#pragma once
#include <Arduino.h>
#include "flash_storage.h"

enum class Screen {
    NORMAL,
    SETTINGS,
    OVERCURRENT_ERROR,
    AUTO_SETUP,
    BAUD_CHANGED
};

class OledDisplay {
public:
    OledDisplay();
    void begin();

    // 일반 화면: 현재 위치(초) 큰 글씨
    void showNormal(float pos_sec);

    // 과전류 에러 화면
    void showOvercurrentError(float current_a);

    // 설정 화면
    void showSettings(const Settings &cfg, int selected_item, float edit_val, bool editing);

    // 자동 설정 화면
    void showAutoSetup(int step, float elapsed_sec);

    // 보레이트 변경 안내
    void showBaudChanged();

    // 미설정 안내
    void showNotConfigured(int selected_item);

private:
    void clear();
    void drawBigNumber(float val);
    void setCursor(int x, int y);
    void print(const char *str);
    void println(const char *str);
    void setTextSize(int size);
    void setInverted(bool inv);
    void display();
};

extern OledDisplay oled;
