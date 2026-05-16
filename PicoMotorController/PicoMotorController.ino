/**
 * Raspberry Pi Pico (Arduino IDE) — 모터 2채 + SH1106 + EC11 + ACS758-050B
 *
 * 보드: Earle Philhower "Raspberry Pi Pico" 등 RP2040 Arduino 코어.
 * 라이브러리: U8g2 (U8g2lib)
 *
 * 핀
 *   OLED I²C: SDA GP6, SCL GP7 (RP2040 I²C1 — Wire1 하드웨어 권장, 아래 OLED_CTRL)
 *   엔코더 TRA/TRB: GP3, GP4  | PSH GP2 (5초 홀드=SETUP), CONFIRM GP5, BACK GP10
 *   전류 ADC: GP26
 *   모터1: GP16(L1), GP17(L2)  | 모터2: GP18(L1), GP19(L2)
 *
 * 드라이버(EN=0): 정 L1=0·L2=PWM, 역 L1=PWM·L2=0, 정지는 L1=L2=LOW.
 * EN 핀을 MCU에 연결한 경우 MOTOR_EN_PIN 을 설정하세요.
 */

#include <Arduino.h>
#include <math.h>
#include <Wire.h>
#include <U8g2lib.h>

#ifndef DBG_SERIAL_BAUD
#define DBG_SERIAL_BAUD 115200
#endif

/** 1이면 setup에서 Wire1 I²C 스캔. SW OLED(OLED_CTRL 2·3)와 동시 사용 금지 */
#ifndef DBG_I2C_SCAN
#define DBG_I2C_SCAN 0
#endif

/**
 * OLED_CTRL: 0=SH1106+Wire1 1=SSD1306+Wire1 2=SH1106 SW 3=SSD1306 SW
 * GP6/7은 I²C1 → Wire1(2ND_HW) 권장.
 */
#ifndef OLED_CTRL
#define OLED_CTRL 0
#endif
/** 1=풀 프레임 버퍼(_F_). 0=1페이지(_1_)만 — 깨짐 시 1 권장(RAM ~1KB) */
#ifndef OLED_USE_FULL_BUFFER
#define OLED_USE_FULL_BUFFER 1
#endif
#ifndef OLED_I2C_CLOCK_HZ
#define OLED_I2C_CLOCK_HZ 400000
#endif
/** OLED 전체 갱신 최소 간격(ms). 너무 짧으면 I²C만 과busy */
#ifndef OLED_REDRAW_MS
#define OLED_REDRAW_MS 50
#endif
#ifndef OLED_SWAP_SDA_SCL
#define OLED_SWAP_SDA_SCL 0
#endif

#define DBG_PRINT(x) Serial.print(x)
#define DBG_PRINTLN(x) Serial.println(x)

// ----------------------------- 핀 -----------------------------
static const int PIN_OLED_SDA = 6;
static const int PIN_OLED_SCL = 7;

static const int PIN_ENC_A = 3;
static const int PIN_ENC_B = 4;
static const int PIN_ENC_SW = 2;
static const int PIN_BTN_CONFIRM = 5;
static const int PIN_BTN_BACK = 10;

static const int PIN_CURRENT = 26;

static const int PIN_M1_L1 = 16;
static const int PIN_M1_L2 = 17;
static const int PIN_M2_L1 = 18;
static const int PIN_M2_L2 = 19;

/** 미사용 시 -1 */
static const int MOTOR_EN_PIN = -1;

// ----------------------------- 튜닝 -----------------------------
static const uint32_t ENCODER_EDIT_END_MS = 80;   // 마지막 스텝 후 → ARM
static const uint32_t ENCODER_ARM_START_MS = 2000;
static const uint32_t DIR_CHANGE_COOLDOWN_MS = 3000;
static const float OVERCURRENT_A = 4.0f;
/** ACS758LCB-050B, 3.3V 전원 시 mV/A ≈ 40*3.3/5 */
static const float MV_PER_AMP = 40.0f * (3.3f / 5.0f);
static const int ADC_SAMPLES = 24;
static const uint32_t OC_TRIP_MS = 120;
static const uint8_t MOTOR_PWM_DUTY = 220;
/** 소프트 스타트: 이 시간(ms) 동안 PWM·누적 속도를 0→최대로 선형 증가 */
static const uint32_t SOFT_START_MS = 2000;
/** RUN 위치 적분·PWM 갱신 주기 (짧을수록 램프·누적이 매끄러움) */
static const uint32_t RUN_POSITION_TICK_MS = 50;
static const uint32_t ENC_SW_DEBOUNCE_MS = 40;
static const uint32_t SETUP_HOLD_MS = 5000;

