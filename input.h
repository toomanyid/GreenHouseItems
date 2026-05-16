#pragma once
#include <Arduino.h>
#include "config.h"

enum class ButtonEvent {
    NONE,
    SHORT_PRESS,
    LONG_PRESS
};

class InputHandler {
public:
    InputHandler();
    void begin();
    void update();

    // 엔코더
    int  getEncoderDelta();   // 누적 변화량 반환 후 초기화

    // 버튼 이벤트
    ButtonEvent getEncSwEvent();
    bool        isSwUpPressed();
    bool        isSwDownPressed();
    bool        isSwBothPressed();  // 상+하 동시

private:
    // 엔코더
    int      _enc_delta;
    uint8_t  _enc_last;

    // 버튼 디바운스
    struct BtnState {
        bool     raw;
        bool     stable;
        uint32_t last_change_ms;
        bool     prev_stable;
    };

    BtnState _enc_sw;
    BtnState _sw_up;
    BtnState _sw_down;

    uint32_t _enc_sw_press_ms;   // 길게 누름 측정용
    bool     _long_press_fired;

    ButtonEvent _enc_sw_event;

    void debounce(BtnState &btn, bool raw_active);
};

extern InputHandler input;
