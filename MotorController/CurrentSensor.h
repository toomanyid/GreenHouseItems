#ifndef CURRENT_SENSOR_H
#define CURRENT_SENSOR_H

#include <Arduino.h>

#define CURRENT_SENSOR_SAMPLE_INTERVAL_MS  250   // 초당 4번 (250ms)
#define CURRENT_SENSOR_SAMPLES_PER_READ   16    // 한 번에 16개 샘플 평균

// 전류 초과 콜백 함수 타입 (current: 초과된 전류값 A)
typedef void (*OverCurrentCallback)(float current);

// ACS758 등 전류 센서 클래스 (ADC 기반) - 싱글톤, 별도 스레드에서 초당 4회 샘플링
class CurrentSensor {
private:
  int pin;
  float zeroCurrentVoltage;   // 제로 전류 전압 (V)
  float sensitivity;           // 감도 (V/A), ACS758-50A: 0.040
  
  volatile float currentAverage;  // 현재 전류값 (A), getCurrent()가 반환 (버퍼 없음)
  float maxCurrent;            // 최대 허용 전류 (A), 초과 시 콜백 호출
  OverCurrentCallback overCurrentCallback;  // 전류 초과 콜백 함수
  
  bool taskRunning;
  
  CurrentSensor();
  float readSample();         // 단일 샘플 읽기 (A)
  float readAverage();        // 16개 샘플 읽고 평균 (A)
  static void taskFunction(void* param);
  void taskLoop();            // 250ms마다 16개 샘플 읽고 평균 계산 후 바로 저장
  
  CurrentSensor(const CurrentSensor&) = delete;
  void operator=(const CurrentSensor&) = delete;

public:
  static CurrentSensor& getInstance();
  
  void init(int adcPin);      // 핀 설정 (캘리브 후 startTask() 호출 권장)
  void startTask();           // 별도 스레드 시작 (calibrate() 이후 호출)
  void calibrate();           // 제로 전류 보정 (전류 0 가정, 메인 스레드에서 호출)
  float getCurrent();         // 현재 전류 반환 (A)
  
  void setSensitivity(float vPerA);  // 감도 설정 (V/A)
  float getZeroVoltage();             // 현재 제로 전압 반환
  
  // 전류 초과 콜백 설정
  void setMaxCurrent(float maxA);                    // 최대 허용 전류 설정 (A)
  void setOverCurrentCallback(OverCurrentCallback cb); // 콜백 함수 등록
  float getMaxCurrent();                             // 현재 최대 전류 반환
};

#endif
