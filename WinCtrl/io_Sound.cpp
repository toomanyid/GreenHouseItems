#include "io_Sound.h"
#include "dev_Buzzer.h"

static const uint32_t FAULT_REPEAT_MS = 10000;   // (임시)
static const uint32_t ESTOP_REPEAT_MS = 3000;    // (임시)

#define N(arr) (uint8_t)(sizeof(arr) / sizeof(arr[0]))

void Sound::play(Snd s) {
    static const Note click[]  = { {3000, 4, 0} };
    static const Note bump[]   = { {500, 40, 0} };
    static const Note commit[] = { {2000, 60, 0} };
    static const Note longp[]  = { {3500, 120, 0} };
    static const Note limit[]  = { {600, 200, 0} };
    static const Note alert[]  = { {2500, 300, 0} };
    static const Note boot[]   = { {2000, 80, 80}, {2000, 80, 0} };
    static const Note err[]    = { {800, 150, 0} };

    switch (s) {
    case Snd::CLICK:      if (_click) start(click, N(click), 0); break;
    case Snd::BUMP:       if (_click) start(bump, N(bump), 0); break;
    case Snd::COMMIT:     if (_alarm) start(commit, N(commit), 0); break;
    case Snd::LONG_PRESS: if (_alarm) start(longp, N(longp), 0); break;
    case Snd::LIMIT_STOP: if (_alarm) start(limit, N(limit), 0); break;
    case Snd::ALERT:      if (_alarm) start(alert, N(alert), 0); break;
    case Snd::BOOT_OK:    if (_alarm) start(boot, N(boot), 0); break;
    case Snd::ERROR_BEEP: if (_alarm) start(err, N(err), 0); break;
    }
}

void Sound::start(const Note* seq, uint8_t n, uint32_t now) {
    (void)now;
    // 소리가 이어지는 중이면 다음 tick에서 바로 바꿔 건다
    _pendSeq = seq;
    _pendN = n;
    _pendingStart = true;
}

void Sound::tick(uint32_t now, bool fault, bool estop) {
    // 고장과 비상정지 소리는 끌 수 없다
    static const Note faultSeq[] = { {1500, 150, 100}, {1500, 150, 100}, {1500, 150, 0} };
    static const Note estopSeq[] = { {1000, 800, 0} };
    if (estop && (!_wasEstop || now - _lastEstopMs >= ESTOP_REPEAT_MS)) {
        _lastEstopMs = now;
        start(estopSeq, N(estopSeq), now);
    } else if (!estop && fault && (!_wasFault || now - _lastFaultMs >= FAULT_REPEAT_MS)) {
        _lastFaultMs = now;
        start(faultSeq, N(faultSeq), now);
    }
    _wasEstop = estop;
    _wasFault = fault;

    if (!_bz) return;
    if (_pendingStart) {
        _pendingStart = false;
        _seq = _pendSeq; _n = _pendN; _i = 0; _nextMs = now;
    }
    if (_seq && _i < _n && (int32_t)(now - _nextMs) >= 0) {
        const Note& nt = _seq[_i++];
        _bz->tone(nt.hz, nt.ms);
        _nextMs = now + nt.ms + nt.gapMs;
        if (_i >= _n) _seq = nullptr;
    }
}
