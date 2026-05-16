#pragma once
#include <Arduino.h>
#include "config.h"

class CurrentSensor {
public:
    CurrentSensor();
    void begin();

    // 현재 전류 읽기 (A)
    float readAmps();

    // 전류가 없는 상태인지 (리밋스위치 차단 감지용)
    bool isZero();

    // 과전류 여부 (임계값과 비교)
    bool isOvercurrent(float threshold_a);

private:
    float   _baseline;   // 보정된 무전류 기준 ADC값
    void    calibrate();
};

extern CurrentSensor currentSensor;
