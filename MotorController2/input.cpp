#include "input.h"

InputHandler input;

InputHandler::InputHandler()
    : _enc_delta(0), _enc_last(0)
    , _enc_sw_press_ms(0), _long_press_fired(false)
    , _enc_sw_event(ButtonEvent::NONE)
{
    memset(&_enc_sw,   0, sizeof(_enc_sw));
    memset(&_sw_up,    0, sizeof(_sw_up));
    memset(&_sw_down,  0, sizeof(_sw_down));
}

void InputHandler::begin() {
    pinMode(PIN_ENC_A,  INPUT_PULLUP);
    pinMode(PIN_ENC_B,  INPUT_PULLUP);
    pinMode(PIN_ENC_SW, INPUT_PULLUP);
    pinMode(PIN_SW_UP,  INPUT_PULLUP);
    pinMode(PIN_SW_DOWN,INPUT_PULLUP);

    _enc_last = (digitalRead(PIN_ENC_A) << 1) | digitalRead(PIN_ENC_B);
}

void InputHandler::debounce(BtnState &btn, bool raw_active) {
    uint32_t now = millis();
    btn.prev_stable = btn.stable;

    if (raw_active != btn.raw) {
        btn.raw            = raw_active;
        btn.last_change_ms = now;
    }
    if ((now - btn.last_change_ms) >= DEBOUNCE_MS) {
        btn.stable = btn.raw;
    }
}

void InputHandler::update() {
    uint32_t now = millis();

    // 엔코더 A/B 읽기 (풀업 → 눌리면 LOW → 반전)
    uint8_t enc_a = !digitalRead(PIN_ENC_A);
    uint8_t enc_b = !digitalRead(PIN_ENC_B);
    uint8_t enc_now = (enc_a << 1) | enc_b;

    // 그레이 코드 → 방향 판별
    static const int8_t ENC_TABLE[16] = {
         0, -1,  1,  0,
         1,  0,  0, -1,
        -1,  0,  0,  1,
         0,  1, -1,  0
    };
    uint8_t idx = (_enc_last << 2) | enc_now;
    _enc_delta += ENC_TABLE[idx & 0x0F];
    _enc_last   = enc_now;

    // 버튼 디바운스 (풀업 → 눌리면 LOW)
    debounce(_enc_sw,  !digitalRead(PIN_ENC_SW));
    debounce(_sw_up,   !digitalRead(PIN_SW_UP));
    debounce(_sw_down, !digitalRead(PIN_SW_DOWN));

    // 엔코더 푸시 이벤트 처리
    _enc_sw_event = ButtonEvent::NONE;

    if (_enc_sw.stable && !_enc_sw.prev_stable) {
        // 눌림 시작
        _enc_sw_press_ms  = now;
        _long_press_fired = false;
    }

    if (_enc_sw.stable && !_long_press_fired) {
        if ((now - _enc_sw_press_ms) >= LONG_PRESS_MS) {
            _enc_sw_event     = ButtonEvent::LONG_PRESS;
            _long_press_fired = true;
        }
    }

    if (!_enc_sw.stable && _enc_sw.prev_stable) {
        // 놓임
        if (!_long_press_fired) {
            _enc_sw_event = ButtonEvent::SHORT_PRESS;
        }
        _long_press_fired = false;
    }
}

int InputHandler::getEncoderDelta() {
    int d      = _enc_delta;
    _enc_delta = 0;
    return d;
}

ButtonEvent InputHandler::getEncSwEvent() {
    ButtonEvent ev = _enc_sw_event;
    _enc_sw_event  = ButtonEvent::NONE;
    return ev;
}

bool InputHandler::isSwUpPressed() {
    return _sw_up.stable && !_sw_up.prev_stable;
}

bool InputHandler::isSwDownPressed() {
    return _sw_down.stable && !_sw_down.prev_stable;
}

bool InputHandler::isSwBothPressed() {
    return _sw_up.stable && _sw_down.stable;
}
