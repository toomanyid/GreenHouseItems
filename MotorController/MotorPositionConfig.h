#ifndef MOTOR_POSITION_CONFIG_H
#define MOTOR_POSITION_CONFIG_H

#include <Arduino.h>

// 모터 위치 설정값 (동작시간 기준, 초 단위)
// bottom: 하한, ready: 준비위치, top: 상한, max: 동작가능 최대
struct MotorPositionConfig {
  float bottom;   // 모터 하한
  float ready;    // 준비 위치
  float top;      // 모터 상한
  float max;      // 동작가능 최대
  float target;   // Run 목표위치 (런타임, 저장 안 함)
  
  MotorPositionConfig()
    : bottom(0), ready(5), top(10), max(15), target(0) {}
  
  float* getField(int index) {
    switch (index) {
      case 0: return &bottom;
      case 1: return &ready;
      case 2: return &top;
      case 3: return &max;
      default: return nullptr;
    }
  }
  
  static const char* getFieldName(int index) {
    switch (index) {
      case 0: return "bottom";
      case 1: return "ready";
      case 2: return "top";
      case 3: return "max";
      default: return "";
    }
  }
  
  static const int FIELD_COUNT = 4;
  
  // 위치(초) → 표시용 % (ready=1%, top=100%, ready 미만=0%)
  static float positionToPercent(float posSec, float readySec, float topSec) {
    if (topSec <= readySec) return (posSec <= readySec) ? 0.0f : 100.0f;
    if (posSec <= readySec) return 0.0f;
    if (posSec >= topSec) return 100.0f;
    return 1.0f + (posSec - readySec) / (topSec - readySec) * 99.0f;
  }
};

// 싱글톤 + 플래시 저장/로드
class MotorPositionConfigManager {
public:
  static MotorPositionConfig& getInstance() {
    static MotorPositionConfig instance;
    return instance;
  }
  
  // 내부 플래시(NVS)에서 설정 로드, 없으면 기본값 유지
  static bool load();
  // 내부 플래시에 설정 저장
  static bool save();
};

#endif
