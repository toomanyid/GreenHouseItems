#pragma once
#include "io_Stats.h"
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
// "WIN CTRL   00.90"
void splashTop(char* out, const char* name, uint8_t major, uint8_t minor);

// 창의 위치를 백분율로: CLOSE 0.0, 모름 PCT_UNKNOWN
pct_t winPct(const WindowStatus& w);
// 이동 중 목표를 백분율로: CLOSE로 가면 0.0
pct_t targetPct(const WindowStatus& w);

// 줄 끝 상태 글자: ^ 상승, v 하강, L 리밋, ? 위치 모름, ! 고장, 빈칸 대기
char statusChar(const WindowStatus& w);

// 켤 때 모터 정보 없음 경고 (2026-11-22). mask: 기본값을 쓰는 창 (1 = 창1, 2 = 창2)
// "MOTOR NOT SET 12" / "USE DEFAULT PUSH"
void notSetLines(char* r0, char* r1, uint8_t mask);
    // 처음 켤 때의 기본 구성 경고 한 장 (스펙 7절, 2026-11-28). noHome: 원점 센서가 꺼진 창 (비트 0, 1).
    // last면 오른쪽 아래에 OK, 아니면 > (다음 장이 있음)
    enum BasicPage : uint8_t { BASIC_MC, BASIC_HOME, BASIC_TEMP };
    void basicLines(char* r0, char* r1, uint8_t page, uint8_t noHome, bool last);
    // 2004 LCD 아래 두 줄 (개발 시험용, 20글자). 최근 10분 성적
    //   "12:34 E0.42 B 87 M 3"  시:분, 평균 오차(도), 불감대 안 %, 움직인 횟수
    //   "+1.2 -0.8 G640  WAIT"  가장 높이/낮게 벗어난 것(도), 효과의 세기, 효과를 기다리는 중
    void devLines(char* a, char* b, bool timeOk, uint8_t hh, uint8_t mm, const StatRolling& r,
                  int32_t gain, bool settling);

// 작은 글씨 화면의 창 한 줄 (스펙 7절). "1 45.0>99.9    ^", "1 ERR OVERLOAD !"
// warn: 그 창의 미확인 경고(없으면 NONE). 고장이 아니므로 창이 움직여도 문구를 남기고 줄 끝 글자는 그대로
void smallLine(char* out, const WindowStatus& w, uint8_t num, Cause warn);

// ------------------------------------------------------------
//  큰 글씨 (온도, 위치 동기). 정수 두 자리는 가로 3칸 세로 2줄인 7획 글씨다.
//  세로획은 양옆 칸에 3점, 가로획은 가운데 칸에만 2줄로 그려 칸 사이 틈이 획 사이에 온다.
//  숫자 0~2칸, 3~5칸, 아랫줄 6칸 소수점, 아랫줄 7~8칸 소수 자리(가로 두 배, 한 줄 내림).
//  사용자 글자 0~5번은 고정 획, 6~7번은 소수 자리 숫자의 반쪽이다.
// ------------------------------------------------------------
const uint8_t BIG_GLYPH_COUNT = 6;
extern const uint8_t BIG_GLYPHS[BIG_GLYPH_COUNT][8];   // 왼쪽 세로획, 오른쪽 세로획, 위 가로획, 아래 가로획, 위아래 가로획, 도 기호
const char G_VL   = 8;           // 사용자 글자 0번. 숫자 왼쪽 칸의 세로획 (칸의 오른쪽 3점)
const char G_VR   = 9;           // 숫자 오른쪽 칸의 세로획 (칸의 왼쪽 3점)
const char G_TOP  = 10;          // 위 가로획 (위 2줄)
const char G_BOT  = 11;          // 아래 가로획 (아래 2줄). 윗줄에서는 가운데 획, 아랫줄에서는 아래 획
const char G_BOTH = 12;
const char G_DEG  = 13;          // 도 기호 ° (큰 글씨 화면에서만 쓸 수 있음)
const char G_DEC  = 14;          // 소수 자리 왼쪽 반쪽. 오른쪽 반쪽은 G_DEC + 1
const uint8_t BIG_DEC_SLOT = 6;  // G_DEC의 사용자 글자 번호
const char G_DEGREE = G_DEG;        // 사용자 글자로 그린 둥근 °