/** 1: 회전 시 목표값 증감 방향 반전(TR/A·B 핀 교체와 동일). 0: 이전 방향 */
#ifndef ROTARY_ENCODER_REVERSE
#define ROTARY_ENCODER_REVERSE 1
#endif

static const uint8_t OLED_I2C_ADDR_8BIT = 0x3C << 1;

static void wire1OledPinsBegin() {
#if defined(ARDUINO_ARCH_RP2040)
  Wire1.setBufferSize(256);
#endif
  Wire1.setSDA(PIN_OLED_SDA);
  Wire1.setSCL(PIN_OLED_SCL);
  Wire1.begin();
  Wire1.setClock((uint32_t)OLED_I2C_CLOCK_HZ);
}

/** OLED과 같은 핀에서 ACK 되는 I²C 주소 확인 (Wire1, 디버그 전용). */
static void dbgI2cScanWire1() {
  DBG_PRINTLN(F("[S3] Wire1 I2C scan (SDA=GP6 SCL=GP7)..."));
  wire1OledPinsBegin();
  int found = 0;
  for (uint8_t a = 1; a < 127; a++) {
    Wire1.beginTransmission(a);
    uint8_t e = Wire1.endTransmission();
    if (e == 0) {
      found++;
      Serial.printf("       ACK at 0x%02X (7-bit) -> U8g2 8-bit addr 0x%02X\n", a,
                    (unsigned)(a << 1));
    }
  }
  if (found == 0)
    DBG_PRINTLN(F("       (no ACK — 배선·전원·풀업 확인)"));
  DBG_PRINTLN(F("[S4] I2C scan done"));
}

#if OLED_CTRL == 0
#if OLED_USE_FULL_BUFFER
U8G2_SH1106_128X64_NONAME_F_2ND_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
#else
U8G2_SH1106_128X64_NONAME_1_2ND_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
#endif
#elif OLED_CTRL == 1
#if OLED_USE_FULL_BUFFER
U8G2_SSD1306_128X64_NONAME_F_2ND_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
#else
U8G2_SSD1306_128X64_NONAME_1_2ND_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
#endif
#elif OLED_CTRL == 2
#if OLED_SWAP_SDA_SCL
U8G2_SH1106_128X64_NONAME_1_SW_I2C u8g2(U8G2_R0, PIN_OLED_SDA, PIN_OLED_SCL,
                                         U8X8_PIN_NONE);
#else
U8G2_SH1106_128X64_NONAME_1_SW_I2C u8g2(U8G2_R0, PIN_OLED_SCL, PIN_OLED_SDA,
                                         U8X8_PIN_NONE);
#endif
#elif OLED_CTRL == 3
#if OLED_SWAP_SDA_SCL
U8G2_SSD1306_128X64_NONAME_1_SW_I2C u8g2(U8G2_R0, PIN_OLED_SDA, PIN_OLED_SCL,
                                         U8X8_PIN_NONE);
#else
U8G2_SSD1306_128X64_NONAME_1_SW_I2C u8g2(U8G2_R0, PIN_OLED_SCL, PIN_OLED_SDA,
                                         U8X8_PIN_NONE);
#endif
#else
#error "OLED_CTRL must be 0..3"
#endif

/** UI 상태 (Arduino 자동 원형이 커스텀 typedef보다 앞에 오므로 uint8_t + 상수만 사용) */
enum {
  MUI_IDLE = 0,
  MUI_EDIT,
  MUI_ARM,
  MUI_DIR_WAIT,
  MUI_RUN,
  MUI_TRIP,
  MUI_SETUP
};

static const char* motorUiStateStr(uint8_t s) {
  switch (s) {
    case MUI_IDLE:
      return "IDLE";
    case MUI_EDIT:
      return "EDIT";
    case MUI_ARM:
      return "ARM";
    case MUI_DIR_WAIT:
      return "DIR_WAIT";
    case MUI_RUN:
      return "RUN";
    case MUI_TRIP:
      return "TRIP";
    case MUI_SETUP:
      return "SETUP";
    default:
      return "?";
  }
}

static uint8_t phase = MUI_IDLE;

static int32_t currentAbs = 0;
static int32_t targetAbs = 0;

static int8_t lastMotorDir = 0;
static uint32_t lastMotorStopMs = 0;

