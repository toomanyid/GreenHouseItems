#pragma once
#include <stdint.h>
#include "Types.h"

// ============================================================
//  4층: 화면 글자 만들기 (스펙 7절, 9절). 하드웨어를 부르지 않아 PC에서 시험한다.
//  출력 버퍼는 모두 끝에 '\0'을 붙인다.
// ============================================================
namespace Fmt {

// 0.5초 단위로 맞춘 초 "182.5" (오른쪽 맞춤, width칸). 비어 있으면 "---.-"
void secs(char* out, cs_t v, uint8_t width);
// 내부 값 그대로 0.01초까지 "163.52"
void secs2(char* out, cs_t v);
// 정수 초 (0.5는 올림) 오른쪽 맞춤 width칸. 비어 있으면 "---"
void secsInt(char* out, cs_t v, uint8_t width);
// 백분율 1~100 (3칸)
void pct(char* out, cs_t pos, cs_t maxPos);
uint8_t pctValue(cs_t pos, cs_t maxPos);
// 0.1A 단위 "2.1A". 비어 있으면 "-.-A"
void amps(char* out, int32_t ma);
// 온도 4칸 "25.5", " 5.0", "-9.9". 표시 범위 -9.9~99.9
void temp4(char* out, int16_t t);
// 부호 있는 0.1 단위 "+0.5", "-1.0"
void tenthsSigned(char* out, int32_t v);

const char* faultLong(Cause c);     // "ERR OVERLOAD"
const char* faultShort(Cause c);    // "E-OVL"
const char* causeLog(Cause c);      // 에러 기록 화면 문구

// __DATE__ "Oct  9 2026", __TIME__ "19:47:12" → "2026-10-09 19:47"
void buildDate(char* out, const char* date, const char* time);
// "WIN CTRL   00.09"
void splashTop(char* out, const char* name, uint8_t major, uint8_t minor);

// 창 한 줄을 그리는 데 필요한 화면 쪽 값
struct LineCtx {
    uint8_t num;          // 1, 2
    char    sel;          // '>' 선택, '*' 입력 중, ' '
    bool    editing;      // 입력 중 (editTarget을 보임)
    cs_t    editTarget;   // 입력 중 목표 (-1 = CLOSE)
    bool    blinkOn;      // 0.5초 깜빡임 박자
    bool    altPhase;     // 1초 번갈아 보이기 박자 (true면 문구 쪽)
    bool    sensorErr;    // 원점 센서 에러 표시 중
};

char statusChar(const WindowStatus& w, bool blinkOn);

// 온도 센서가 없을 때의 16칸 줄
void winLine(char* out, const WindowStatus& w, const LineCtx& c);
// 온도 센서가 있을 때의 오른쪽 8칸
void winShort(char* out, const WindowStatus& w, const LineCtx& c);
// 온도 센서가 있을 때의 16칸 줄: 왼쪽 6칸 온도 + 2칸 + 오른쪽 8칸
void tempLine(char* out, char sel, bool valid, int16_t t, char suffix, const char* right8);

}  // namespace Fmt
