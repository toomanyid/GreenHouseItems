#pragma once
#include <stdint.h>
#include "Types.h"
#include "ErrorLog.h"
#include "io_Stats.h"

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
    // 마지막으로 고른 조작 항목 (스펙 7절, 8절). 없으면 dflt
    uint8_t  loadUiItem(uint8_t dflt);
    bool     saveUiItem(uint8_t item);

    // 10분 요약 (개발 시험용, 2026-11-28): 빈 플래시 영역(spiffs, 약 3.3MB)에 LittleFS 파일로 2년분.
    // 가득 차면 가장 오래된 것부터 덮어쓴다. 처음 쓸 때 영역을 포맷한다 (몇 초 걸림)
    const uint32_t STATS_CAP = 105120;             // 144건 × 730일
    bool     statsBegin();
    bool     statsAppend(const StatSummary& s);
    uint32_t statsCount();
    // 가장 최근 n건을 오래된 것부터 fn으로 넘긴다
    void     statsEach(uint32_t n, void (*fn)(const StatSummary&, void*), void* ctx);
}