static uint32_t lastEncStepMs = 0;
static uint32_t armPhaseStartMs = 0;

static int8_t runDir = 0;
static uint32_t runStartMs = 0;
static uint32_t runLastPosTickMs = 0;
static float runStepAccum = 0;

static uint32_t ocStartMs = 0;

static uint8_t fanFrame = 0;
static uint32_t fanNextMs = 0;

static uint8_t encLastAB = 0;
static const int8_t encQuadTable[16] = {
    0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};
static int32_t encAccum = 0;

static uint32_t encSwLastChangeMs = 0;
static uint8_t encSwStable = 1;
static uint8_t encSwPrevRaw = 1;
static uint32_t encSwHeldSinceMs = 0;

static float vZero = 1.65f;

static uint32_t dbgSendBufferCount = 0;

// ----------------------------- 모터 -----------------------------
static void motorPinsInit() {
  pinMode(PIN_M1_L1, OUTPUT);
  pinMode(PIN_M1_L2, OUTPUT);
  pinMode(PIN_M2_L1, OUTPUT);
  pinMode(PIN_M2_L2, OUTPUT);
  if (MOTOR_EN_PIN >= 0) {
    pinMode(MOTOR_EN_PIN, OUTPUT);
    digitalWrite(MOTOR_EN_PIN, LOW);
  }
  motorAllStop();
}

static void motorAllStop() {
  analogWrite(PIN_M1_L1, 0);
  analogWrite(PIN_M1_L2, 0);
  analogWrite(PIN_M2_L1, 0);
  analogWrite(PIN_M2_L2, 0);
  digitalWrite(PIN_M1_L1, LOW);
  digitalWrite(PIN_M1_L2, LOW);
  digitalWrite(PIN_M2_L1, LOW);
  digitalWrite(PIN_M2_L2, LOW);
}

static void motorForward(uint8_t duty) {
  if (MOTOR_EN_PIN >= 0) digitalWrite(MOTOR_EN_PIN, LOW);
  digitalWrite(PIN_M1_L1, LOW);
  analogWrite(PIN_M1_L2, duty);
  digitalWrite(PIN_M2_L1, LOW);
  analogWrite(PIN_M2_L2, duty);
}

static void motorReverse(uint8_t duty) {
  if (MOTOR_EN_PIN >= 0) digitalWrite(MOTOR_EN_PIN, LOW);
  analogWrite(PIN_M1_L1, duty);
  digitalWrite(PIN_M1_L2, LOW);
  analogWrite(PIN_M2_L1, duty);
  digitalWrite(PIN_M2_L2, LOW);
}

static void markMotorStopped(int8_t dirJustRun) {
  motorAllStop();
  lastMotorStopMs = millis();
  if (dirJustRun != 0) lastMotorDir = dirJustRun;
}

static float softStartSpeed01(uint32_t elapsedMs) {
  if (elapsedMs >= SOFT_START_MS) return 1.0f;
  return elapsedMs / (float)SOFT_START_MS;
}

// ----------------------------- ADC -----------------------------
static float readAdcV() {
  uint32_t sum = 0;
  for (int i = 0; i < ADC_SAMPLES; i++) sum += analogRead(PIN_CURRENT);
  return (sum / (float)ADC_SAMPLES) * (3.3f / 4095.0f);
}

static float readAmps() {
  return (readAdcV() - vZero) / (MV_PER_AMP / 1000.0f);
}

static void calibrateZero() {
  delay(100);
  double s = 0;
  for (int i = 0; i < 80; i++) {
    s += readAdcV();
    delay(3);
  }
  vZero = (float)(s / 80.0);
}

// ----------------------------- 엔코더 -----------------------------
static void encoderInit() {
  pinMode(PIN_ENC_A, INPUT_PULLUP);
  pinMode(PIN_ENC_B, INPUT_PULLUP);
  pinMode(PIN_ENC_SW, INPUT_PULLUP);
  pinMode(PIN_BTN_CONFIRM, INPUT_PULLUP);
  pinMode(PIN_BTN_BACK, INPUT_PULLUP);
  encLastAB =
      (digitalRead(PIN_ENC_A) ? 1u : 0u) | (digitalRead(PIN_ENC_B) ? 2u : 0u);
  encSwPrevRaw = digitalRead(PIN_ENC_SW) ? 1u : 0u;
  encSwStable = encSwPrevRaw;
  encSwLastChangeMs = millis();
}

