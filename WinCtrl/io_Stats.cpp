#include "io_Stats.h"

static_assert(sizeof(StatSummary) == 24, "StatSummary는 24바이트");
static_assert(sizeof(StatRaw) == 16, "StatRaw는 16바이트");

namespace StatMath {
uint32_t isqrt(uint64_t v) {
    uint64_t r = 0, bit = (uint64_t)1 << 62;
    while (bit > v) bit >>= 2;
    while (bit) {
        if (v >= r + bit) { v -= r + bit; r = (r >> 1) + bit; }
        else r >>= 1;
        bit >>= 2;
    }
    return (uint32_t)r;
}
}

void Stats::second(const StatSample& s) {
    int16_t err = (int16_t)(s.temp - s.setT);
    bool band = s.active && (err <= s.deadband && err >= -s.deadband);
    Cell c = { err, (uint8_t)((s.active ? 1 : 0) | (band ? 2 : 0) | (s.moving ? 4 : 0) | (s.moveStart ? 8 : 0)) };
    _ring[_head] = c;
    _head = (uint16_t)((_head + 1) % WINDOW);
    if (_filled < WINDOW) _filled++;

    _secs++;
    if (s.moving) _moving++;
    if (s.moveStart) _moves++;
    if (s.warn) _warn = true;
    if (s.pos1 >= 0) { _sumP1 += (uint64_t)s.pos1; _nP1++; }
    if (s.pos2 >= 0) { _sumP2 += (uint64_t)s.pos2; _nP2++; }
    _lastSet = s.setT;
    if (s.gain > 0) _lastGain = s.gain;
    if (s.active) {
        if (_active == 0) { _maxOver = err; _maxUnder = err; }
        if (err > _maxOver) _maxOver = err;
        if (err < _maxUnder) _maxUnder = err;
        _active++;
        if (band) _inBand++;
        _sumSq += (int64_t)err * err;
        _sumT += s.temp;
    }
}

StatRolling Stats::rolling() const {
    StatRolling r = {};
    int64_t sq = 0;
    uint16_t band = 0;
    bool first = true;
    for (uint16_t i = 0; i < _filled; i++) {
        const Cell& c = _ring[i];
        if (c.flags & 4) r.motorS++;
        if (c.flags & 8) r.moves++;
        if (!(c.flags & 1)) continue;
        if (first) { r.maxOver = c.err; r.maxUnder = c.err; first = false; }
        if (c.err > r.maxOver) r.maxOver = c.err;
        if (c.err < r.maxUnder) r.maxUnder = c.err;
        sq += (int64_t)c.err * c.err;
        if (c.flags & 2) band++;
        r.n++;
    }
    if (r.n) {
        r.rmsC = (uint16_t)StatMath::isqrt((uint64_t)sq * 100 / r.n);     // 0.1도² → 0.01도
        r.inBand = (uint8_t)(band * 100u / r.n);
    }
    return r;
}

static int8_t clamp8(int32_t v) { return (int8_t)(v > 127 ? 127 : (v < -128 ? -128 : v)); }

void Stats::finish(uint32_t time, bool rtc, StatSummary& o) {
    o = {};
    o.time = time;
    o.setT = _lastSet;
    if (_active) {
        o.avgT = (int16_t)(_sumT / (int64_t)_active);
        uint32_t rms = StatMath::isqrt((uint64_t)_sumSq * 100 / _active);
        o.rmsC = (uint16_t)(rms > 65535 ? 65535 : rms);
        o.inBand = (uint8_t)(_inBand * 100u / _active);
        o.maxOver = clamp8(_maxOver);
        o.maxUnder = clamp8(_maxUnder);
    }
    o.moves = (uint8_t)(_moves > 255 ? 255 : _moves);
    o.motorS = (uint16_t)(_moving > 65535 ? 65535 : _moving);
    o.pos1 = _nP1 ? (uint16_t)(_sumP1 / _nP1) : 0xFFFF;
    o.pos2 = _nP2 ? (uint16_t)(_sumP2 / _nP2) : 0xFFFF;
    o.gain10 = (uint16_t)(_lastGain / 10 > 65535 ? 65535 : _lastGain / 10);
    o.activePct = (uint8_t)(_secs ? _active * 100u / _secs : 0);
    o.flags = (uint8_t)((_warn ? 1 : 0) | (rtc ? 2 : 0));
    _secs = _active = _inBand = _moving = _moves = 0;
    _sumSq = _sumT = 0;
    _maxOver = _maxUnder = 0;
    _sumP1 = _sumP2 = 0;
    _nP1 = _nP2 = 0;
    _warn = false;
}

void RawLog::add(const StatRaw& r) {
    if (_cap == 0) return;
    _buf[_head] = r;
    _head = (_head + 1) % _cap;
    if (_count < _cap) _count++;
}

const StatRaw& RawLog::at(uint32_t i) const {
    uint32_t start = (_head + _cap - _count) % _cap;
    return _buf[(start + i) % _cap];
}
