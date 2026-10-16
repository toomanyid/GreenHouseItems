#include <Arduino.h>
#include "dev_Encoder.h"
#include "config.h"

static Encoder* s_enc = nullptr;
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static const uint32_t SW_DEBOUNCE_MS = 20;

// 이전 상태(2비트)와 지금 상태(2비트)로 방향을 정하는 표. 건너뛴 변화는 0
static const int8_t QUAD[16] = { 0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0 };

static void IRAM_ATTR encIsr() { if (s_enc) s_enc->onEdge(); }

void Encoder::begin(uint8_t pinA, uint8_t pinB, uint8_t pinSw) {
    _a = pinA; _b = pinB; _sw = pinSw;
    pinMode(_a, INPUT_PULLUP);
    pinMode(_b, INPUT_PULLUP);
    pinMode(_sw, INPUT_PULLUP);
    _state = (uint8_t)((digitalRead(_a) << 1) | digitalRead(_b));
    s_enc = this;
    attachInterrupt(digitalPinToInterrupt(_a), encIsr, CHANGE);
    attachInterrupt(digitalPinToInterrupt(_b), encIsr, CHANGE);
}

void IRAM_ATTR Encoder::onEdge() {
    uint8_t now = (uint8_t)((digitalRead(_a) << 1) | digitalRead(_b));
    portENTER_CRITICAL_ISR(&s_mux);
    int8_t d = QUAD[(_state << 2) | now];
    _state = now;
    _edgeCount++;
    if (d != 0) {
        _edges += d;
        if (_edges >= (int8_t)ENC_EDGES_PER_DETENT || _edges <= -(int8_t)ENC_EDGES_PER_DETENT) {
            uint8_t next = (uint8_t)((_head + 1) % RING);
            if (next != _tail) {                       // 가득 차면 버림
                _ring[_head].dir = _edges > 0 ? 1 : -1;
                _ring[_head].ms = (uint32_t)(esp_timer_get_time() / 1000);
                _head = next;
            }
            _edges = 0;
        }
    }
    portEXIT_CRITICAL_ISR(&s_mux);
}

uint8_t Encoder::take(EncStep* out, uint8_t max) {
    uint8_t n = 0;
    portENTER_CRITICAL(&s_mux);
    while (_tail != _head && n < max) {
        out[n++] = _ring[_tail];
        _tail = (uint8_t)((_tail + 1) % RING);
    }
    portEXIT_CRITICAL(&s_mux);
    return n;
}

bool Encoder::pressed() {
    bool raw = digitalRead(_sw) == LOW;
    uint32_t now = millis();
    if (raw != _swRaw) { _swRaw = raw; _swSince = now; }
    if (raw != _swValue && now - _swSince >= SW_DEBOUNCE_MS) _swValue = raw;
    return _swValue;
}