/** 한 디텐트 = ±1 (목표 절대값). true = 목표 변경됨 */
static bool encoderPollStep() {
  uint8_t ab =
      (digitalRead(PIN_ENC_A) ? 1u : 0u) | (digitalRead(PIN_ENC_B) ? 2u : 0u);
  if (ab == encLastAB) return false;
  uint8_t idx = (uint8_t)((encLastAB << 2) | ab);
  int8_t d = encQuadTable[idx];
#if ROTARY_ENCODER_REVERSE
  d = (int8_t)(-d);
#endif
  encLastAB = ab;
  if (d == 0) return false;
  encAccum += d;
  if (encAccum >= 4) {
    targetAbs += encAccum / 4;
    encAccum %= 4;
    return true;
  }
  if (encAccum <= -4) {
    targetAbs += encAccum / 4;
    encAccum %= 4;
    return true;
  }
  return false;
}

// ----------------------------- 화면 -----------------------------
/** dir > 0 정회전, dir < 0 역회전 — 모터 방향과 같은 쪽으로 팬이 돌아가게 */
static void drawFan(int cx, int cy, int8_t dir) {
  const int spin = (dir < 0) ? -1 : 1;
  for (int i = 0; i < 3; i++) {
    float ang = (fanFrame * 30 * spin + i * 120) * 3.1415926f / 180.0f;
    int x2 = cx + (int)(9 * cosf(ang));
    int y2 = cy + (int)(9 * sinf(ang));
    u8g2.drawLine(cx, cy, x2, y2);
  }
}

/** 표시용 -999~999, 선행 0 없음. bandTop~bandBottom 세로 구간 안에서 가로·세로 중앙 */
static void drawPositionLargeCentered(int32_t v, int bandTop, int bandBottom) {
  int32_t c = v;
  if (c > 999) c = 999;
  if (c < -999) c = -999;
  char buf[8];
  if (c < 0)
    snprintf(buf, sizeof(buf), "-%ld", (long)(-c));
  else
    snprintf(buf, sizeof(buf), "%ld", (long)c);

  u8g2.setFont(u8g2_font_logisoso22_tf);
  int bandH = bandBottom - bandTop;
  if (bandH < 8) bandH = 8;
  int bandMid = bandTop + bandH / 2;
  int baselineY =
      bandMid + (int)(u8g2.getAscent() - u8g2.getDescent()) / 2;
  int w = u8g2.getStrWidth(buf);
  int x = (128 - w) / 2;
  if (x < 0) x = 0;
  u8g2.drawStr(x, baselineY, buf);
}

/** 사각형 [x0,x1]×[y0,y1] 안에서 큰 글씨 가로·세로 중앙 (끝점 포함) */
static void drawPositionLargeCenteredInRect(int32_t v, int x0, int x1, int y0,
                                            int y1) {
  int32_t c = v;
  if (c > 999) c = 999;
  if (c < -999) c = -999;
  char buf[8];
  if (c < 0)
    snprintf(buf, sizeof(buf), "-%ld", (long)(-c));
  else
    snprintf(buf, sizeof(buf), "%ld", (long)c);

  u8g2.setFont(u8g2_font_logisoso22_tf);
  int w = u8g2.getStrWidth(buf);
  int midX = (x0 + x1) / 2;
  int x = midX - w / 2;
  if (x < x0) x = x0;
  if (x + w > x1 + 1) x = (x1 + 1) - w;
  if (x < x0) x = x0;

  int bandH = y1 - y0 + 1;
  if (bandH < 8) bandH = 8;
  int bandMid = y0 + bandH / 2;
  int baselineY =
      bandMid + (int)(u8g2.getAscent() - u8g2.getDescent()) / 2;
  u8g2.drawStr(x, baselineY, buf);
}

static void drawBig(int x, int y, int32_t v) {
  char b[16];
  snprintf(b, sizeof(b), "%ld", (long)v);
  u8g2.setFont(u8g2_font_logisoso22_tf);
  u8g2.drawStr(x, y, b);
}

static void drawSmall(int x, int y, int32_t v) {
  char b[16];
  snprintf(b, sizeof(b), "%ld", (long)v);
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(x, y, b);
}

/** 위쪽 꼭짓점이 (cx, yTip). 몸통은 아래로 */
static void drawArrowGraphicUp(int cx, int yTip) {
  const int tipH = 7;
  const int halfW = 5;
  const int stemLen = 11;
  int yBase = yTip + tipH;
  u8g2.drawTriangle(cx, yTip, cx - halfW, yBase, cx + halfW, yBase);
  u8g2.drawVLine(cx, yBase, stemLen);
}

