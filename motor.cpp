#include "motor.h"
#include "current_sensor.h"
#include <hardware/pwm.h>

Motor motor;

// RP2040 PWM 설정 헬퍼
static void rp2040_pwm_init(uint8_t pin, uint32_t freq_hz) {
    gpio_set_function(pin, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(pin);
    uint32_t clk = 125000000;
    uint32_t divider16 = clk / freq_hz / 4096 + (clk % (freq_hz * 4096) != 0);
    if (divider16 < 16) divider16 = 16;
    pwm_set_clkdiv_int_frac(slice, divider16 / 16, divider16 & 0xF);
    pwm_set_wrap(slice, 4095);
    pwm_set_enabled(slice, true);
}

static void rp2040_pwm_set(uint8_t pin, float duty) {
    uint slice   = pwm_gpio_to_slice_num(pin);
    uint channel = pwm_gpio_to_channel(pin);
    uint16_t level = (uint16_t)(duty * 4095.0f);
    pwm_set_chan_level(slice, channel, level);
}

Motor::Motor()
    : _state(MotorState::IDLE)
    , _dir(MotorDir::STOP)
    , _position_ds(0)
    , _target_ds(0)
    , _pos_frac(0)
    , _ramp_start_ms(0)
    , _last_update_ms(0)
    , _current_duty(0.0f)
{}

void Motor::begin() {
    pinMode(PIN_MOTOR_EN, OUTPUT);
    digitalWrite(PIN_MOTOR_EN, HIGH);

    rp2040_pwm_init(PIN_PWM_A, PWM_FREQ_HZ);
    rp2040_pwm_init(PIN_PWM_B, PWM_FREQ_HZ);
    rp2040_pwm_set(PIN_PWM_A, 0.0f);
    rp2040_pwm_set(PIN_PWM_B, 0.0f);

    _last_update_ms = millis();
}

void Motor::setPWM(uint8_t pin_a, uint8_t pin_b, float duty_a, float duty_b) {
    rp2040_pwm_set(pin_a, duty_a);
    rp2040_pwm_set(pin_b, duty_b);
}

void Motor::setDuty(MotorDir dir, float duty) {
    _current_duty = duty;
    switch (dir) {
        case MotorDir::FORWARD:
            setPWM(PIN_PWM_A, PIN_PWM_B, duty, 0.0f);
            break;
        case MotorDir::REVERSE:
            setPWM(PIN_PWM_A, PIN_PWM_B, 0.0f, duty);
            break;
        case MotorDir::STOP:
        default:
            setPWM(PIN_PWM_A, PIN_PWM_B, 0.0f, 0.0f);
            break;
    }
}

bool Motor::checkOvercurrent(float threshold_a) {
    if (currentSensor.isOvercurrent(threshold_a)) {
        emergencyStop();
        _state = MotorState::OVERCURRENT_ERROR;
        return true;
    }
    return false;
}

void Motor::moveTo(float target_sec, float overcurrent_a) {
    if (_state == MotorState::OVERCURRENT_ERROR) return;

    _target_ds = (int32_t)roundf(target_sec * 10.0f);

    if (abs(_target_ds - _position_ds) < 1) return;  // 이미 목표 위치 (0.1초 이내)

    MotorDir newDir = (_target_ds > _position_ds) ? MotorDir::FORWARD : MotorDir::REVERSE;

    // 방향 전환이면 먼저 정지
    if (isMoving() && newDir != _dir) {
        stop();
        return;  // 다음 update()에서 정지 완료 후 재시작
    }

    _dir           = newDir;
    _state         = MotorState::RAMPING_UP;
    _ramp_start_ms = millis();
}

void Motor::stop() {
    if (!isMoving()) return;
    _state         = MotorState::RAMPING_DOWN;
    _ramp_start_ms = millis();
}

void Motor::emergencyStop() {
    setDuty(MotorDir::STOP, 0.0f);
    _dir      = MotorDir::STOP;
    _state    = MotorState::IDLE;
    _pos_frac = 0;
}

void Motor::update(float overcurrent_a) {
    uint32_t now    = millis();
    int32_t  dt_ms  = (int32_t)(now - _last_update_ms);
    _last_update_ms = now;

    // 위치 누적 (이동 중일 때)
    // _pos_frac 단위: dt_ms × duty (100 누적 = 1 ds = 0.1초)
    if (_current_duty > 0.01f && _dir != MotorDir::STOP) {
        _pos_frac += (int32_t)(dt_ms * _current_duty);
        if (_pos_frac >= 100) {
            int32_t ds = _pos_frac / 100;
            _pos_frac %= 100;
            if (_dir == MotorDir::FORWARD) _position_ds += ds;
            else                           _position_ds -= ds;
        }
    } else {
        _pos_frac = 0;
    }

    switch (_state) {

        case MotorState::RAMPING_UP: {
            if (checkOvercurrent(overcurrent_a)) return;

            // 램프 중 목표 도달 시 바로 소프트스톱
            bool arrived = (_dir == MotorDir::FORWARD)
                           ? (_position_ds >= _target_ds)
                           : (_position_ds <= _target_ds);
            if (arrived) {
                stop();
                break;
            }

            uint32_t elapsed = now - _ramp_start_ms;
            float    duty    = (float)elapsed / RAMP_TIME_MS;
            if (duty >= 1.0f) {
                duty   = 1.0f;
                _state = MotorState::RUNNING;
            }
            setDuty(_dir, duty);
            break;
        }

        case MotorState::RUNNING: {
            // 과전류 체크
            if (checkOvercurrent(overcurrent_a)) return;

            // 목표 위치 도달 확인
            bool arrived = (_dir == MotorDir::FORWARD)
                           ? (_position_ds >= _target_ds)
                           : (_position_ds <= _target_ds);
            if (arrived) {
                stop();
            }
            break;
        }

        case MotorState::RAMPING_DOWN: {
            // 과전류 체크 (램프 중도 예외 없음)
            if (checkOvercurrent(overcurrent_a)) return;

            uint32_t elapsed = now - _ramp_start_ms;
            float    duty    = 1.0f - (float)elapsed / RAMP_TIME_MS;
            if (duty <= 0.0f) {
                duty      = 0.0f;
                _state    = MotorState::IDLE;
                _dir      = MotorDir::STOP;
                _pos_frac = 0;
            }
            setDuty(_dir, duty);
            break;
        }

        case MotorState::IDLE: {
            setDuty(MotorDir::STOP, 0.0f);
            // 방향 전환 후 정지 완료 시 남은 목표로 재이동
            if (abs(_target_ds - _position_ds) >= 1) {
                _pos_frac      = 0;
                _dir           = (_target_ds > _position_ds) ? MotorDir::FORWARD : MotorDir::REVERSE;
                _state         = MotorState::RAMPING_UP;
                _ramp_start_ms = millis();
            }
            break;
        }

        case MotorState::OVERCURRENT_ERROR:
        default:
            setDuty(MotorDir::STOP, 0.0f);
            break;
    }
}
