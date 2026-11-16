// ============================================================
//  부품 시험: 저울 + 전자잉크 (지금 무게를 화면에 보이기)
//  pio run -e test_live -t upload, 그다음 pio device monitor
//
//  1초마다 두 저울의 무게(kg)를 화면에 부분 갱신으로 그린다.
//  부분 갱신을 거듭하면 그대로 있는 글자가 조금씩 옅어진다 (이 패널의 부분 갱신 방식).
//  그래서 60번(1분)마다 한 번은 전체 갱신해 진하게 되돌린다.
//  시리얼 명령 (끝에 엔터):
//    t          두 저울 영점 맞추기 (아무것도 안 올린 상태에서)
//    c1 1000    1라인 저울에 1000g을 올려 놓고 보정
//    c2 1000    2라인 저울 보정
//    f 60       부분 갱신 몇 번마다 전체 갱신할지 (1이면 늘 전체 갱신)
//    i 5000     갱신 간격 (ms)
//    p          갱신 뒤 패널 전원 끄기 / 켜 두기 바꾸기 (처음은 켜 두기)
//    x          저장된 영점과 보정값 지우기
//  영점과 보정값은 보드에 저장돼 전원을 꺼도 남는다 (io_Calib).
//  저장된 영점이 있으면 켤 때 영점을 다시 잡지 않는다. 없으면 켤 때 잡아서 저장한다.
// ============================================================
#include <Arduino.h>
#include <SPI.h>
#include <GxEPD2_BW.h>
#include <U8g2_for_Adafruit_GFX.h>
#include "pins.h"
#include "config.h"
#include "dev_Scale.h"
#include "io_Calib.h"

static uint32_t updateMs = 1000;            // 화면 갱신 간격 (i 명령으로 바꾼다)
static bool powerOffAfter = false;          // 갱신 뒤 패널 전원을 끌지 (p 명령으로 바꾼다)
static uint16_t fullEvery = 60;             // 이만큼 갱신하면 한 번은 전체 갱신 (f 명령으로 바꾼다)

static GxEPD2_BW<EPD_DRIVER, EPD_DRIVER::HEIGHT> display(
    EPD_DRIVER(PIN_EPD_CS, PIN_EPD_DC, PIN_EPD_RST, PIN_EPD_BUSY));
static U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;

static ScaleCalib cal;
static uint32_t updates = 0;

// ---- 저울 ----

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
    } else if (c == 'f') {
        int n = line.substring(1).toInt();
        if (n >= 1) {
            fullEvery = n;
            Serial.printf("[epd] %d번마다 전체 갱신\n", n);
        } else {
            Serial.println("[epd] 1 이상으로 적어 주세요. 예: f 12");
        }
    } else if (c == 'i') {
        int ms = line.substring(1).toInt();
        if (ms >= 500) {
            updateMs = ms;
            Serial.printf("[epd] %dms마다 갱신\n", ms);
        } else {
            Serial.println("[epd] 500 이상으로 적어 주세요. 예: i 5000");
        }
    } else if (c == 'p') {
        powerOffAfter = !powerOffAfter;
        Serial.printf("[epd] 갱신 뒤 패널 전원: %s\n", powerOffAfter ? "끄기" : "켜 두기");
    } else if (c == 'x') {
        calibClear();
        calibLoad(cal);
        Serial.println("[cal] 저장값을 지웠습니다. 영점은 t, 보정은 c1/c2로 다시 하세요.");
    } else {
        Serial.println("명령: t(영점), c1 <g>, c2 <g>(보정), x(저장값 지우기), f <n>(전체 갱신 간격), i <ms>(갱신 간격), p(패널 전원)");
    }
}

// ---- 화면 ----

static void setText(const uint8_t* font) {
    u8g2Fonts.setFont(font);
    u8g2Fonts.setFontMode(1);
    u8g2Fonts.setForegroundColor(GxEPD_BLACK);
    u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
}

static void textAt(int16_t x, int16_t y, const char* s) {
    u8g2Fonts.setCursor(x, y);
    u8g2Fonts.print(s);
}

