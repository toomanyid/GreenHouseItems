#ifndef MOTOR_DRIVER_H
#define MOTOR_DRIVER_H

#include <Arduino.h>

// 모터 제어 명령 타입
enum MotorCommand {
  MOTOR_CMD_STOP,
  MOTOR_CMD_RUN_CW,
  MOTOR_CMD_RUN_CCW
};

// 모터 제어 클래스 (FreeRTOS 태스크로 동작)
class MotorDriver {
private:
  int enablePin;
  int pwmCwPin;
  int pwmCcwPin;
  
  uint8_t targetSpeed;     // 목표 속도 (0-100)
  uint8_t currentActualSpeed;  // 현재 실제 속도 (soft start용, 0-100)
  MotorCommand currentCmd;
  MotorCommand lastCmd;    // 이전 명령 (soft start 트리거용)
  bool running;
  bool taskRunning;
  
  // Soft start 변수
  bool softStartActive;
  unsigned long softStartStartTime;
  unsigned long softStartDuration;  // 밀리초 (기본 1000ms = 1초)
  
  // 위치 = 속도 보정 동작시간 (CW: +, CCW: -), 100% 기준 동등 ms
  volatile int32_t positionMs;
  unsigned long lastPositionUpdateMs;
  
  static void taskFunction(void* param);
  void taskLoop();
  void setPWM(int cwValue, int ccwValue);
  void updateSoftStart();
  
public:
  MotorDriver(int enablePin, int pwmCwPin, int pwmCcwPin);
  
  void init();
  void runCW(uint8_t speed);   // 시계방향, speed: 0-100
  void runCCW(uint8_t speed);  // 반시계방향, speed: 0-100
  void runCW() { runCW(100); }   // 시계방향, speed: 100
  void runCCW() { runCCW(100); }  // 반시계방향, speed: 100
  void stop();
  void setSoftStartDuration(unsigned long ms);  // soft start 시간 설정
  
  bool isRunning();
  uint8_t getSpeed();
  uint8_t getActualSpeed();  // 현재 실제 속도 (soft start 중)
  
  // 위치 = 속도 보정 누적 동작시간 (CW +, CCW -), 100% 기준 동등 시간
  int32_t getPositionMs();   // 동등 밀리초
  float getPositionSec();    // 동등 초
  void resetPosition();      // 위치 0으로 리셋
};

#endif
