// ============================================================
//  부품 시험: 온도에 따른 영점 흐름 (저울 + RTC 온도 + 전자잉크)
//  pio run -e test_drift -t upload
//
//  켜면 영점을 새로 잡는다 (저장된 영점은 쓰지도 바꾸지도 않는다).
//  그 뒤 아무것도 올리지 않고 둔다.
//  1분마다 DS3231 칩 온도와 영점에서 달라진 무게(g)를 재서, 온도 0.5도 칸마다 평균을 모은다.
//  10분마다 전자잉크에 재 본 온도 칸만 정리해 보인다 (전체 갱신).
//
//  화면
//    위 줄: 지금 온도와 지금 흐름(g), 1도당 몇 g인지 (모든 기록으로 구한 기울기)
//    아래 표: "온도 흐름(g)" 칸들. 낮은 온도부터. 칸이 많으면 지금 온도에서 먼 칸을 뺀다.
//
//  - 1g당 읽음값은 보드에 저장된 보정값(io_Calib)을 쓴다. 보정 전이면 g 대신 읽음값 그대로다.
//  - 켠 직후에는 회로가 데워지면서 영점이 흐른다. 처음 15~20분 값은 따로 보세요.
//  - DS3231 온도는 보드 온도다. 로드셀이 놓인 곳의 온도와 다를 수 있다.
//  - 저울은 계속 깨어 있다 (본 펌웨어는 10분마다 깨어 잠깐 잰다).
//
//  시리얼 명령 (끝에 엔터, 시리얼 모니터를 열었을 때만):
//    t   영점을 지금 값으로 다시 잡고 모은 기록을 지운다
//    s   화면을 지금 바로 갱신
// ============================================================
#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <RTClib.h>
#include <GxEPD2_BW.h>
#include <U8g2_for_Adafruit_GFX.h>
#include "pins.h"
#include "config.h"
#include "dev_Scale.h"
#include "io_Calib.h"

static const uint8_t AVG_N = 20;                // 한 번 잴 때 평균 낼 횟수 (10회/초라 약 2초)
static const uint32_t SAMPLE_MS = 60000UL;      // 재는 간격
static const uint32_t SCREEN_MS = 600000UL;     // 화면 갱신 간격

// 온도 칸: -10도부터 50도까지 0.5도 간격
static const float BIN_MIN = -10.0f;
static const float BIN_STEP = 0.5f;
static const int BIN_COUNT = 121;
static float binSum[BIN_COUNT];
static uint16_t binN[BIN_COUNT];

// 1도당 흐름(기울기)을 구하려고 모으는 합
static double sT, sG, sTT, sTG;
static uint32_t sN;

static GxEPD2_BW<EPD_DRIVER, EPD_DRIVER::HEIGHT> display(
    EPD_DRIVER(PIN_EPD_CS, PIN_EPD_DC, PIN_EPD_RST, PIN_EPD_BUSY));
static U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;

static RTC_DS3231 rtc;
static bool rtcOk = false;
static ScaleCalib cal;

static int32_t zeroA = 0;
static bool zeroOk = false;
static bool lastOk = false;     // 마지막으로 잴 때 저울이 응답했는지
static float lastTemp = NAN;
static float lastG = 0;

// ---- 재기 ----

static void clearRecords() {
    for (int i = 0; i < BIN_COUNT; i++) {
        binSum[i] = 0;
        binN[i] = 0;
    }
    sT = sG = sTT = sTG = 0;
    sN = 0;
}

static void zeroNow() {
    ScaleRaw r;
    zeroOk = scaleReadAvg(r, AVG_N);
    if (zeroOk) zeroA = r.a;
    clearRecords();
    Serial.printf("[영점] %s  1라인 %ld\n", zeroOk ? "잡음" : "실패: HX711이 응답하지 않음", (long)zeroA);
}

static void sample() {
    ScaleRaw r;
    lastOk = zeroOk && scaleReadAvg(r, AVG_N);
    lastTemp = rtcOk ? rtc.getTemperature() : NAN;
    if (!lastOk) return;
    lastG = (r.a - zeroA) / cal.perG[0];
    if (isnan(lastTemp)) return;

    int i = (int)lroundf((lastTemp - BIN_MIN) / BIN_STEP);
    if (i >= 0 && i < BIN_COUNT) {
        binSum[i] += lastG;
        binN[i]++;
    }
    sT += lastTemp;
    sG += lastG;
    sTT += (double)lastTemp * lastTemp;
    sTG += (double)lastTemp * lastG;
    sN++;
}