// 한 라인 칸: 이름, 큰 숫자(kg), 아래에 보정 여부
static void drawLine(int16_t x, const char* name, bool ok, float kg, bool calibrated) {
    setText(u8g2_font_unifont_t_korean2);
    textAt(x, 16, name);

    char buf[16];
    if (!ok) {
        snprintf(buf, sizeof(buf), "--");
    } else if (kg > 999.999f || kg < -99.999f) {
        snprintf(buf, sizeof(buf), "넘침");   // 보정 전이면 숫자가 너무 커서 칸을 넘는다
    } else {
        snprintf(buf, sizeof(buf), "%.3f", kg);
    }
    if (ok && strcmp(buf, "넘침") != 0) {
        // 칸 너비(144) 안에 숫자와 "kg"가 들어가지 않으면 작은 글꼴로 바꾼다
        setText(u8g2_font_logisoso32_tn);
        int16_t w = u8g2Fonts.getUTF8Width(buf);
        if (w + 4 + 16 > 142) {
            setText(u8g2_font_logisoso24_tn);
            w = u8g2Fonts.getUTF8Width(buf);
        }
        textAt(x, 68, buf);
        setText(u8g2_font_unifont_t_korean2);
        textAt(x + w + 4, 68, "kg");
    } else {
        setText(u8g2_font_unifont_t_korean2);
        textAt(x, 60, ok ? buf : "응답 없음");
    }

    setText(u8g2_font_unifont_t_korean2);
    if (!calibrated) textAt(x, 90, "보정 전");
}

static void drawScreen(bool ok, float kgA, float kgB) {
    display.fillScreen(GxEPD_WHITE);
    drawLine(4, "1라인", ok, kgA, cal.hasPerG[0]);
    drawLine(152, "2라인", ok, kgB, cal.hasPerG[1]);
    display.drawFastVLine(148, 0, 98, GxEPD_BLACK);
    display.drawFastHLine(0, 102, display.width(), GxEPD_BLACK);

    setText(u8g2_font_unifont_t_korean2);
    char buf[40];
    snprintf(buf, sizeof(buf), "갱신 %lu  %.1f초  전원 %s", (unsigned long)updates,
             updateMs / 1000.0f, powerOffAfter ? "off" : "on");
    textAt(4, 122, buf);
}

static void showWeights() {
    ScaleRaw r;
    bool ok = scaleReadAvg(r, 3);   // 10회/초라 3번에 약 0.3초. 1초 안에 화면까지 그리려고 줄였다
    float kgA = ok ? (r.a - cal.offset[0]) / cal.perG[0] / 1000.0f : 0;
    float kgB = ok ? (r.b - cal.offset[1]) / cal.perG[1] / 1000.0f : 0;
    updates++;

    if (ok) {
        Serial.printf("[%lu] 1라인 %.3fkg (raw %ld) | 2라인 %.3fkg (raw %ld)\n",
                      (unsigned long)updates, kgA, (long)r.a, kgB, (long)r.b);
    } else {
        Serial.printf("[%lu] 저울 응답 없음  DOUT1=%d DOUT2=%d\n", (unsigned long)updates,
                      digitalRead(PIN_HX1_DOUT), digitalRead(PIN_HX2_DOUT));
    }

    static uint32_t sinceFull = 0;
    bool full = (updates == 1) || (++sinceFull >= fullEvery);   // 처음과 그 뒤 fullEvery번마다
    if (full) sinceFull = 0;
    uint32_t t0 = millis();
    if (full) display.setFullWindow();
    else display.setPartialWindow(0, 0, display.width(), display.height());
    display.firstPage();
    do {
        drawScreen(ok, kgA, kgB);
    } while (display.nextPage());
    if (powerOffAfter) display.powerOff();   // 끄면 다음 갱신 때 다시 켜느라 0.1초쯤 더 걸린다
    Serial.printf("      %s 갱신 %lums\n", full ? "전체" : "부분", (unsigned long)(millis() - t0));
}

void setup() {
    Serial.begin(115200);
    delay(1500);
    Serial.printf("\n[test_live] %s\n", DRAINMETER_VERSION);
    Serial.println("명령: t(영점), c1 <g>, c2 <g>(보정), x(저장값 지우기), f <n>(전체 갱신 간격), i <ms>(갱신 간격), p(패널 전원)");

    // 읽기선(MISO)은 쓰지 않는다. D9는 BUSY로 쓴다.
    SPI.begin(PIN_EPD_SCK, -1, PIN_EPD_DIN, -1);
    display.init(115200, true, 2, false);
    display.setRotation(1);   // 가로로 놓고 쓴다 (296x128)
    u8g2Fonts.begin(display);

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
    showWeights();
}

void loop() {
    if (Serial.available()) {
        handleCommand(Serial.readStringUntil('\n'));
    }
    static uint32_t last = millis();
    if (millis() - last >= updateMs) {
        last = millis();
        showWeights();
    }
    delay(10);
}
