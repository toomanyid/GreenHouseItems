#include <Arduino.h>
#include "dev_Sensor.h"

void HomeSensor::begin(uint8_t pinNo, uint8_t pinNc, uint32_t now) {
    _no = pinNo;
    _nc = pinNc;
    pinMode(_no, INPUT_PULLUP);
    pinMode(_nc, INPUT_PULLUP);
    _f.begin(now);
}

SensorState HomeSensor::update(uint32_t now) {
    bool noClosed = digitalRead(_no) == LOW;
    bool ncClosed = digitalRead(_nc) == LOW;
    return _f.update(noClosed, ncClosed, now);
}