// 모든 기록으로 구한 1도당 흐름. 온도 폭이 너무 좁으면 false.
static bool slope(float& out) {
    if (sN < 2) return false;
    double var = sTT - sT * sT / sN;     // 온도 흩어짐 × 개수
    if (var / sN < 0.04) return false;   // 온도 폭이 0.2도쯤 안이면 기울기를 믿을 수 없다
    out = (float)((sTG - sT * sG / sN) / var);
    return true;
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

static const int COLS = 4, ROWS = 9;
static const int16_t COL_W = 74, ROW_H = 12, GRID_Y = 31;

static void drawScreen() {
    display.fillScreen(GxEPD_WHITE);
    char buf[40];

    // 위 줄 왼쪽: 지금 온도와 흐름
    setText(u8g2_font_unifont_t_korean2);
    if (!rtcOk) {
        snprintf(buf, sizeof(buf), "시계 없음");
    } else if (!lastOk) {
        snprintf(buf, sizeof(buf), "지금 %.1f도 저울 응답 없음", lastTemp);
    } else {
        snprintf(buf, sizeof(buf), "지금 %.1f도 %+.0fg", lastTemp, lastG);
    }
    textAt(2, 14, buf);

    // 위 줄 오른쪽: 1도당 흐름
    float k;
    if (slope(k)) snprintf(buf, sizeof(buf), "1도당 %+.1fg", k);
    else snprintf(buf, sizeof(buf), "1도당 ?");
    textAt(display.width() - 2 - u8g2Fonts.getUTF8Width(buf), 14, buf);
    display.drawFastHLine(0, 18, display.width(), GxEPD_BLACK);

    // 재 본 온도 칸 모으기
    int idx[BIN_COUNT];
    int n = 0;
    for (int i = 0; i < BIN_COUNT; i++) {
        if (binN[i] > 0) idx[n++] = i;
    }
    if (n == 0) {
        textAt(2, 50, "기록을 모으는 중");
        return;
    }
    // 칸이 너무 많으면 지금 온도에서 먼 쪽부터 뺀다
    int first = 0, last = n - 1;
    float nowBin = isnan(lastTemp) ? 0 : (lastTemp - BIN_MIN) / BIN_STEP;
    while (last - first + 1 > COLS * ROWS) {
        if (fabsf(idx[first] - nowBin) > fabsf(idx[last] - nowBin)) first++;
        else last--;
    }

    // 표: 위에서 아래로, 그다음 오른쪽 열
    setText(u8g2_font_6x12_tf);
    for (int j = first; j <= last; j++) {
        int k2 = j - first;
        int col = k2 / ROWS, row = k2 % ROWS;
        int i = idx[j];
        float t = BIN_MIN + i * BIN_STEP;
        snprintf(buf, sizeof(buf), "%4.1f %+5.0f", t, binSum[i] / binN[i]);
        textAt(col * COL_W + 2, GRID_Y + row * ROW_H, buf);
    }
    for (int c = 1; c < COLS; c++) {
        display.drawFastVLine(c * COL_W - 2, 21, display.height() - 21, GxEPD_BLACK);
    }
}

static void showScreen() {
    display.setFullWindow();
    display.firstPage();
    do {
        drawScreen();
    } while (display.nextPage());
    display.powerOff();   // 10분에 한 번이라 갱신 뒤 패널 전원을 끈다
}

// ---- 시작과 반복 ----

static void handleCommand(String line) {
    line.trim();
    if (line == "t") {
        zeroNow();
        sample();
        showScreen();
    } else if (line == "s") {
        showScreen();
    } else if (line.length() > 0) {
        Serial.println("명령: t(영점 다시, 기록 지우기), s(화면 바로 갱신)");
    }
}

void setup() {
    Serial.begin(115200);
    Serial.setTxTimeoutMs(0);   // 시리얼 모니터를 안 열어도 기다리지 않게 (안 그러면 한 줄에 0.1초씩 멈춘다)
    delay(1500);
    Serial.printf("\n[test_drift] %s\n", DRAINMETER_VERSION);

    // 읽기선(MISO)은 쓰지 않는다. D9는 BUSY로 쓴다.
    SPI.begin(PIN_EPD_SCK, -1, PIN_EPD_DIN, -1);
    display.init(115200, true, 2, false);
    display.setRotation(1);   // 가로로 놓고 쓴다 (296x128)
    u8g2Fonts.begin(display);

    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    rtcOk = rtc.begin(&Wire);
    if (rtcOk) {
        rtc.disable32K();
        rtc.writeSqwPinMode(DS3231_OFF);
    }

    calibLoad(cal);
    scaleBegin();
    scaleWake();
    zeroNow();
    sample();
    showScreen();
}

void loop() {
    if (Serial.available()) {
        handleCommand(Serial.readStringUntil('\n'));
    }
    uint32_t now = millis();
    static uint32_t lastSample = now, lastScreen = now;
    if (now - lastSample >= SAMPLE_MS) {
        lastSample = now;
        sample();
    }
    if (now - lastScreen >= SCREEN_MS) {
        lastScreen = now;
        showScreen();
    }
    delay(10);
}
