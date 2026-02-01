#ifndef APP_STATE_IDLE_H
#define APP_STATE_IDLE_H

#include "AppState.h"

// Idle 상태: 대기 모드
class AppStateIdle : public AppState {
public:
  void onEnter() override;
  void onUpdate() override;
  const char* getName() const override { return "Idle"; }
  
  static AppStateIdle& getInstance();
  
private:
  unsigned long lastEncoderChangeMs;
  AppStateIdle() : lastEncoderChangeMs(0) {}
  AppStateIdle(const AppStateIdle&) = delete;
  void operator=(const AppStateIdle&) = delete;
};

#endif