/** 아래쪽 꼭짓점이 (cx, yBottom). 몸통은 위로 */
static void drawArrowGraphicDown(int cx, int yBottom) {
  const int tipH = 7;
  const int halfW = 5;
  const int stemLen = 11;
  int yBase = yBottom - tipH;
  u8g2.drawTriangle(cx, yBottom, cx - halfW, yBase, cx + halfW, yBase);
  u8g2.drawVLine(cx, yBase - stemLen, stemLen);
}

/**
 * RUN: 3×3 그리드 (128×64).
 * 좌상=정회전 목표, 좌하=역회전 목표, 좌중=화살표(정↑ 역↓),
 * 정중앙 셀=현재값 큰글씨, 우하=회전 애니메이션은 drawUi에서 drawFan.
 */
static void drawRunNineGridLayout(int32_t cur, int32_t tgt, int8_t fwd) {
  /* 열: 43+43+42=128 / 행: 22+22+20=64 */
  static const int XL0 = 0, XL1 = 42;
  static const int XM0 = 43, XM1 = 85;
  static const int XR0 = 86, XR1 = 127;
  static const int CX_L = (XL0 + XL1) / 2;

  static const int YT0 = 0, YT1 = 21;
  static const int YM0 = 22, YM1 = 43;
  static const int YB0 = 44, YB1 = 63;

  char tbuf[16];
  snprintf(tbuf, sizeof(tbuf), "%ld", (long)tgt);
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setDrawColor(1);

  if (fwd > 0) {
    int tw = u8g2.getStrWidth(tbuf);
    int yc = (YT0 + YT1) / 2;
    int bl = yc + 4;
    int x = CX_L - tw / 2;
    if (x < XL0) x = XL0;
    if (x + tw > XL1 + 1) x = XL1 + 1 - tw;
    u8g2.drawStr(x, bl, tbuf);
  }

  if (fwd < 0) {
    int tw = u8g2.getStrWidth(tbuf);
    int yc = (YB0 + YB1) / 2;
    int bl = yc + 4;
    int x = CX_L - tw / 2;
    if (x < XL0) x = XL0;
    if (x + tw > XL1 + 1) x = XL1 + 1 - tw;
    u8g2.drawStr(x, bl, tbuf);
  }

  {
    int yMidC = (YM0 + YM1) / 2;
    if (fwd > 0) {
      int yTip = yMidC - 6;
      if (yTip < YM0 + 1) yTip = YM0 + 1;
      drawArrowGraphicUp(CX_L, yTip);
    } else {
      int yBottom = yMidC + 6;
      if (yBottom > YM1 - 1) yBottom = YM1 - 1;
      drawArrowGraphicDown(CX_L, yBottom);
    }
  }

  drawPositionLargeCenteredInRect(cur, XM0, XM1, YM0, YM1);
}

static void drawUi() {
  u8g2.clearBuffer();
  u8g2.setFontMode(0);

  switch (phase) {
    case MUI_IDLE:
      drawPositionLargeCentered(currentAbs, 0, 64);
      break;
    case MUI_EDIT:
    case MUI_ARM:
      drawPositionLargeCentered(currentAbs, 0, 46);
      u8g2.setFont(u8g2_font_6x10_tf);
      {
        char t[20];
        snprintf(t, sizeof(t), "SET %ld", (long)targetAbs);
        int tw = u8g2.getStrWidth(t);
        u8g2.drawStr((128 - tw) / 2, 54, t);
      }
      break;
    case MUI_DIR_WAIT: {
      uint32_t elapsed = millis() - lastMotorStopMs;
      uint32_t left = 0;
      if (elapsed < DIR_CHANGE_COOLDOWN_MS)
        left = (DIR_CHANGE_COOLDOWN_MS - elapsed + 999) / 1000;
      u8g2.setFont(u8g2_font_6x10_tf);
      u8g2.drawStr(0, 10, "WAIT DIR 3s");
      drawBig(40, 42, (int32_t)left);
      break;
    }
    case MUI_RUN: {
      drawRunNineGridLayout(currentAbs, targetAbs, runDir);
      /* 우하 칸 (86–127, 44–63) 중심 근처 */
      drawFan(106, 54, runDir);
      break;
    }
    case MUI_TRIP:
      u8g2.setFont(u8g2_font_logisoso22_tf);
      u8g2.drawStr(12, 40, "TRIP");
      u8g2.setFont(u8g2_font_6x10_tf);
      u8g2.drawStr(0, 10, "OC  enc=clear");
      break;
    case MUI_SETUP:
      u8g2.setFont(u8g2_font_logisoso22_tf);
      u8g2.drawStr(20, 38, "SETUP");
      u8g2.setFont(u8g2_font_6x10_tf);
      u8g2.drawStr(0, 10, "I-zero calibrated");
      u8g2.drawStr(0, 63, "release PSH");
      break;
  }
  u8g2.sendBuffer();
  dbgSendBufferCount++;
}

