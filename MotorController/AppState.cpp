#include "AppState.h"

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
}

const char* AppStateContext::getStateName() const {
  return (currentState != nullptr) ? currentState->getName() : "None";
}

void AppStateContext::update() {
  if (currentState != nullptr) {
    currentState->onUpdate();
  }
}
