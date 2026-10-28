#include <Arduino.h>
#include "dev_Sensor.h"
#ifdef WINCTRL_UISIM
#include "dev_Sim.h"
#include "pins.h"
#endif

void HomeSensor::begin(uint8_t pin, uint32_t now) {
    _pin = pin;
    pinMode(_pin, INPUT);        // 보드에 10kΩ 풀업이 있다
    _f.begin(now);
}

SensorState HomeSensor::update(uint32_t now) {
#ifdef WINCTRL_UISIM
    return _f.update(simWindow(_pin == PIN_S1 ? 0 : 1).sensorOn(), now);
#else
    return _f.update(digitalRead(_pin) == LOW, now);
#endif
}
