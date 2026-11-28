#pragma once
#include <stdint.h>

// ============================================================
//  4층: 온도제어 성적 (개발 시험용, 2026-11-28). 하드웨어를 부르지 않는다.
//  화면 태스크가 1초마다 상태 복사본에서 한 칸을 넣는다.
//   - 최근 10분 (계속 밀려감): 2004 LCD 아래 두 줄에 보인다
//   - 10분 요약 (24바이트): 플래시에 남긴다 (Store)
//   - 30초 원본 (16바이트): 메모리(PSRAM)에 60일 (RawLog)
//  성적은 자동이고 온도 센서가 정상인 동안만 센다. 창이 이미 끝에 있어 원하는 온도로 갈 수 없는 동안
//  (추운데 모두 CLOSE, 더운데 모두 상한 리밋이나 top)도 뺀다 (2026-11-28).
// ============================================================

struct StatSample {          // 1초 한 칸
    bool    active;          // 자동이고 온도 센서 정상 (성적에 셈)
    int16_t temp;            // 0.1도
    int16_t setT;
    int16_t deadband;
    bool    moving;          // 어느 창이든 구동 중
    bool    moveStart;       // 이번 초에 움직이기 시작함
    int16_t pos1, pos2;      // 0.1%, 모르면 -1
    int32_t gain;            // 효과의 세기 (0 = 모름)
    bool    warn;            // 미확인 경고가 있음
    bool    saturated;       // 창이 끝에 있어 원하는 온도로 갈 수 없음 (성적에서 뺌)
};

#pragma pack(push, 1)
struct StatSummary {         // 10분 요약 (플래시, 24바이트)
    uint32_t time;           // 구간 끝 시각 (Clock 초). RTC가 없으면 켠 뒤 초 | 0x80000000
    int16_t  setT;           // 0.1도 (구간 끝)
    int16_t  avgT;           // 평균 온도 0.1도
    uint16_t rmsC;           // 평균 오차 RMS 0.01도
    uint8_t  inBand;         // 불감대 안 비율 %
    int8_t   maxOver;        // 설정보다 가장 높았던 것 0.1도 (127까지)
    int8_t   maxUnder;       // 가장 낮았던 것 0.1도 (음수, -128까지)
    uint8_t  moves;          // 움직이기 시작한 횟수
    uint16_t motorS;         // 모터가 돈 초
    uint16_t pos1, pos2;     // 평균 위치 0.1% (0xFFFF = 모름)
    uint16_t gain10;         // 효과의 세기 ÷ 10
    uint8_t  activePct;      // 성적을 센 시간 비율 % (자동이고 센서 정상, 끝에 있어 뺀 시간 제외)
    uint8_t  flags;          // 비트 0: 경고가 있었음, 비트 1: RTC 시각, 비트 2: 끝에 있어 뺀 시간이 있었음
};
struct StatRaw {             // 30초 원본 (메모리, 16바이트)
    uint32_t time;
    int16_t  temp;           // 0.1도, 센서가 없거나 끊기면 -32768
    int16_t  setT;
    int16_t  pos1, pos2;     // 0.1%, 모르면 -1
    int16_t  slope;          // 분당 0.01도
    uint16_t gain10;
};
#pragma pack(pop)

struct StatRolling {         // 최근 10분
    uint16_t n;              // 센 초
    uint16_t rmsC;           // 0.01도
    uint8_t  inBand;         // %
    int16_t  maxOver, maxUnder;   // 0.1도
    uint8_t  moves;
    uint16_t motorS;
};

const uint32_t STAT_TIME_UPTIME = 0x80000000u;   // RTC가 없을 때 시각 표시

class Stats {
public:
    static const uint16_t WINDOW = 600;          // 10분
    void second(const StatSample& s);
    StatRolling rolling() const;
    // 지금 구간을 닫아 요약을 만들고 다음 구간을 시작한다
    void finish(uint32_t time, bool rtc, StatSummary& out);

private:
    struct Cell { int16_t err; uint8_t flags; };   // 비트 0 센 초, 1 불감대 안, 2 구동, 3 움직임 시작
    Cell     _ring[WINDOW] = {};
    uint16_t _head = 0;
    uint16_t _filled = 0;
    // 구간 누적
    uint32_t _secs = 0, _active = 0, _inBand = 0, _moving = 0, _moves = 0;
    int64_t  _sumSq = 0, _sumT = 0;
    int16_t  _maxOver = 0, _maxUnder = 0;
    uint64_t _sumP1 = 0, _sumP2 = 0;
    uint32_t _nP1 = 0, _nP2 = 0;
    int16_t  _lastSet = 0;
    int32_t  _lastGain = 0;
    bool     _warn = false;
    bool     _sat = false;
};

// 30초 원본을 담는 고리 (밖에서 준 버퍼에)
class RawLog {
public:
    void     begin(StatRaw* buf, uint32_t cap) { _buf = buf; _cap = buf ? cap : 0; _head = 0; _count = 0; }
    void     add(const StatRaw& r);
    uint32_t count() const { return _count; }
    uint32_t capacity() const { return _cap; }
    const StatRaw& at(uint32_t i) const;    // 0 = 가장 오래된 것
private:
    StatRaw* _buf = nullptr;
    uint32_t _cap = 0, _head = 0, _count = 0;
};

namespace StatMath {
    uint32_t isqrt(uint64_t v);
}