// ----------------------------- 실행 시작 -----------------------------
static void attemptStartMotor() {
  int32_t delta = targetAbs - currentAbs;
  Serial.printf("[MOTOR] attempt cur=%ld tgt=%ld delta=%ld lastMotorDir=%d\n",
                (long)currentAbs, (long)targetAbs, (long)delta,
                (int)lastMotorDir);
  if (delta == 0) {
    DBG_PRINTLN(F("[MOTOR] delta==0 -> stay IDLE"));
    phase = MUI_IDLE;
    return;
  }
  int8_t nd = (delta > 0) ? 1 : -1;

  if (lastMotorDir != 0 && nd != lastMotorDir) {
    if (millis() - lastMotorStopMs < DIR_CHANGE_COOLDOWN_MS) {
      Serial.printf(
          "[MOTOR] dir change blocked, cooldown left ~%lums -> DIR_WAIT\n",
          (unsigned long)(DIR_CHANGE_COOLDOWN_MS -
                          (millis() - lastMotorStopMs)));
      phase = MUI_DIR_WAIT;
      return;
    }
  }

  runDir = nd;
  phase = MUI_RUN;
  uint32_t t0 = millis();
  runStartMs = t0;
  runLastPosTickMs = t0 - RUN_POSITION_TICK_MS;
  runStepAccum = 0.f;
  ocStartMs = 0;
  fanFrame = 0;

  Serial.printf(
      "[MOTOR] -> RUN dir=%s PWM_max=%u softStart=%lums tick=%lums\n",
      runDir > 0 ? "FWD" : "REV", (unsigned)MOTOR_PWM_DUTY,
      (unsigned long)SOFT_START_MS, (unsigned long)RUN_POSITION_TICK_MS);
  /* 첫 PWM·누적은 loop RUN 틱에서 적용 */
}

static void stopRunForEncoderEdit() {
  DBG_PRINTLN(F("[ENC] run interrupted -> EDIT"));
  int8_t d = runDir;
  markMotorStopped(d);
  phase = MUI_EDIT;
  lastEncStepMs = millis();
  armPhaseStartMs = 0;
}

/** RUN 중 푸시(PSH): 모터 정지 후 IDLE (자동 ARM 재출발 없음) */
static void stopRunForPushIdle() {
  DBG_PRINTLN(F("[PSH] run stopped -> IDLE"));
  markMotorStopped(runDir);
  phase = MUI_IDLE;
  runDir = 0;
  runStepAccum = 0.f;
}

