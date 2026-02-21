#ifndef MOTOR_POSITION_CONFIG_H
#define MOTOR_POSITION_CONFIG_H

#include <Arduino.h>

// 모터 위치 설정값 (동작시간 기준, 초 단위) + 전류 한계
// min: 하한, zero: 기준(0%), limit: 동작 상한, max: 최대, maxCurrent: 전류 한계(A)
struct MotorPositionConfig {
  float min;        // 모터 하한
  float zero;       // 기준 위치 (0%)
  float limit;      // 동작 상한 (이 위로 진행 불가)
  float max;        // 최대
  float maxCurrent; // 전류 한계 (A), 범위 1~10, 0.1 단위
  float target;     // Run 목표위치 (런타임, 저장 안 함)
  
  MotorPositionConfig()
    : min(0), zero(5), limit(10), max(15), maxCurrent(2.0f), target(0) {}
  
  float* getField(int index) {
    switch (index) {
      case 0: return &min;
      case 1: return &zero;
      case 2: return &limit;
      case 3: return &max;
      case 4: return &maxCurrent;
      default: return nullptr;
    }
  }
  
  static const char* getFieldName(int index) {
    switch (index) {
      case 0: return "min";
      case 1: return "zero";
      case 2: return "limit";
      case 3: return "max";
      case 4: return "maxCur";
      default: return "";
    }
  }
  
  static const int FIELD_COUNT = 5;
  
  // 위치(초) → 로터리용 % (zero=0%, max=100%)
  static float positionToPercent(float posSec, float zeroSec, float maxSec) {
    if (maxSec <= zeroSec) return (posSec <= zeroSec) ? 0.0f : 100.0f;
    if (posSec <= zeroSec) return 0.0f;
    if (posSec >= maxSec) return 100.0f;
    return (posSec - zeroSec) / (maxSec - zeroSec) * 100.0f;
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