// row0의 0~5칸, row1의 0~8칸에 0.1 단위 값을 큰 숫자로 쓴다. 0~99.9, 음수면 "--.-".
// 소수 자리 숫자를 돌려준다(음수면 -1, 가운데 획). 그 반쪽은 bigDecHalf로 만들어 6, 7번에 넣는다
int8_t bigNumber(char* row0, char* row1, int16_t tenths);
// 소수 자리 숫자의 반쪽. 가로 두 배 숫자를 한 줄 내려 큰 숫자와 바닥을 맞춘다. digit -1은 가운데 획
void bigDecHalf(int8_t digit, uint8_t side, uint8_t out[8]);

// ------------------------------------------------------------
//  가로 두 배 숫자 (개별 화면). 숫자 하나 = 글자 두 칸 = 사용자 글자 두 개.
//  10x8 점에서 맨 왼쪽 줄과 맨 아래 줄을 비운 9x7 점. 가로획 1점, 세로획 2점.
// ------------------------------------------------------------
void wideHalf(uint8_t digit, uint8_t side, uint8_t out[8]);

// 사용자 글자 8개를 숫자 4개(반쪽 두 개씩)에 나눠 쓴다. 칸 p는 사용자 글자 2p, 2p+1
const char G_WIDE = 8;           // 사용자 글자 0번
struct WideSlots { int8_t digit[4]; };
// 필요한 숫자를 칸에 넣는다. 이미 있는 숫자는 그 칸에 둔다(화면이 덜 깜빡이게)
void wideAssign(WideSlots& s, const bool need[10]);
// 개별 화면 한 줄: 0칸 창 번호, 1칸 고른 창이면 *, 2~5칸 정수 부분(가로 두 배), 6칸 '.', 7칸 소수, 9~15칸 extra.
void wideLine(char* out, uint8_t num, pct_t p, const WideSlots& s, bool selected, const char* extra);
// wideLine이 쓰는 숫자를 need에 표시한다
void wideNeed(bool need[10], pct_t p);

// 자동 판단 상태 두 글자: 예측 온도가 설정 온도 + 불감대보다 높으면 "OP"(열 차례), 설정 온도 - 불감대보다
// 낮으면 "CL"(닫을 차례), 그 사이면 "HD"(그대로). app_TempControl과 같은 기준, 0.1도 단위 (2026-11-25)
const char* autoState(int16_t predicted, int16_t setT, int16_t deadband);
// 다음 판단까지 남은 초를 3글자 오른쪽 맞춤으로 ("  8", " 45", "600")
void waitSecs3(char* out, uint16_t s);

// 이번 이동이 끝날 때까지 남은 초 (자동 화면, 2026-11-26). 움직이지 않으면 -1.
// 내려갈 때는 하강 시간으로 환산하고, 방향 전환 정지 1초를 더한다. 위치를 모르면(원점 잡기) -2
int32_t moveRemainS(const WindowStatus& w, cs_t upTime, cs_t downTime);

// 지그재그(+3 -3 +3, 1초 안) 판정. 엔코더 칸마다 feed()하고, 완성되는 칸에서 true (2026-11-25)
class Zigzag {
public:
    bool feed(int8_t dir, uint32_t ms);
    void reset() { _dir = 0; _run = 0; _done = 0; }
private:
    int8_t   _dir = 0;
    uint8_t  _run = 0;          // 지금 방향으로 이어진 칸 수
    uint8_t  _done = 0;         // ZIGZAG_RUN칸 이상으로 끝난 묶음 수 (방향이 번갈아)
    uint32_t _runMs = 0;        // 지금 묶음의 첫 칸 시각
    uint32_t _startMs = 0;      // 첫 묶음의 첫 칸 시각
};

}  // namespace Fmt
