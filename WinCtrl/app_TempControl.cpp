#include "app_TempControl.h"

static cs_t roundStep(cs_t v) {
    if (v <= 0) return 0;
    return (v + CS_STEP / 2) / CS_STEP * CS_STEP;
}

void TempControl::begin(uint32_t now) {
    _logHead = 0;
    _logCount = 0;
    _lastLogMs = now;
    _lastDecisionMs = now;
    _wasValid = false;
    _wasPaused = false;
    _pred = 0;
    _slope = 0;
    _trendDir = _candDir = 0;
    _candSince = now;
    _respN = 0;
    resetStep();
}

uint32_t TempControl::periodMs() const {
    uint32_t setMs = (uint32_t)_t.periodS * 1000;
    uint16_t r = responseS();
    if (r == 0) return setMs;
    uint32_t p = (uint32_t)r * 100 * TEMP_PERIOD_RATE_X10;   // 초 × 1.2 → ms
    uint32_t minMs = (uint32_t)TEMP_PERIOD_MIN_S * 1000;
    if (p < minMs) p = minMs;
    if (p > setMs) p = setMs;
    return p;
}

// 2시간 안의 측정이 둘 이상일 때만 쓴다. 셋이면 가운데, 둘이면 긴 쪽 (짧게 잘못 줄이지 않게)
uint16_t TempControl::responseS() const {
    uint32_t v[TEMP_RESP_KEEP];
    uint8_t n = 0;
    for (uint8_t i = 0; i < _respN; i++)
        if (_nowMs - _respAt[i] <= TEMP_RESP_EXPIRE_MS) v[n++] = _respMs[i];
    if (n < 2) return 0;
    for (uint8_t i = 1; i < n; i++)
        for (uint8_t j = i; j > 0 && v[j - 1] > v[j]; j--) { uint32_t t = v[j]; v[j] = v[j - 1]; v[j - 1] = t; }
    uint32_t ms = n == 2 ? v[1] : v[n / 2];
    return (uint16_t)((ms + 500) / 1000);
}

void TempControl::addResponse(uint32_t ms, uint32_t now) {
    for (uint8_t i = TEMP_RESP_KEEP - 1; i > 0; i--) { _respMs[i] = _respMs[i - 1]; _respAt[i] = _respAt[i - 1]; }
    _respMs[0] = ms;
    _respAt[0] = now;
    if (_respN < TEMP_RESP_KEEP) _respN++;
    _respLastS = (uint16_t)((ms + 500) / 1000);
    _respCount++;
}

// 기록마다(10초) 부른다. 기울기 방향이 1분 이어지면 흐름이 바뀐 것으로 본다
void TempControl::updateTrend(uint32_t now, bool paused) {
    int8_t s = _slope >= TEMP_TREND_SLOPE ? 1 : (_slope <= -TEMP_TREND_SLOPE ? -1 : 0);
    if (s != _candDir) { _candDir = s; _candSince = now; }
    if (_pendMove && now - _pendMs > TEMP_RESP_MAX_MS + TEMP_TREND_HOLD_MS) _pendMove = false;   // 반응 없음: 버림
    if (s == 0 || s == _trendDir || now - _candSince < TEMP_TREND_HOLD_MS) return;

    int8_t old = _trendDir;
    _trendDir = s;
    uint32_t start = _candSince;             // 그 방향이 시작된 때

    // 반응 시간: 움직인 뒤 그 움직임이 노린 쪽으로 흐름이 생김 (열면 내림, 닫으면 오름)
    if (_pendMove && s == -_pendDir && (int32_t)(start - _pendMs) >= 0) {
        uint32_t r = start - _pendMs;
        if (r <= TEMP_RESP_MAX_MS) addResponse(r, now);
        _pendMove = false;
    }

    // 흐름의 역전 (오름 ↔ 내림)
    if (old == 0 || old != -s || paused) return;
    _revCount++;
    bool ourEffect = _lastMoveDir != 0 && s == -_lastMoveDir && now - _lastMoveMs <= TEMP_RESP_MAX_MS + TEMP_TREND_HOLD_MS;
    _revHeld = ourEffect;
    if (ourEffect) {
        // 우리가 움직인 효과가 나타남: 더 움직이지 않고 효과를 볼 시간을 준다
        _lastDecisionMs = now;
        _streak = 0;
    }
    // 우리 움직임과 상관없는 역전(해, 구름: 열원이 바뀜)은 아직 아무것도 하지 않는다.
    // 열원 변화는 따로 다룰 방법을 정한 뒤 넣는다 (2026-11-28, 지금은 바람에 따른 반응 속도만)
}

int16_t TempControl::toTemp(int32_t ua, const TempSettings& t) {
    // 온도 = 4mA 온도 + (전류 − 4mA) × (20mA 온도 − 4mA 온도) ÷ 16mA + 보정값
    int32_t v = t.t4 + (int32_t)((int64_t)(ua - 4000) * (t.t20 - t.t4) / 16000) + t.offset;
    if (v < -999) v = -999;
    if (v > 1500) v = 1500;
    return (int16_t)v;
}

cs_t TempControl::stepSize() const {
    cs_t s = _t.step + (cs_t)_streak * _t.stepAdd;
    if (s > _t.stepMax) s = _t.stepMax;
    return s;
}

void TempControl::record(int16_t temp) {
    _log[_logHead] = temp;
    _logHead = (uint16_t)((_logHead + 1) % TEMP_LOG_SIZE);
    if (_logCount < TEMP_LOG_SIZE) _logCount++;
}

