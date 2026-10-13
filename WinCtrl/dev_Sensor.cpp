#include <Arduino.h>
#include "dev_Sensor.h"

void HomeSensor::begin(uint8_t pin, uint32_t now) {
    _pin = pin;
    pinMode(_pin, INPUT);        // 보드에 10kΩ 풀업이 있다
    _f.begin(now);
}

SensorState HomeSensor::update(uint32_t now) {
    return _f.update(digitalRead(_pin) == LOW, now);
}
