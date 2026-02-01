#include "MotorDriver.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

MotorDriver::MotorDriver()
  : enablePin(0), pwmCwPin(0), pwmCcwPin(0),
    targetSpeed(0), currentActualSpeed(0), currentCmd(MOTOR_CMD_STOP), 
    lastCmd(MOTOR_CMD_STOP), running(false), taskRunning(false),
    softStartActive(false), softStartStartTime(0), softStartDuration(1000),
    positionMs(0), lastPositionUpdateMs(0),
    minPositionMs(0), maxPositionMs(999000) {  // max 999초 기본
}

MotorDriver& MotorDriver::getInstance() {
  static MotorDriver instance;
  return instance;
}

void MotorDriver::init(int enPin, int cwPin, int ccwPin) {
  enablePin = enPin;
  pwmCwPin = cwPin;
  pwmCcwPin = ccwPin;
  pinMode(enablePin, OUTPUT);
  pinMode(pwmCwPin, OUTPUT);
  pinMode(pwmCcwPin, OUTPUT);
  
  // ESP32 Arduino 3.x: ledcAttach(pin, freq, resolution)
  ledcAttach(pwmCwPin, 5000, 8);   // 5kHz, 8bit
  ledcAttach(pwmCcwPin, 5000, 8);
  
  stop();
  
  // FreeRTOS 태스크 생성
  xTaskCreate(
    taskFunction,
    "MotorTask",
    2048,
    this,
    1,
    NULL
  );
  taskRunning = true;
}

void MotorDriver::taskFunction(void* param) {
  MotorDriver* motor = (MotorDriver*)param;
  motor->taskLoop();
}

void MotorDriver::taskLoop() {
  lastPositionUpdateMs = millis();
  for (;;) {
    unsigned long now = millis();
    unsigned long elapsed = now - lastPositionUpdateMs;
    lastPositionUpdateMs = now;
    
    // 명령이 변경되었는지 확인 (STOP -> RUN 시 soft start 시작)
    if (lastCmd == MOTOR_CMD_STOP && (currentCmd == MOTOR_CMD_RUN_CW || currentCmd == MOTOR_CMD_RUN_CCW)) {
      // Soft start 시작
      softStartActive = true;
      softStartStartTime = millis();
      currentActualSpeed = 0;
    }
    lastCmd = currentCmd;
    
    // Soft start 업데이트
    updateSoftStart();
    
    // 0~max 범위 제한: 어떤 조건에서도 진행 불가 (setPositionLimits로 설정됨)
    if (currentCmd == MOTOR_CMD_RUN_CW && positionMs >= maxPositionMs) currentCmd = MOTOR_CMD_STOP;
    if (currentCmd == MOTOR_CMD_RUN_CCW && positionMs <= minPositionMs) currentCmd = MOTOR_CMD_STOP;
    
    // 모터 제어
    switch (currentCmd) {
      case MOTOR_CMD_STOP:
        digitalWrite(enablePin, LOW);
        ledcWrite(pwmCwPin, 0);
        ledcWrite(pwmCcwPin, 0);
        running = false;
        softStartActive = false;
        currentActualSpeed = 0;
        break;
        
      case MOTOR_CMD_RUN_CW:
        digitalWrite(enablePin, HIGH);
        {
          int pwmVal = map(currentActualSpeed, 0, 100, 0, 255);
          ledcWrite(pwmCwPin, pwmVal);
          ledcWrite(pwmCcwPin, 0);
        }
        running = true;
        // 속도 보정: 같은 시간이라도 속도에 비례한 위치 (100% 기준 동등 시간)
        positionMs += (int32_t)(elapsed * currentActualSpeed / 100);
        if (positionMs > maxPositionMs) positionMs = maxPositionMs;
        break;
        
      case MOTOR_CMD_RUN_CCW:
        digitalWrite(enablePin, HIGH);
        {
          int pwmVal = map(currentActualSpeed, 0, 100, 0, 255);
          ledcWrite(pwmCwPin, 0);
          ledcWrite(pwmCcwPin, pwmVal);
        }
        running = true;
        // 속도 보정: 같은 시간이라도 속도에 비례한 위치 (100% 기준 동등 시간)
        positionMs -= (int32_t)(elapsed * currentActualSpeed / 100);
        if (positionMs < minPositionMs) positionMs = minPositionMs;
        break;
    }
    
    vTaskDelay(pdMS_TO_TICKS(20));  // 20ms 주기
  }
}

void MotorDriver::updateSoftStart() {
  if (!softStartActive) return;
  
  unsigned long elapsed = millis() - softStartStartTime;
  
  if (elapsed >= softStartDuration) {
    // Soft start 완료
    softStartActive = false;
    currentActualSpeed = targetSpeed;
  } else {
    // Soft start 진행 중 - 0에서 targetSpeed까지 점진적으로 증가
    currentActualSpeed = map(elapsed, 0, softStartDuration, 0, targetSpeed);
  }
}

void MotorDriver::setPWM(int cwValue, int ccwValue) {
  ledcWrite(pwmCwPin, cwValue);
  ledcWrite(pwmCcwPin, ccwValue);
}

void MotorDriver::runCW(uint8_t speed) {
  if (speed > 100) speed = 100;
  targetSpeed = speed;
  currentCmd = MOTOR_CMD_RUN_CW;
}

void MotorDriver::runCCW(uint8_t speed) {
  if (speed > 100) speed = 100;
  targetSpeed = speed;
  currentCmd = MOTOR_CMD_RUN_CCW;
}

void MotorDriver::stop() {
  currentCmd = MOTOR_CMD_STOP;
  targetSpeed = 0;
  currentActualSpeed = 0;
  softStartActive = false;
}

void MotorDriver::setSoftStartDuration(unsigned long ms) {
  softStartDuration = ms;
}

bool MotorDriver::isRunning() {
  return running;
}

uint8_t MotorDriver::getSpeed() {
  return targetSpeed;
}

uint8_t MotorDriver::getActualSpeed() {
  return currentActualSpeed;
}

int32_t MotorDriver::getPositionMs() {
  return positionMs;
}

float MotorDriver::getPositionSec() {
  return (float)positionMs / 1000.0f;
}

void MotorDriver::resetPosition() {
  positionMs = 0;
}

void MotorDriver::setPositionLimits(float minSec, float maxSec) {
  minPositionMs = (int32_t)(minSec * 1000.0f);
  maxPositionMs = (int32_t)(maxSec * 1000.0f);
}
