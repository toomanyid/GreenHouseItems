#ifndef CURRENT_SENSOR_H
#define CURRENT_SENSOR_H

#include <Arduino.h>

// ACS758 등 전류 센서 클래스 (ADC 기반)
class CurrentSensor {
private:
  int pin;
  float zeroCurrentVoltage;   // 제로 전류 전압 (V)
  float sensitivity;          // 감도 (V/A), ACS758-50A: 0.040
  
  float readSample();         // 단일 샘플 읽기 (A)

public:
  CurrentSensor(int adcPin);
  
  void init();                // 핀 설정
  void calibrate();           // 제로 전류 보정 (전류 0 가정)
  float getCurrent();        // 전류 읽기: 10회 중 첫 회 버리고 9회 평균 (A)
  
  void setSensitivity(float vPerA);  // 감도 설정 (V/A)
  float getZeroVoltage();            // 현재 제로 전압 반환
};

#endif
