#include "dev_Compat.h"
#include "dev_MotorDriver.h"
#include "config.h"

void MotorDriver::begin(uint8_t pinR, uint8_t pinL, uint8_t pinEn, uint8_t pinIs,
                        uint8_t chR, uint8_t chL, int32_t senseOhm, uint32_t now) {
    _r = pinR; _l = pinL; _en = pinEn; _is = pinIs; _chR = chR; _chL = chL;
    _ohm = senseOhm > 0 ? senseOhm : 1000;
    digitalWrite(_en, LOW);
    pinMode(_en, OUTPUT);
    pwmAttach(_r, _chR, PWM_FREQ_HZ, PWM_BITS);
    pwmAttach(_l, _chL, PWM_FREQ_HZ, PWM_BITS);
    pwmWrite(_r, _chR, 0);
    pwmWrite(_l, _chL, 0);
    analogSetPinAttenuation(_is, ADC_11db);
    _dir = 0;
    _lastDir = 0;
    _stopMs = now;              // 전원을 켠 뒤 첫 구동 전에도 1초
}

void MotorDriver::apply(int8_t dir, uint16_t duty) {
    if (duty > DUTY_FULL) duty = DUTY_FULL;
    uint32_t raw = (uint32_t)duty * ((1u << PWM_BITS) - 1) / DUTY_FULL;
    if (dir == 0 || duty == 0) {
        digitalWrite(_en, LOW);
        pwmWrite(_r, _chR, 0);
        pwmWrite(_l, _chL, 0);
        return;
    }
    // 반대쪽을 먼저 0으로
    if (dir > 0) { pwmWrite(_l, _chL, 0); pwmWrite(_r, _chR, raw); }
    else         { pwmWrite(_r, _chR, 0); pwmWrite(_l, _chL, raw); }
    digitalWrite(_en, HIGH);
}

void MotorDriver::drive(int8_t dir, uint16_t duty, uint32_t now) {
    if (dir != 0 && duty > 0) {
        if (_dir == 0) {
            bool reverse = _lastDir != 0 && dir != _lastDir;
            bool first = _lastDir == 0;
            if ((reverse || first) && now - _stopMs < REVERSE_WAIT_MS) dir = 0;   // 1초가 안 됨: 받지 않음
        } else if (dir != _dir) {
            dir = 0;            // 구동 중 반대 방향: 끊기만 한다
        }
    }
    if (dir == 0 || duty == 0) {
        if (_dir != 0) _stopMs = now;
        _dir = 0;
        apply(0, 0);
        return;
    }
    _dir = dir;
    _lastDir = dir;
    apply(dir, duty);
}

void MotorDriver::sense() {
    int32_t mv = readMv(_is, 4);
    _fault = mv >= DRIVER_FAULT_MV;
    // 부하 전류 = IS 전류 × KILIS, IS 전류 = 전압 ÷ 감지 저항
    _ma = (int32_t)((int64_t)mv * BTS_KILIS / _ohm);
}
