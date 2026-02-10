#include "CurrentSensor.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// ESP32C6: 12비트 ADC, 3.3V 기준
static const float ADC_MAX = 4095.0f;
static const float VREF = 3.3f;
static const float NOISE_THRESHOLD = 0.1f;  // A 이하는 0으로 처리

CurrentSensor::CurrentSensor()
  : pin(0), zeroCurrentVoltage(1.65f), sensitivity(0.040f),
    currentAverage(0.0f), maxCurrent(10.0f), overCurrentCallback(nullptr),
    taskRunning(false) {
}

CurrentSensor& CurrentSensor::getInstance() {
  static CurrentSensor instance;
  return instance;
}

void CurrentSensor::init(int adcPin) {
  pin = adcPin;
  pinMode(pin, INPUT);
  currentAverage = 0.0f;
}

void CurrentSensor::startTask() {
  if (taskRunning) return;
  // 코어 0 명시 (ESP32-C6 단일코어: 0만 유효, xTaskCreate는 라이브러리에서 core 1 쓰며 assert 유발)
  xTaskCreatePinnedToCore(
    taskFunction,
    "CurrentTask",
    2048,
    this,
    1,
    NULL,
    0
  );
  taskRunning = true;
}

void CurrentSensor::calibrate() {
  float sum = 0;
  const int samples = 100;

  for (int i = 0; i < samples; i++) {
    sum += analogRead(pin);
    delay(10);
  }

  float avgReading = sum / samples;
  zeroCurrentVoltage = (avgReading / ADC_MAX) * VREF;
  Serial.print("제로 전류 전압 보정: ");
  Serial.print(zeroCurrentVoltage, 3);
  Serial.println("V");
}

float CurrentSensor::readSample() {
  int reading = analogRead(pin);
  float voltage = (reading / ADC_MAX) * VREF;
  float current = (voltage - zeroCurrentVoltage) / sensitivity;

  if (abs(current) < NOISE_THRESHOLD) {
    current = 0.0f;
  }
  return current;
}

void CurrentSensor::taskFunction(void* param) {
  CurrentSensor* self = (CurrentSensor*)param;
  self->taskLoop();
}

float CurrentSensor::readAverage() {
  float sum = 0;
  for (int i = 0; i < CURRENT_SENSOR_SAMPLES_PER_READ; i++) {
    sum += readSample();
  }
  return sum / (float)CURRENT_SENSOR_SAMPLES_PER_READ;
}

void CurrentSensor::taskLoop() {
  const TickType_t intervalMs = CURRENT_SENSOR_SAMPLE_INTERVAL_MS;
  for (;;) {
    // 16개 샘플 읽고 평균을 바로 저장 (버퍼 없음)
    currentAverage = readAverage();
    
    // 전류 초과 체크 및 콜백 호출
    if (overCurrentCallback != nullptr && fabsf(currentAverage) > maxCurrent) {
      overCurrentCallback(currentAverage);
    }

    vTaskDelay(pdMS_TO_TICKS(intervalMs));
  }
}

float CurrentSensor::getCurrent() {
  return currentAverage;
}

void CurrentSensor::setSensitivity(float vPerA) {
  sensitivity = vPerA;
}

float CurrentSensor::getZeroVoltage() {
  return zeroCurrentVoltage;
}

void CurrentSensor::setMaxCurrent(float maxA) {
  maxCurrent = maxA;
}

void CurrentSensor::setOverCurrentCallback(OverCurrentCallback cb) {
  overCurrentCallback = cb;
}

float CurrentSensor::getMaxCurrent() {
  return maxCurrent;
}
