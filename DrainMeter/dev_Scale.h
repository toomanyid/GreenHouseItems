#pragma once
#include <stdint.h>

// ============================================================
//  로드셀 2세트 (HX711 2개, 클럭 공용)
//
//  두 HX711이 클럭선 하나를 같이 쓰므로 한쪽만 따로 읽을 수 없다.
//  클럭을 줄 때마다 두 데이터선을 같이 읽어 두 값을 한 번에 얻는다.
//  흔한 HX711 라이브러리는 하나씩 읽게 돼 있어 쓰지 않는다.
//
//  채널 A, 증폭 128배, 10회/초(모듈의 RATE 핀이 GND) 기준이다.
// ============================================================

struct ScaleRaw {
    int32_t a;   // 1라인
    int32_t b;   // 2라인
};

void scaleBegin();

// 두 HX711을 깨운다. 값이 안정될 때까지 HX_SETTLE_MS 기다리고 첫 값들을 버린다.
void scaleWake();

// 두 HX711을 쉬게 한다 (클럭을 60µs 넘게 HIGH로 둔다). 쉬는 동안 1µA 아래.
void scaleSleep();

// 두 값이 모두 준비되면 한 번에 읽는다. 시간 안에 준비되지 않으면 false.
bool scaleReadRaw(ScaleRaw& out);

// n번 읽어 평균을 낸다. 하나라도 실패하면 false.
bool scaleReadAvg(ScaleRaw& out, uint8_t n);