void setup() {
  Serial.begin(DBG_SERIAL_BAUD);
#if defined(ARDUINO_ARCH_RP2040)
  delay(800);
#endif
  DBG_PRINTLN(F("\n\n======== PicoMotorController debug ========"));
  DBG_PRINTLN(F("[S0] Serial started"));
  analogReadResolution(12);
  DBG_PRINTLN(F("[S1] analogReadResolution(12)"));

  Serial.printf(
      "[S2] OLED ctrl=%d fullBuf=%d redraw=%dms I2C=%luHz\n", OLED_CTRL,
      OLED_USE_FULL_BUFFER, OLED_REDRAW_MS, (unsigned long)OLED_I2C_CLOCK_HZ);

#if (OLED_CTRL == 0 || OLED_CTRL == 1)
#if DBG_I2C_SCAN
  dbgI2cScanWire1();
#else
  DBG_PRINTLN(F("[S3] Wire1 begin (OLED HW)"));
  wire1OledPinsBegin();
#endif
#elif DBG_I2C_SCAN
  DBG_PRINTLN(F("[S3] WARN: I2C scan + SW OLED 같은 핀 — DBG_I2C_SCAN=0 권장"));
  dbgI2cScanWire1();
#else
  DBG_PRINTLN(F("[S3] SW OLED — Wire1 OLED 버스 미사용"));
#endif

#if OLED_CTRL == 0
  DBG_PRINTLN(F("[S5] U8g2 SH1106 128x64 2ND_HW (Wire1, F=full/1=page)"));
#elif OLED_CTRL == 1
  DBG_PRINTLN(F("[S5] U8g2 SSD1306 128x64 2ND_HW (Wire1, F=full/1=page)"));
#elif OLED_CTRL == 2
  DBG_PRINTLN(F("[S5] U8g2 SH1106 SW_I2C"));
#else
  DBG_PRINTLN(F("[S5] U8g2 SSD1306 SW_I2C"));
#endif
  Serial.printf("       SCL=GP%d SDA=GP%d u8g2_8bit_addr=0x%02X\n",
                PIN_OLED_SCL, PIN_OLED_SDA, OLED_I2C_ADDR_8BIT);

  u8g2.setI2CAddress(OLED_I2C_ADDR_8BIT);
  DBG_PRINTLN(F("[S6] u8g2.setI2CAddress OK, calling begin()..."));
  u8g2.begin();
  DBG_PRINTLN(F("[S6] u8g2.begin() done (void)"));
  u8g2.setContrast(255);
  DBG_PRINTLN(F("[S7] setContrast(255)"));

  DBG_PRINTLN(F("[S8] OLED test: full ON 500ms (화면이 잠깐 하얗/반전이면 통신됨)"));
  u8g2.clearBuffer();
  u8g2.setDrawColor(1);
  u8g2.drawBox(0, 0, 128, 64);
  u8g2.sendBuffer();
  dbgSendBufferCount++;
  delay(500);
  u8g2.clearBuffer();
  u8g2.sendBuffer();
  dbgSendBufferCount++;
  DBG_PRINTLN(F("[S9] OLED test cleared"));

  DBG_PRINTLN(F("[S10] motorPinsInit"));
  motorPinsInit();
  DBG_PRINTLN(F("[S11] encoderInit"));
  encoderInit();
  DBG_PRINTLN(F("[S12] calibrateZero..."));
  calibrateZero();
  Serial.printf("[S12] vZero=%.4f V (전류 0 기준)\n", vZero);

  targetAbs = currentAbs;
  phase = MUI_IDLE;
  lastEncStepMs = millis();
  DBG_PRINTLN(F("[S13] drawUi() first frame"));
  drawUi();
  Serial.printf("[S14] setup done, sendBuffer calls=%lu\n",
                (unsigned long)dbgSendBufferCount);
  DBG_PRINTLN(F("Tip: 윗줄만/깨짐 → OLED_USE_FULL_BUFFER=1(기본), I2C 50k, redraw 늘리기"));
  DBG_PRINTLN(F("     칩: SH1106=ctrl0 SSD1306=ctrl1 | 0x3D | SDA/SCL 바뀌면 SW에서 SWAP=1\n"));
}

