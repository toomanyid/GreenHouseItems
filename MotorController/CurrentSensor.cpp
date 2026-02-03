#include "CurrentSensor.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// ESP32C6: 12비트 ADC, 3.3V 기준
static const float ADC_MAX = 4095.0f;
static const float VREF = 3.3f;
static const float NOISE_THRESHOLD = 0.1f;  // A 이하는 0으로 처리

CurrentSensor::CurrentSensor()
  : pin(0), zeroCurrentVoltage(1.65f), sensitivity(0.040f),
    sampleIndex(0), sampleCount(0), sampleSum(0), currentAverage(0.0f), taskRunning(false) {
  for (int i = 0; i < CURRENT_SENSOR_BUFFER_SIZE; i++) sampleBuffer[i] = 0.0f;
}

CurrentSensor& CurrentSensor::getInstance() {
  static CurrentSensor instance;
  return instance;
}

void CurrentSensor::init(int adcPin) {
  pin = adcPin;
  pinMode(pin, INPUT);
  sampleIndex = 0;
  sampleCount = 0;
  sampleSum = 0;
  currentAverage = 0.0f;
  for (int i = 0; i < CURRENT_SENSOR_BUFFER_SIZE; i++) sampleBuffer[i] = 0.0f;
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

void CurrentSensor::taskLoop() {
  const TickType_t intervalMs = CURRENT_SENSOR_SAMPLE_INTERVAL_MS;
  for (;;) {
    float s = readSample();

    if (sampleCount < CURRENT_SENSOR_BUFFER_SIZE) {
      sampleBuffer[sampleIndex] = s;
      sampleSum += s;
      sampleIndex = (sampleIndex + 1) % CURRENT_SENSOR_BUFFER_SIZE;
      sampleCount++;
    } else {
      sampleSum -= sampleBuffer[sampleIndex];
      sampleBuffer[sampleIndex] = s;
      sampleSum += s;
      sampleIndex = (sampleIndex + 1) % CURRENT_SENSOR_BUFFER_SIZE;
    }

    currentAverage = (sampleCount > 0) ? (sampleSum / (float)sampleCount) : 0.0f;

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
