#ifndef APP_STATE_SETUP_H
#define APP_STATE_SETUP_H

#include "AppState.h"

// Setup 상태: bottom, ready, top, max 4가지 위치값 설정
// PUSH: 저장 후 Idle로 복귀
// CW/CCW (Up/Down): 항목 간 이동
// 로터리: 선택된 값 증감 (1초/틱)
class AppStateSetup : public AppState {
private:
  int selectedIndex;   // 0=bottom, 1=ready, 2=top, 3=max
  int lastEncoderValue;
  
  AppStateSetup();
  AppStateSetup(const AppStateSetup&) = delete;
  void operator=(const AppStateSetup&) = delete;

public:
  void onEnter() override;
  void onUpdate() override;
  const char* getName() const override { return "Setup"; }
  
  static AppStateSetup& getInstance();
};

#endif