void loop() {
  static uint8_t dbgPrevPhase = MUI_IDLE;
  static uint32_t dbgLastHb = 0;
  static bool dbgLoopOnce = false;
  const uint32_t now = millis();

  if (!dbgLoopOnce) {
    DBG_PRINTLN(F("[L0] loop() running"));
    dbgLoopOnce = true;
  }

  const bool encStep =
      (phase == MUI_SETUP) ? false : encoderPollStep();

  {
    uint8_t raw = digitalRead(PIN_ENC_SW) ? 1u : 0u;
    if (raw != encSwPrevRaw) {
      encSwPrevRaw = raw;
      encSwLastChangeMs = now;
    } else if ((now - encSwLastChangeMs) >= ENC_SW_DEBOUNCE_MS &&
               raw != encSwStable) {
      encSwStable = raw;
      if (encSwStable == 0u) {
        encSwHeldSinceMs = now;
        if (phase == MUI_RUN) stopRunForPushIdle();
      } else {
        encSwHeldSinceMs = 0;
        if (phase == MUI_SETUP) {
          phase = MUI_IDLE;
          encLastAB = (digitalRead(PIN_ENC_A) ? 1u : 0u) |
                      (digitalRead(PIN_ENC_B) ? 2u : 0u);
          encAccum = 0;
        }
      }
    }
  }

  if (encSwStable == 0u && phase != MUI_SETUP && encSwHeldSinceMs != 0u) {
    if (now - encSwHeldSinceMs >= SETUP_HOLD_MS) {
      motorAllStop();
      runDir = 0;
      runStepAccum = 0.f;
      phase = MUI_SETUP;
      calibrateZero();
      Serial.printf(
          "[SETUP] I-zero cal vZero=%.4f V (release PSH -> IDLE)\n", vZero);
      encSwHeldSinceMs = 0;
    }
  }

  if (encStep) {
    lastEncStepMs = now;
    if (phase == MUI_RUN) {
      stopRunForEncoderEdit();
    } else if (phase == MUI_TRIP) {
      phase = MUI_EDIT;
      armPhaseStartMs = 0;
    } else {
      phase = MUI_EDIT;
      armPhaseStartMs = 0;
    }
  }

  // EDIT → ARM (회전 멈춤 감지)
  if (phase == MUI_EDIT && (now - lastEncStepMs >= ENCODER_EDIT_END_MS)) {
    phase = MUI_ARM;
    armPhaseStartMs = now;
  }

  // ARM: 2초 후 출발
  if (phase == MUI_ARM) {
    if (encStep) {
      /* 위에서 MUI_EDIT 으로 감 */
    } else if (now - armPhaseStartMs >= ENCODER_ARM_START_MS) {
      attemptStartMotor();
    }
  }

  // 방향 전환 대기
  if (phase == MUI_DIR_WAIT) {
    if (now - lastMotorStopMs >= DIR_CHANGE_COOLDOWN_MS) attemptStartMotor();
  }

  // 구동 중
  if (phase == MUI_RUN) {
    float ia = fabsf(readAmps());
    if (ia > OVERCURRENT_A) {
      if (ocStartMs == 0) ocStartMs = now;
      else if (now - ocStartMs >= OC_TRIP_MS) {
        Serial.printf("[OC] TRIP I=%.2f A (limit %.2f A, %lu ms)\n", ia,
                      OVERCURRENT_A, (unsigned long)OC_TRIP_MS);
        markMotorStopped(runDir);
        phase = MUI_TRIP;
        ocStartMs = 0;
      }
    } else {
      ocStartMs = 0;
    }

    if (phase == MUI_RUN &&
        now - runLastPosTickMs >= RUN_POSITION_TICK_MS) {
      uint32_t dt = now - runLastPosTickMs;
      if (dt > 250u) dt = 250u;
      runLastPosTickMs = now;

      uint32_t t0 = now - dt;
      uint32_t elStart =
          (t0 > runStartMs) ? (t0 - runStartMs) : 0u;
      uint32_t elEnd = now - runStartMs;
      float s0 = softStartSpeed01(elStart);
      float s1 = softStartSpeed01(elEnd);
      float speedAvg = 0.5f * (s0 + s1);
      runStepAccum += speedAvg * (dt / 1000.0f);

      uint8_t duty = (uint8_t)(MOTOR_PWM_DUTY * s1 + 0.5f);
      if (duty > MOTOR_PWM_DUTY) duty = MOTOR_PWM_DUTY;
      if (runDir > 0)
        motorForward(duty);
      else
        motorReverse(duty);

      while (runStepAccum >= 1.0f && currentAbs != targetAbs) {
        if (currentAbs < targetAbs)
          currentAbs++;
        else if (currentAbs > targetAbs)
          currentAbs--;
        else
          break;
        runStepAccum -= 1.0f;
      }

      if (currentAbs == targetAbs) {
        DBG_PRINTLN(F("[MOTOR] target reached -> IDLE"));
        markMotorStopped(runDir);
        phase = MUI_IDLE;
        runDir = 0;
        runStepAccum = 0.f;
      }
    }

    if (now >= fanNextMs) {
      fanNextMs = now + 140;
      fanFrame = (fanFrame + 1) % 12;
    }
  }

  if (phase != dbgPrevPhase) {
    Serial.printf("[PH] %s -> %s | cur=%ld tgt=%ld\n",
                  motorUiStateStr(dbgPrevPhase), motorUiStateStr(phase),
                  (long)currentAbs, (long)targetAbs);
    dbgPrevPhase = phase;
  }

  if (now - dbgLastHb >= 2000) {
    dbgLastHb = now;
    float iA = readAmps();
    Serial.printf(
        "[HB] phase=%s cur=%ld tgt=%ld I=%.3f A encStep=%s draws=%lu ms=%lu\n",
        motorUiStateStr(phase), (long)currentAbs, (long)targetAbs, iA,
        encStep ? "Y" : "N", (unsigned long)dbgSendBufferCount,
        (unsigned long)now);
  }

  static uint32_t tDraw = 0;
  if (now - tDraw >= (uint32_t)OLED_REDRAW_MS) {
    tDraw = now;
    drawUi();
  }
}
