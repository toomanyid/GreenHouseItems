#ifndef CURRENT_SENSOR_H
#define CURRENT_SENSOR_H

#include <Arduino.h>

#define CURRENT_SENSOR_SAMPLE_INTERVAL_MS  100   // 1초에 10번
#define CURRENT_SENSOR_BUFFER_SIZE        5     // 최근 5회 평균

// ACS758 등 전류 센서 클래스 (ADC 기반) - 싱글톤, 별도 스레드에서 10Hz 샘플링
class CurrentSensor {
private:
  int pin;
  float zeroCurrentVoltage;   // 제로 전류 전압 (V)
  float sensitivity;           // 감도 (V/A), ACS758-50A: 0.040
  
  float sampleBuffer[CURRENT_SENSOR_BUFFER_SIZE];
  uint8_t sampleIndex;        // 다음 쓰기 위치 (원형 버퍼)
  uint8_t sampleCount;        // 채워진 개수 (최대 5)
  float sampleSum;            // 버퍼 합 (평균 계산용)
  volatile float currentAverage;  // 최근 5회 평균 (A), getCurrent()가 반환
  
  bool taskRunning;
  
  CurrentSensor();
  float readSample();         // 단일 샘플 읽기 (A)
  static void taskFunction(void* param);
  void taskLoop();            // 100ms마다 1회 샘플, 평균 갱신
  
  CurrentSensor(const CurrentSensor&) = delete;
  void operator=(const CurrentSensor&) = delete;

public:
  static CurrentSensor& getInstance();
  
  void init(int adcPin);      // 핀 설정 (캘리브 후 startTask() 호출 권장)
  void startTask();           // 별도 스레드 시작 (calibrate() 이후 호출)
  void calibrate();           // 제로 전류 보정 (전류 0 가정, 메인 스레드에서 호출)
  float getCurrent();         // 최근 5회 평균 전류 반환 (A)
  
  void setSensitivity(float vPerA);  // 감도 설정 (V/A)
  float getZeroVoltage();             // 현재 제로 전압 반환
};

#endif
