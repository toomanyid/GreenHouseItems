#pragma once
#include <Arduino.h>
#include "config.h"

enum class MotorDir { STOP, FORWARD, REVERSE };
enum class MotorState {
    IDLE,
    RAMPING_UP,
    RUNNING,
    RAMPING_DOWN,
    OVERCURRENT_ERROR
};

class Motor {
public:
    Motor();
    void begin();

    // 명령
    void moveTo(float target_sec, float overcurrent_a);  // 시간 기반 목표 이동
    void stop();                                          // 즉시 소프트스톱 시작
    void emergencyStop();                                 // 즉시 강제 정지

    // 루프에서 매번 호출
    void update(float overcurrent_a);

    // 상태 조회
    MotorState  getState()       const { return _state; }
    MotorDir    getDir()         const { return _dir; }
    float       getPositionSec() const { return _position_sec; }
    bool        isMoving()       const { return _state == MotorState::RAMPING_UP
                                              || _state == MotorState::RUNNING
                                              || _state == MotorState::RAMPING_DOWN; }
    bool        isError()        const { return _state == MotorState::OVERCURRENT_ERROR; }
    void        clearError()           { if (_state == MotorState::OVERCURRENT_ERROR)
                                             _state = MotorState::IDLE; }

    // 위치 강제 설정 (자동 설정에서 사용)
    void setPosition(float sec) { _position_sec = sec; }

private:
    MotorState  _state;
    MotorDir    _dir;
    float       _position_sec;    // 현재 위치 (초)
    float       _target_sec;      // 목표 위치 (초)

    uint32_t    _ramp_start_ms;
    uint32_t    _ramp_step;
    uint32_t    _last_update_ms;

    float       _current_duty;    // 0.0 ~ 1.0

    void        setDuty(MotorDir dir, float duty);
    void        setPWM(uint8_t pin_a, uint8_t pin_b, float duty_a, float duty_b);
    bool        checkOvercurrent(float threshold_a);
};

extern Motor motor;
