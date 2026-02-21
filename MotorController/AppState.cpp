#include "AppState.h"
#include "KeyQueue.h"

AppStateContext::AppStateContext()
  : currentState(nullptr), nextState(nullptr) {
}

void AppStateContext::setState(AppState* state) {
  if (state == nullptr) return;
  
  if (currentState != nullptr) {
    currentState->onExit();
  }
  
  currentState = state;
  nextState = nullptr;
  currentState->onEnter();
  
  Serial.print("State: ");
  Serial.println(currentState->getName());
}

const char* AppStateContext::getStateName() const {
  return (currentState != nullptr) ? currentState->getName() : "None";
}

void AppStateContext::update() {
  if (currentState == nullptr) return;
  // 큐에 쌓인 키를 하나씩 꺼내 현재 상태에 전달
  while (!KeyQueue::isEmpty()) {
    Key k = KeyQueue::pop();
    if (k != Key::None) {
      currentState->onKey(k);
    }
  }
  currentState->onUpdate();
}
