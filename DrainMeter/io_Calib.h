#pragma once
#include <stdint.h>

// ============================================================
//  저울 보정값 저장 (ESP32 안의 비휘발 저장소 NVS)
//
//  영점(아무것도 안 올렸을 때 읽음값)과 1g당 읽음값을 라인마다 저장한다.
//  전원을 끄거나 새로 업로드해도 남는다. 플래시를 통째로 지우면 사라진다.
//  저장된 값이 없으면 영점 0, 1g당 읽음값은 config.h의 값을 쓴다.
// ============================================================

struct ScaleCalib {
    int32_t offset[2];   // [0] 1라인, [1] 2라인
    float perG[2];
    bool hasOffset;      // 영점이 저장돼 있었는지
    bool hasPerG[2];     // 라인마다 보정값이 저장돼 있었는지
};

// 저장된 값을 읽는다. 없는 값은 기본값으로 채운다.
void calibLoad(ScaleCalib& c);

// 영점만 저장한다 (두 라인 같이).
void calibSaveOffset(const ScaleCalib& c);

// 한 라인의 1g당 읽음값을 저장한다. line은 0(1라인) 또는 1(2라인).
void calibSavePerG(const ScaleCalib& c, uint8_t line);

// 저장된 값을 모두 지운다.
void calibClear();
