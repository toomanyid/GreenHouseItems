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
// 0.1A 단위 "2.1A". 비어 있으면 "-.-A"
void amps(char* out, int32_t ma);
// 온도 4칸 "25.5", " 5.0", "-9.9". 표시 범위 -9.9~99.9
void temp4(char* out, int16_t t);
// 부호 있는 0.1 단위 "+0.5", "-1.0"
void tenthsSigned(char* out, int32_t v);
// 위치 백분율 4칸 "49.6", " 0.5". 위치 모름이면 "--.-"
void pct4(char* out, pct_t p);

const char* faultLong(Cause c);     // "ERR OVERLOAD"
const char* causeLog(Cause c);      // 에러 기록 화면 문구

// __DATE__ "Oct  9 2026", __TIME__ "19:47:12" → "2026-10-09 19:47"
void buildDate(char* out, const char* date, const char* time);
// "WIN CTRL   00.09"
void splashTop(char* out, const char* name, uint8_t major, uint8_t minor);

// 창의 위치를 백분율로: CLOSE 0.0, 모름 PCT_UNKNOWN
pct_t winPct(const WindowStatus& w);
// 이동 중 목표를 백분율로: CLOSE로 가면 0.0
pct_t targetPct(const WindowStatus& w);

// 줄 끝 상태 글자: ^ 상승, v 하강, L 리밋, ? 위치 모름, ! 고장, 빈칸 대기
char statusChar(const WindowStatus& w);

// 작은 글씨 화면의 창 한 줄 (스펙 7절). "1 45.0>99.9    ^", "1 ERR OVERLOAD !"
void smallLine(char* out, const WindowStatus& w, uint8_t num, bool sensorErr);

// ------------------------------------------------------------
//  큰 글씨 (온도, 위치 동기). 숫자 하나는 가로 3칸 세로 2줄, 숫자 사이 한 칸,
//  소수점 1칸으로 왼쪽 11칸을 쓴다. 사용자 글자 5개와 ROM의 꽉 찬 글자로 그린다.
// ------------------------------------------------------------
const uint8_t BIG_GLYPH_COUNT = 5;
extern const uint8_t BIG_GLYPHS[BIG_GLYPH_COUNT][8];   // 위 막대, 아래 막대, 위아래 막대, 소수점, ≠
const char G_TOP  = 8;           // 사용자 글자 0번
const char G_BOT  = 9;
const char G_BOTH = 10;
const char G_DOT  = 11;
const char G_NE   = 12;          // ≠
const char G_FULL = (char)0xFF;

// row0, row1의 0~10칸에 0.1 단위 값을 큰 숫자로 쓴다. 0~99.9, 음수면 "--.-"
void bigNumber(char* row0, char* row1, int16_t tenths);

// ------------------------------------------------------------
//  가로 두 배 숫자 (개별 화면). 숫자 하나 = 글자 두 칸 = 사용자 글자 두 개.
//  10x8 점에서 맨 왼쪽 줄과 맨 아래 줄을 비운 9x7 점. 가로획 1점, 세로획 2점.
// ------------------------------------------------------------
void wideHalf(uint8_t digit, uint8_t side, uint8_t out[8]);

// 사용자 글자 8개를 숫자 4개(반쪽 두 개씩)에 나눠 쓴다. 칸 p는 사용자 글자 2p, 2p+1
struct WideSlots { int8_t digit[4]; };
// 필요한 숫자를 칸에 넣는다. 이미 있는 숫자는 그 칸에 둔다(화면이 덜 깜빡이게)
void wideAssign(WideSlots& s, const bool need[10]);
// 개별 화면 한 줄: 0칸 창 번호, 1칸 고른 창이면 *, 2~5칸 정수 부분(가로 두 배), 6칸 '.', 7칸 소수, 9~15칸 extra.
void wideLine(char* out, uint8_t num, pct_t p, const WideSlots& s, bool selected, const char* extra);
// wideLine이 쓰는 숫자를 need에 표시한다
void wideNeed(bool need[10], pct_t p);

}  // namespace Fmt
