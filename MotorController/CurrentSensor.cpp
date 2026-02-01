#include "CurrentSensor.h"

// ESP32C6: 12비트 ADC, 3.3V 기준
static const float ADC_MAX = 4095.0f;
static const float VREF = 3.3f;
static const float NOISE_THRESHOLD = 0.1f;  // A 이하는 0으로 처리

CurrentSensor::CurrentSensor()
  : pin(0), zeroCurrentVoltage(1.65f), sensitivity(0.040f) {
}

CurrentSensor& CurrentSensor::getInstance() {
  static CurrentSensor instance;
  return instance;
}

void CurrentSensor::init(int adcPin) {
  pin = adcPin;
  pinMode(pin, INPUT);
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

float CurrentSensor::getCurrent() {
  readSample();  // 첫 샘플 버림
  
  float sum = 0;
  for (int i = 0; i < 9; i++) {
    sum += readSample();
  }
  return sum / 9.0f;
}

void CurrentSensor::setSensitivity(float vPerA) {
  sensitivity = vPerA;
}

float CurrentSensor::getZeroVoltage() {
  return zeroCurrentVoltage;
}
