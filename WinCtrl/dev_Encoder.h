#pragma once
#include <stdint.h>

// ============================================================
//  1층: 누름 스위치가 있는 엔코더 (PEC16-4220F-S0024).
//  회전은 핀 인터럽트로 세고, 한 칸(4번 바뀜)마다 방향과 시각을 쌓아 둔다.
//  화면 태스크가 take()로 가져간다. 스위치는 화면 태스크가 주기마다 읽는다.
// ============================================================
struct EncStep {
    int8_t   dir;      // +1 시계 방향, -1 반대
    uint32_t ms;
};

class Encoder {
public:
    void    begin(uint8_t pinA, uint8_t pinB, uint8_t pinSw);
    uint8_t take(EncStep* out, uint8_t max);   // 쌓인 칸을 꺼냄
    bool    pressed();                          // 스위치 (떨림 거른 값)

    void    onEdge();                           // 인터럽트에서만 부름
private:
    static const uint8_t RING = 32;
    uint8_t  _a = 0, _b = 0, _sw = 0;
    volatile uint8_t  _state = 0;
    volatile int8_t   _edges = 0;
    volatile uint8_t  _head = 0, _tail = 0;
    EncStep  _ring[RING];
    bool     _swRaw = false, _swValue = false;
    uint32_t _swSince = 0;
};
