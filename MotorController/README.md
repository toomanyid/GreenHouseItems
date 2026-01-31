# SH1116 OLED 사용 가이드

## 라이브러리 설치

Arduino IDE에서 다음 라이브러리를 설치해야 합니다:

1. **Adafruit GFX Library**
   - 스케치 → 라이브러리 포함하기 → 라이브러리 관리
   - "Adafruit GFX" 검색 후 설치

2. **Adafruit SH110X Library**
   - 스케치 → 라이브러리 포함하기 → 라이브러리 관리
   - "Adafruit SH110X" 검색 후 설치

## 하드웨어 연결

### XIAO ESP32C6와 SH1116 OLED 연결

| SH1116 OLED | XIAO ESP32C6 | 설명 |
|------------|--------------|------|
| VCC | 3.3V | 전원 |
| GND | GND | 그라운드 |
| SDA | D4 (GPIO 4) | I2C 데이터 |
| SCL | D5 (GPIO 5) | I2C 클럭 |

**참고**: 일부 SH1116 모듈은 I2C 주소가 0x3D일 수 있습니다. 코드에서 자동으로 두 주소를 시도합니다.

## 핀 정의 수정

다른 핀을 사용하려면 `MotorController.ino` 파일의 다음 부분을 수정하세요:

```cpp
#define OLED_SDA 4      // 원하는 GPIO 핀 번호로 변경
#define OLED_SCL 5      // 원하는 GPIO 핀 번호로 변경
```

## I2C 주소 확인

OLED가 인식되지 않으면 I2C 주소를 확인하세요:

```cpp
#define OLED_ADDR 0x3C  // 또는 0x3D
```

코드는 자동으로 두 주소를 시도하지만, 수동으로 변경할 수도 있습니다.

## 사용 예제

기본적인 텍스트 표시:

```cpp
display.clearDisplay();
display.setTextSize(1);
display.setTextColor(SH110X_WHITE);
display.setCursor(0, 0);
display.println("Hello World!");
display.display();
```

## 참고사항

- SH1116은 128x64 픽셀 해상도를 지원합니다
- I2C 통신 속도는 기본적으로 400kHz입니다
- 디스플레이 업데이트 후 `display.display()`를 호출해야 화면에 반영됩니다
