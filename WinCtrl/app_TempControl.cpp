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
    resetStep();
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
    if (!valid) {
        // 센서 값이 끊기면 온도제어를 멈추고 창은 그 자리에 둔다
        if (_wasValid) { resetStep(); _logCount = 0; }
        _wasValid = false;
        return 0;
    }
    if (!_wasValid) {
        _logCount = 0;
        _logHead = 0;
        record(temp);
        _lastLogMs = now;
        _wasValid = true;
    } else if (now - _lastLogMs >= TEMP_LOG_MS) {
        _lastLogMs += TEMP_LOG_MS;
        record(temp);
    }

    _slope = computeSlope();
    _pred = (int16_t)(temp + (int32_t)_slope * _t.aheadMin / 10);

    if (paused) {
        _wasPaused = true;
        return 0;
    }
    if (_wasPaused) {
        _wasPaused = false;
        resetStep();                      // 메뉴에서 나오면 처음 크기부터
        _lastDecisionMs = now;
        return 0;
    }
    if (now - _lastDecisionMs < (uint32_t)_t.periodS * 1000) return 0;
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
            // CLOSE에서는 불감대 안으로 들어오면 0.0초로 (단계 횟수에 넣지 않음)
            if (_pred >= lo) { j.type = JobType::MOVE; j.target = 0; make = true; }
        } else if (v[i].posState == PosState::UNKNOWN) {
            if (dir > 0) { j.type = JobType::MOVE; j.target = 0; make = true; }
            else if (dir < 0) { j.type = JobType::CLOSE; make = true; }
        } else if (dir > 0) {
            cs_t t = roundStep(v[i].pos + size);
            if (t > v[i].limit) t = v[i].limit;
            if (t > v[i].pos) { j.type = JobType::MOVE; j.target = t; make = true; counted = true; }
        } else if (dir < 0) {
            if (v[i].pos <= 0) {
                j.type = JobType::CLOSE; make = true;           // 0.0초에서 다음 주기에도 낮으면 CLOSE
            } else {
                cs_t t = v[i].pos - size;
                t = t <= 0 ? 0 : roundStep(t);
                if (t >= v[i].pos) t = v[i].pos - CS_STEP < 0 ? 0 : v[i].pos - CS_STEP;
                j.type = JobType::MOVE; j.target = t; make = true; counted = true;
            }
        }
        if (make) jobs[n++] = j;
    }
    if (counted) {
        if (_streak < 255) _streak++;
        _lastDir = dir;
    }
    return n;
}
