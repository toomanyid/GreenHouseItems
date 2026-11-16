#include "io_Calib.h"
#include <Preferences.h>
#include "config.h"

static const char* NS = "scale";   // NVS 이름 공간
static const char* KEY_OFF[2] = {"off1", "off2"};
static const char* KEY_PERG[2] = {"perg1", "perg2"};

void calibLoad(ScaleCalib& c) {
    const float defPerG[2] = {SCALE1_COUNTS_PER_G, SCALE2_COUNTS_PER_G};
    Preferences p;
    p.begin(NS, false);   // 읽기 전용으로 열면 처음(저장값 없음)에 오류 줄이 찍혀서 쓰기로 연다
    c.hasOffset = p.isKey(KEY_OFF[0]) && p.isKey(KEY_OFF[1]);
    for (uint8_t i = 0; i < 2; i++) {
        c.offset[i] = p.getInt(KEY_OFF[i], 0);
        c.hasPerG[i] = p.isKey(KEY_PERG[i]);
        // 없는 키를 getFloat로 읽으면 오류 줄이 찍혀서, 있을 때만 읽는다
        c.perG[i] = c.hasPerG[i] ? p.getFloat(KEY_PERG[i], defPerG[i]) : defPerG[i];
    }
    p.end();
}

void calibSaveOffset(const ScaleCalib& c) {
    Preferences p;
    p.begin(NS, false);
    p.putInt(KEY_OFF[0], c.offset[0]);
    p.putInt(KEY_OFF[1], c.offset[1]);
    p.end();
}

void calibSavePerG(const ScaleCalib& c, uint8_t line) {
    if (line > 1) return;
    Preferences p;
    p.begin(NS, false);
    p.putFloat(KEY_PERG[line], c.perG[line]);
    p.end();
}

void calibClear() {
    Preferences p;
    p.begin(NS, false);
    p.clear();
    p.end();
}
