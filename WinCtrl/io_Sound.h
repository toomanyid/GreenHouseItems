#pragma once
#include <stdint.h>

// ============================================================
//  4층: 소리 (스펙 7절 소리 표). 화면 태스크가 주기마다 tick()을 부른다.
//  여러 번 울리는 소리는 여기서 순서대로 걸고, 한 음의 끝은 Buzzer 타이머가 끊는다.
// ============================================================
class Buzzer;

enum class Snd : uint8_t {
    CLICK,        // 엔코더 한 칸
    BUMP,         // 리밋이나 CLOSE에 닿아 더 안 돌아감
    COMMIT,       // 입력 완료
    LONG_PRESS,   // 길게 누르기 3초
    LIMIT_STOP,   // 상한 리밋에서 정지
    ALERT,        // 원점 센서 에러, 온도 센서 끊김
    BOOT_OK,      // 자가 점검 통과
    ERROR_BEEP    // 저장 실패, 범위 밖 같은 화면 안내
};

class Sound {
public:
    void begin(Buzzer* bz) { _bz = bz; }
    void setOptions(bool click, bool alarm) { _click = click; _alarm = alarm; }
    void play(Snd s);
    // fault: 고장 상태가 있음 (세 번, 주기적으로). estop: 비상정지 눌림 (길게, 주기적으로)
    void tick(uint32_t now, bool fault, bool estop);

private:
    struct Note { uint16_t hz; uint16_t ms; uint16_t gapMs; };
    void start(const Note* seq, uint8_t n, uint32_t now);

    Buzzer*     _bz = nullptr;
    bool        _click = true, _alarm = true;
    const Note* _seq = nullptr;
    uint8_t     _n = 0, _i = 0;
    uint32_t    _nextMs = 0;
    bool        _pendingStart = false;
    const Note* _pendSeq = nullptr;
    uint8_t     _pendN = 0;
    uint32_t    _lastFaultMs = 0, _lastEstopMs = 0;
    bool        _wasFault = false, _wasEstop = false;
};
