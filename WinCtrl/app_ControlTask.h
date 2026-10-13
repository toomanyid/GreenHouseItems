#pragma once
#include <stdint.h>
#include "Types.h"

// ============================================================
//  3층: 제어 태스크 (코어 1, 10ms 고정 주기).
//  장치를 읽어 Dispatcher에 넘기고 출력을 건다. 큐와 상태 복사본을 가진다.
//  화면 태스크는 아래 함수만 쓴다.
// ============================================================
namespace ControlTask {
    // setup()에서 한 번. 장치를 준비하고 태스크를 띄운다.
    // 띄우기 전에 넣은 얼럿(전원 켤 때 미확인 고장 등)은 첫 주기에 처리된다.
    void begin(Model model, const Settings& s);

    bool sendJob(const Job& j);          // 큐가 가득 차면 false
    bool sendAlert(const Alert& a);      // 가득 차면 false (제어 쪽은 전체 정지로 처리)

    // 저장한 설정을 넘긴다. "설정 바뀜" 잡으로 알린다. 큐가 가득 차면 false
    bool sendSettings(const Settings& s, uint8_t timesMask, uint8_t calMask);

    void getStatus(Status& out);         // 상태 복사본을 통째로 복사
}
