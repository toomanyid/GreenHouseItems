#pragma once
#include <stdint.h>
#include "Types.h"
#include "ErrorLog.h"

// ============================================================
//  플래시 저장 (NVS). 화면 태스크만 부른다.
//  설정, 에러 저장, 전원을 켠 횟수.
// ============================================================
namespace Store {
    void     begin();
    // 저장된 설정을 읽는다. 없거나 버전이 다르거나 범위 밖이면 기본값으로 채우고 false
    bool     loadSettings(Settings& s);
    bool     saveSettings(const Settings& s);
    void     loadErrors(ErrorLog& log);
    bool     saveErrors(const ErrorLog& log);
    uint16_t bumpBootCount();        // 1 늘려 저장하고 돌려준다
}
