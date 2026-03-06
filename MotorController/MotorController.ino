#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include "RotaryEncoder.h"
#include "MotorDriver.h"
#include "CurrentSensor.h"
#include "Button.h"
#include "ButtonInterrupt.h"
#include "AppState.h"
#include "AppStateIdle.h"
#include "AppStateSetup.h"
#include "AppStateRun.h"
#include "AppStateFault.h"
#include "MotorPositionConfig.h"

// SH1116 OLED 핀 정의 (XIAO ESP32C6)
#define OLED_SDA 22      // D4 (I2C SDA)
#define OLED_SCL 23      // D5 (I2C SCL)
#define OLED_RESET -1   // 리셋 핀 없음 (공유 리셋 사용)
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C  // I2C 주소 (일부 모듈은 0x3D)

// 로터리 엔코더 핀 정의
#define ENCODER_CLK 16   // D6 (CLK 핀)
#define ENCODER_DT 17    // D7 (DT 핀)

// 버튼 핀 정의 (PUSH, CW, CCW)
#define BUTTON_PUSH_PIN 19   // D8
#define BUTTON_CW_PIN   20   // D9
#define BUTTON_CCW_PIN  18   // D10

// 전류 센서 핀 정의
#define CURRENT_SENSOR_PIN A0  // GPIO 0 (ADC 입력)

// 모터 제어핀 정의
#define MOTOR_ENABLE 1  // D1 
#define MOTOR_PWM_CW 2  // D2
#define MOTOR_PWM_CCW 21  // D3



// SH1116 OLED 객체 생성 (I2C 통신)
Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// 로터리 엔코더 객체
RotaryEncoder encoder;

// 싱글톤 참조 (setup에서 init 호출 전에 사용 가능)
MotorDriver& motor = MotorDriver::getInstance();
CurrentSensor& currentSensor = CurrentSensor::getInstance();

// 버튼 인터럽트 초기화 (PUSH, UP, DOWN 통합)
// 기존 Button 객체는 사용하지 않음 (인터럽트 기반으로 변경)

// 상태 패턴 컨텍스트
AppStateContext appStateContext;

// 엔코더 디스플레이용 변수
int lastEncoderValue = 0;

// 과전류 시 모터 정지 후 호출되는 알림 (Fault 전환)
void onOverCurrentFaultNotify(float current) {
  appStateContext.setState(&AppStateFault::getInstance());
  Serial.print("전류 초과! Current: ");
  Serial.print(current, 3);
  Serial.println("A");
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("시스템 초기화 중...");

  // 모터 위치 설정 로드 (플래시)
  if (MotorPositionConfigManager::load()) {
    Serial.println("위치 설정 로드 완료!");
  } else {
    Serial.println("위치 설정 기본값 사용");
  }
  // 설정 변경 알림: 모터 드라이버에 한계값 전달
  motor.setPositionLimits(MotorPositionConfigManager::getInstance().min, MotorPositionConfigManager::getInstance().top);

  // 버튼 인터럽트 초기화 (하나의 ISR로 처리)
  ButtonInterrupt::init(BUTTON_PUSH_PIN, BUTTON_CW_PIN, BUTTON_CCW_PIN);
  Serial.println("버튼 인터럽트 초기화 완료!");

  // 로터리 엔코더 초기화
  encoder.init(ENCODER_CLK, ENCODER_DT);
  Serial.println("로터리 엔코더 초기화 완료!");
  
  // 모터 제어 초기화 (별도 스레드로 동작)
  motor.init(MOTOR_ENABLE, MOTOR_PWM_CW, MOTOR_PWM_CCW);
  Serial.println("모터 제어 초기화 완료!");
  
  // 전류 센서 초기화 (ACS758-50A) - 캘리브 후 별도 스레드 시작
  currentSensor.init(CURRENT_SENSOR_PIN);
  currentSensor.calibrate();
  
  // 설정된 최대 전류 설정. 과전류 콜백은 모터 드라이버가 받아 즉시 정지
  float maxCur = MotorPositionConfigManager::getInstance().maxCurrent;
  currentSensor.setMaxCurrent(maxCur);
  currentSensor.setOverCurrentCallback(MotorDriver::onOverCurrent);
  motor.setOverCurrentFaultNotify(onOverCurrentFaultNotify);
  Serial.print("최대 전류 설정: ");
  Serial.print(maxCur, 1);
  Serial.println("A");
  
  currentSensor.startTask();
  Serial.println("전류 센서 초기화 완료!");
  
  // I2C 초기화
  Wire.begin(OLED_SDA, OLED_SCL);
  
  // OLED 초기화
  if (!display.begin(OLED_ADDR, true)) {
    Serial.println("SH1116 OLED 초기화 실패!");
    Serial.println("다음 주소를 시도해보세요: 0x3D");
    // 0x3D 주소로 재시도
    if (!display.begin(0x3D, true)) {
      Serial.println("OLED를 찾을 수 없습니다. 연결을 확인하세요.");
      while (1); // 무한 루프 (OLED 없이는 진행 불가)
    }
  }
  
  Serial.println("SH1116 OLED 초기화 완료!");
  
  // 디스플레이 설정
  display.display(); // Adafruit 로고 표시
  delay(2000);
  display.clearDisplay();
  
  // 텍스트 설정
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 0);
  
  // 초기 메시지 표시
  display.println("SH1116 OLED");
  display.println("로터리 엔코더");
  display.println("테스트 중...");
  display.display();
  
  // Idle 상태로 시작, 초기 목표 = 현재 위치
  MotorPositionConfigManager::getInstance().target = motor.getPositionSec();
  appStateContext.setState(&AppStateIdle::getInstance());
  Serial.println("초기화 완료! State: Idle");
}

void loop() {
  appStateContext.update();  // 큐에서 키를 하나씩 팝 → 현재 상태 onKey(key), then onUpdate()
  delay(50);
}
