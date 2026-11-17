// ============================================================
//  부품 시험: 로드셀 2세트 (HX711 2개, 클럭 공용)
//  pio run -e test_scale -t upload, 그다음 pio device monitor
//
//  1초마다 두 저울의 읽음값과 무게(g), 흔들림(최근 10개 최댓값-최솟값)을 찍는다.
//  시리얼 명령 (끝에 엔터):
//    t          두 저울 영점 맞추기 (아무것도 안 올린 상태에서)
//    c1 1000    1라인 저울에 1000g을 올려 놓고 보정 → 1g당 읽음값을 찍는다
//    c2 1000    2라인 저울 보정
//    z          쉬기/깨기 시험 (쉬는 동안 읽기를 멈춘다)
//    x          저장된 영점과 보정값 지우기
//  영점과 보정값은 보드에 저장돼 전원을 꺼도 남는다 (io_Calib). test_live와 같은 값을 쓴다.
//  저장된 영점이 있으면 켤 때 영점을 다시 잡지 않는다. 없으면 켤 때 잡아서 저장한다.
// ============================================================
#include <Arduino.h>
#include "pins.h"
#include "config.h"
#include "dev_Scale.h"
#include "io_Calib.h"

static ScaleCalib cal;

static const uint8_t HIST = 10;
static float histA[HIST], histB[HIST];
static uint8_t histN = 0, histI = 0;
static bool sleeping = false;

static float spread(const float* h, uint8_t n) {
    if (n == 0) return 0;
    float lo = h[0], hi = h[0];
    for (uint8_t i = 1; i < n; i++) {
        if (h[i] < lo) lo = h[i];
        if (h[i] > hi) hi = h[i];
    }
    return hi - lo;
}

static void tare() {
    ScaleRaw r;
    if (!scaleReadAvg(r, 20)) {
        Serial.println("[tare] 실패: HX711이 응답하지 않음");
        return;
    }
    cal.offset[0] = r.a;
    cal.offset[1] = r.b;
    cal.hasOffset = true;
    calibSaveOffset(cal);
    histN = histI = 0;
    Serial.printf("[tare] 영점 A=%ld B=%ld (저장함)\n", (long)r.a, (long)r.b);
}

static void calibrate(uint8_t which, float grams) {
    if (grams <= 0) {
        Serial.println("[cal] 무게를 g으로 적어 주세요. 예: c1 1000");
        return;
    }
    ScaleRaw r;
    if (!scaleReadAvg(r, 20)) {
        Serial.println("[cal] 실패: HX711이 응답하지 않음");
        return;
    }
    uint8_t i = which - 1;
    int32_t net = ((which == 1) ? r.a : r.b) - cal.offset[i];
    float perG = net / grams;
    if (perG > -0.001f && perG < 0.001f) {
        Serial.println("[cal] 실패: 무게를 올렸는데 읽음값이 거의 그대로입니다. 저장하지 않았습니다.");
        return;
    }
    cal.perG[i] = perG;
    cal.hasPerG[i] = true;
    calibSavePerG(cal, i);
    histN = histI = 0;
    Serial.printf("[cal] %d라인: %.0fg에 %ld → 1g당 %.3f (저장함)\n", which, grams, (long)net, perG);
    if (perG < 0) Serial.println("      값이 음수입니다. 무게는 맞게 나오지만, A+와 A-를 바꿔 연결하면 양수가 됩니다.");
}

static void handleCommand(String line) {
    line.trim();
    if (line.length() == 0) return;
    char c = line.charAt(0);
    if (c == 't') {
        tare();
    } else if (c == 'c' && line.length() >= 2) {
        uint8_t which = line.charAt(1) - '0';
        float grams = line.substring(2).toFloat();
        if (which == 1 || which == 2) calibrate(which, grams);
        else Serial.println("[cal] c1 또는 c2로 적어 주세요");
    } else if (c == 'z') {
        if (sleeping) {
            uint32_t t0 = millis();
            scaleWake();
            sleeping = false;
            Serial.printf("[sleep] 깨어남 (%lums)\n", (unsigned long)(millis() - t0));
        } else {
            scaleSleep();
            sleeping = true;
            Serial.println("[sleep] 쉬는 중. 다시 z를 누르면 깨어남");
        }
    } else if (c == 'x') {
        calibClear();
        calibLoad(cal);
        histN = histI = 0;
        Serial.println("[cal] 저장값을 지웠습니다. 영점은 t, 보정은 c1/c2로 다시 하세요.");
    } else {
        Serial.println("명령: t, c1 <g>, c2 <g>, z, x");
    }
}

void setup() {
    Serial.begin(115200);
    Serial.setTxTimeoutMs(0);   // 시리얼 모니터를 안 열어도 기다리지 않게 (안 그러면 한 줄에 0.1초씩 멈춘다)
    delay(1500);
    Serial.printf("\n[test_scale] %s\n", DRAINMETER_VERSION);
    Serial.println("명령: t(영점), c1 <g>, c2 <g>(보정), z(쉬기/깨기), x(저장값 지우기)");
    calibLoad(cal);
    Serial.printf("[cal] 저장된 영점: %s  A=%ld B=%ld\n", cal.hasOffset ? "있음" : "없음",
                  (long)cal.offset[0], (long)cal.offset[1]);
    for (uint8_t i = 0; i < 2; i++) {
        Serial.printf("[cal] %d라인 1g당 %.3f (%s)\n", i + 1, cal.perG[i],
                      cal.hasPerG[i] ? "저장값" : "보정 전");
    }
    scaleBegin();
    scaleWake();
    if (!cal.hasOffset) tare();
}

void loop() {
    if (Serial.available()) {
        handleCommand(Serial.readStringUntil('\n'));
    }
    if (sleeping) {
        delay(50);
        return;
    }

    static uint32_t last = 0;
    if (millis() - last < 1000) {
        delay(5);
        return;
    }
    last = millis();

    ScaleRaw r;
    if (!scaleReadAvg(r, 4)) {
        Serial.printf("[scale] 응답 없음  DOUT1=%d DOUT2=%d (둘 다 0이어야 준비)\n",
                      digitalRead(PIN_HX1_DOUT), digitalRead(PIN_HX2_DOUT));
        return;
    }
    float gA = (r.a - cal.offset[0]) / cal.perG[0];
    float gB = (r.b - cal.offset[1]) / cal.perG[1];
    histA[histI] = gA;
    histB[histI] = gB;
    histI = (histI + 1) % HIST;
    if (histN < HIST) histN++;

    Serial.printf("A raw=%9ld  %9.1fg (흔들림 %.1f) | B raw=%9ld  %9.1fg (흔들림 %.1f)\n",
                  (long)r.a, gA, spread(histA, histN),
                  (long)r.b, gB, spread(histB, histN));
}
