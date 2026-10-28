#pragma once
#include <stdint.h>
#include "Types.h"
#include "ErrorLog.h"

// ============================================================
//  4층: 화면 태스크 (코어 0, 약 20ms 주기).
//  LCD, 엔코더 조작과 설정 메뉴, 소리, 설정과 에러의 플래시 저장을 맡는다.
//  모터와 MC는 만지지 않고 잡과 얼럿만 보낸다.
// ============================================================
namespace UiTask {
    // setup()에서 ControlTask::begin() 다음에 부른다.
    // 저장된 미확인 고장과 원점 센서 에러 기록을 제어 쪽에 얼럿으로 넘기고 태스크를 띄운다.
    void begin(Model model, const Settings& s, const ErrorLog& log, uint16_t bootCount,
               const char* buildDate, const char* buildTime);
}
