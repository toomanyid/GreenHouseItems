#include "current_sensor.h"

CurrentSensor currentSensor;

CurrentSensor::CurrentSensor() : _baseline(0) {}

void CurrentSensor::begin() {
    analogReadResolution(12);
    calibrate();
}

void CurrentSensor::calibrate() {
    // 시작 시 무전류 기준값 평균 측정 (모터 정지 상태에서 호출)
    long sum = 0;
    const int N = 64;
    for (int i = 0; i < N; i++) {
        sum += analogRead(PIN_ACS758);
        delay(1);
    }
    _baseline = (float)sum / N;
}

float CurrentSensor::readAmps() {
    // 여러 샘플 평균으로 노이즈 감소
    long sum = 0;
    const int N = 16;
    for (int i = 0; i < N; i++) {
        sum += analogRead(PIN_ACS758);
    }
    float adc = (float)sum / N;

    // ADC값 → 전압 → 전류 변환
    float voltage = (adc / ADC_RESOLUTION) * ADC_VREF;
    float baseline_v = (_baseline / ADC_RESOLUTION) * ADC_VREF;
    float current = (voltage - baseline_v) / ACS758_SENS;

    return fabsf(current);  // 절댓값 반환 (방향 무관)
}

bool CurrentSensor::isZero() {
    return readAmps() < CURRENT_ZERO_THR;
}

bool CurrentSensor::isOvercurrent(float threshold_a) {
    return readAmps() > threshold_a;
}