// 최근 "기울기 구간" 기록에 가장 잘 맞는 직선의 기울기 (분당 0.01도)
int16_t TempControl::computeSlope() const {
    uint16_t want = (uint16_t)_t.slopeMin * (60000 / TEMP_LOG_MS);
    if (want > TEMP_LOG_SIZE) want = TEMP_LOG_SIZE;
    if (_logCount < 2 || _logCount * 2 < want) return 0;    // 절반도 안 쌓이면 0
    uint16_t n = _logCount < want ? _logCount : want;

    int64_t sx = 0, sy = 0, sxy = 0, sxx = 0;
    for (uint16_t i = 0; i < n; i++) {
        uint16_t idx = (uint16_t)((_logHead + TEMP_LOG_SIZE - n + i) % TEMP_LOG_SIZE);
        int64_t x = i, y = _log[idx];
        sx += x; sy += y; sxy += x * y; sxx += x * x;
    }
    int64_t den = (int64_t)n * sxx - sx * sx;
    if (den == 0) return 0;
    int64_t num = ((int64_t)n * sxy - sx * sy) * (10 * 60000 / (int64_t)TEMP_LOG_MS);  // 0.1도/기록 → 0.01도/분
    int64_t s = (num >= 0 ? num + den / 2 : num - den / 2) / den;
    if (s > 30000) s = 30000;
    if (s < -30000) s = -30000;
    return (int16_t)s;
}

uint8_t TempControl::tick(uint32_t now, bool valid, int16_t temp, bool paused,
                          const WinView v[2], Job jobs[2]) {
    _nowMs = now;
    if (!valid) {
        // 센서 값이 끊기면 온도제어를 멈추고 창은 그 자리에 둔다
        if (_wasValid) { resetStep(); _logCount = 0; _trendDir = _candDir = 0; _candSince = now; }
        _wasValid = false;
        return 0;
    }
    bool recorded = false;
    if (!_wasValid) {
        _logCount = 0;
        _logHead = 0;
        record(temp);
        _lastLogMs = now;
        _wasValid = true;
    } else if (now - _lastLogMs >= TEMP_LOG_MS) {
        _lastLogMs += TEMP_LOG_MS;
        record(temp);
        recorded = true;
    }

    _slope = computeSlope();
    _pred = (int16_t)(temp + (int32_t)_slope * _t.aheadMin / 10);
    if (recorded) updateTrend(now, paused);

    if (paused) {
        _wasPaused = true;
        return 0;
    }
    if (_wasPaused) {
        _wasPaused = false;
        resetStep();                      // 메뉴에서 나오면 처음 크기부터
        _lastDecisionMs = now;
        if (!_decideNow) return 0;
    }
    if (!_decideNow && now - _lastDecisionMs < periodMs()) return 0;
    _decideNow = false;
    _lastDecisionMs = now;

    int16_t hi = _t.setT + _t.deadband;
    int16_t lo = _t.setT - _t.deadband;
    int8_t dir = _pred > hi ? 1 : (_pred < lo ? -1 : 0);

    if (dir == 0 || dir != _lastDir) _streak = 0;      // 불감대 안이거나 방향이 바뀌면 처음 크기로
    cs_t size = stepSize();

    uint8_t n = 0;
    bool counted = false;
    for (uint8_t i = 0; i < 2; i++) {
        if (!v[i].eligible) continue;
        Job j = {};
        j.source = JobSource::TEMP_CONTROL;
        j.window = i;
        bool make = false;

        if (v[i].posState == PosState::CLOSED) {
            // CLOSE에서는 불감대 안으로 들어오면 home으로 (단계 횟수에 넣지 않음)
            if (_pred >= lo) { j.type = JobType::MOVE; j.target = v[i].home; make = true; }
        } else if (v[i].posState == PosState::UNKNOWN) {
            if (dir > 0) { j.type = JobType::MOVE; j.target = v[i].home; make = true; }
            else if (dir < 0) { j.type = JobType::CLOSE; make = true; }
        } else if (dir > 0) {
            cs_t t = roundStep(v[i].pos + size);
            if (t > v[i].limit) t = v[i].limit;
            if (t > v[i].pos) { j.type = JobType::MOVE; j.target = t; make = true; counted = true; }
        } else if (dir < 0) {
            cs_t h = v[i].home;
            if (v[i].pos <= h) {
                j.type = JobType::CLOSE; make = true;           // home에서 다음 주기에도 낮으면 CLOSE
            } else {
                cs_t t = v[i].pos - size;
                t = t <= h ? h : roundStep(t);
                if (t < h) t = h;
                if (t >= v[i].pos) t = v[i].pos - CS_STEP < h ? h : v[i].pos - CS_STEP;
                j.type = JobType::MOVE; j.target = t; make = true; counted = true;
            }
        }
        if (make) jobs[n++] = j;
    }
    if (counted) {
        if (_streak < 255) _streak++;
        _lastDir = dir;
    }
    if (n > 0) {
        // 이번에 움직인 방향 (CLOSE에서 home으로 가는 것은 엶)
        int8_t md = dir != 0 ? dir : 1;
        _lastMoveMs = now;
        _lastMoveDir = md;
        _pendMove = true;                    // 반응 시간을 잰다 (새로 움직이면 앞의 측정은 버림)
        _pendMs = now;
        _pendDir = md;
    }
    return n;
}
