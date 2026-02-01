#ifndef APP_STATE_H
#define APP_STATE_H

#include <Arduino.h>

// 상태 패턴: 추상 베이스 클래스
class AppState {
public:
  virtual ~AppState() {}
  
  virtual void onEnter() {}   // 상태 진입 시
  virtual void onUpdate() {}  // 매 루프 호출
  virtual void onExit() {}    // 상태 이탈 시
  
  virtual const char* getName() const { return "State"; }
};

// 앱 상태 컨텍스트 (현재 상태 보유 및 위임)
class AppStateContext {
private:
  AppState* currentState;
  AppState* nextState;  // 상태 전환 대기

public:
  AppStateContext();
  
  void setState(AppState* state);
  AppState* getState() { return currentState; }
  const char* getStateName() const;
  
  void update();  // loop에서 호출, 현재 상태의 onUpdate() 실행
};

#endif
