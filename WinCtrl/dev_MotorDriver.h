#pragma once
#include <stdint.h>

// ============================================================
//  1층: BTS7960 모터 드라이버 하나.
//  방향과 출력 천분율을 받아 PWM과 EN을 건다. 전류는 mA로 올린다.
//  방향 전환 1초 규칙을 여기에도 둔다 (스펙 4절). 2층이 잘못 불러도 막힌다.
// ============================================================
class MotorDriver {
public:
    void begin(uint8_t pinR, uint8_t pinL, uint8_t pinEn, uint8_t pinIs,
               uint8_t chR, uint8_t chL, int32_t senseOhm, uint32_t now);

    // dir: +1 상승, -1 하강, 0 끊기. duty: 0~1000
    void drive(int8_t dir, uint16_t duty, uint32_t now);
    void cut(uint32_t now) { drive(0, 0, now); }

    // 전류 감지 (R_IS + L_IS 묶음). 한 번 읽어 둔 값을 돌려준다
    void    sense();
    int32_t currentMa() const   { return _ma; }
    bool    driverFault() const { return _fault; }

private:
    void    apply(int8_t dir, uint16_t duty);

    uint8_t  _r = 0, _l = 0, _en = 0, _is = 0, _chR = 0, _chL = 0;
    int32_t  _ohm = 1000;
    int8_t   _dir = 0;          // 지금 걸린 방향
    uint16_t _duty = 0;         // 지금 걸린 출력 (UI 시험 빌드의 가짜 창이 씀)
    int8_t   _lastDir = 0;      // 마지막으로 구동한 방향
    uint32_t _stopMs = 0;       // 멈춘 시각
    int32_t  _ma = 0;
    bool     _fault = false;
};
