# 측창 모터 제어기 펌웨어 0.9 (WinCtrl)

ESP32-S3로 측창 모터 2대를 초 단위로 움직이는 제어기의 펌웨어입니다.
무엇을 해야 하는지는 Google 드라이브 "측창 모터 제어기" 폴더의 문서를 따릅니다.

- 소프트웨어 스펙 v0.9: 동작, 화면, 메뉴, 안전 규칙
- 핀 배정 (ESP32-S3): `pins.h`
- 설계 정리 (2채널 50W/300W): 하드웨어

50W와 300W 모델은 펌웨어 한 벌을 씁니다. 전원을 켤 때 GPIO39 점퍼를 읽어 모델을 정합니다.
점퍼가 없으면(열림) 300W, GND로 이어져 있으면 50W입니다. 정보 메뉴 셋째 장에서 확인할 수 있습니다.

## 아두이노 IDE에서 빌드

1. 아두이노 IDE 2.x에 ESP32 보드 패키지(Espressif Systems의 esp32)를 설치합니다. 2.x와 3.x 모두 쓸 수 있게 만들었습니다.
2. `WinCtrl/WinCtrl.ino`를 엽니다. 같은 폴더의 다른 파일은 자동으로 함께 빌드됩니다.
3. 도구 메뉴를 아래처럼 맞춥니다.

| 항목 | 값 |
|---|---|
| 보드 | ESP32S3 Dev Module |
| USB CDC On Boot | Disabled |
| USB Mode | Hardware CDC and JTAG (기본값 그대로) |
| Flash Size | 16MB (128Mb) |
| PSRAM | OPI PSRAM |
| Partition Scheme | 16M Flash (3MB APP/9.9MB FATFS) 또는 기본값 |
| Upload Speed | 921600 |

핀 배정 문서대로 직결 USB 포트(USB 라벨)에는 케이블을 꽂지 않고, 업로드는 보드의 UART 포트(COM 라벨)로 합니다.

맥에서도 같습니다. UART 포트의 USB 칩(CH340 등)이 보이지 않으면 해당 드라이버를 설치합니다.

## PlatformIO

`platformio.ini`가 같이 있어 VS Code + PlatformIO로 이 폴더를 열어도 됩니다.
만든 곳에서 PlatformIO 빌드를 해 보지 못했으므로 처음 빌드에서 고칠 것이 나올 수 있습니다.

## 파일 구성

스펙 2절의 네 층 구조를 따릅니다. 의존은 위층에서 아래층으로만 흐릅니다.

| 층 | 파일 | 하는 일 |
|---|---|---|
| 4. 입력 | `io_UiTask`, `io_Sound`, `io_Format` | LCD 화면, 엔코더 조작과 설정 메뉴, 소리, 설정과 에러 저장 |
| 3. 조정 | `app_ControlTask`, `app_Dispatcher`, `app_TempControl` | 10ms 제어 주기, 얼럿과 잡 처리, 온도제어 |
| 2. 제어 | `ctl_Window`, `ctl_Safety` | 창의 상태 흐름, 위치 계산, 전류와 MC 판정 |
| 1. 장치 | `dev_*` | 센서, 모터 드라이버, MC, LCD, 엔코더, 피에조, 온도 센서 |
| 공통 | `pins.h`, `config.h`, `Types.h`, `Settings`, `Store`, `ErrorLog` | 핀, 상수, 구조체, 설정 범위, 플래시 저장 |

`dev_Compat.h`는 ESP32 아두이노 코어 2.x와 3.x의 PWM 함수 차이를 감쌉니다.

## PC에서 시험

2층과 3층, 화면 글자 만들기는 하드웨어 없이 PC에서 시험합니다. g++만 있으면 됩니다 (맥은 Xcode 명령줄 도구).

```
sh test/run_tests.sh       # 창 상태 흐름, 온도제어, 에러 저장, 화면 글자 시험
sh test/check_compile.sh   # 아두이노 쪽 파일까지 가짜 헤더로 문법만 확인
```

`check_compile.sh`는 ESP32용 컴파일을 대신하지 않습니다. 실제 확인은 아두이노 IDE에서 합니다.

## 실물로 맞춰야 하는 값

`config.h`에서 `(임시)`가 붙은 값은 실물로 확인한 뒤 고칩니다.

- 전류 환산 `BTS_KILIS`(8500), 드라이버 고장 전압 `DRIVER_FAULT_MV`
- MC 전압 경계 `RELAY_ON_MV`, `RELAY_OFF_MV`, 뗀 뒤 확인 시간 `RELAY_DECAY_MS`, 떼기 전 대기 `RELAY_RELEASE_DELAY_MS`
- 비상정지 전압 경계 `ESTOP_OK_MV`
- 원점 센서 유지 시간 `SENSOR_DEBOUNCE_MS`, 위치 확인 여유 `POS_CHECK_MARGIN`, CLOSE 여유 `CLOSE_MARGIN`, 리밋 여유 `LIMIT_BACKUP_MARGIN`
- 해제 후 재고장 시간 `REFAULT_WINDOW_MS`
- 엔코더 빠르게 돌리기 경계 `ENC_FAST_MS`, `ENC_FASTER_MS`
- 온도 센서 경계 `TEMP_PRESENT_UA`, `TEMP_OVER_UA`
- 워치독 시간 `WATCHDOG_TIMEOUT_S`, 제품명 `PRODUCT_NAME`
- 고장과 비상정지 소리 반복 간격 (`io_Sound.cpp`)
- 피에조와 상태 LED 핀(38, 48)은 보드마다 다르므로 받은 보드로 확인합니다 (`pins.h`)

## 스펙에 없어서 정한 것

- 창 설정, 온도제어, 소리 목록의 마지막에 BACK 칸을 두어 첫 화면으로 돌아갑니다.
- 측정 결과의 저장 확인은 일곱째 장에서 누른 뒤, 돌려서 YES/NO를 고르고 다시 누릅니다.
- 설정 메뉴에서는 한 칸짜리 회전도 받습니다. "0.5초 안에 두 칸" 규칙은 평소 화면에만 둡니다.
- 얼럿 큐가 가득 차면 전체 정지로 처리합니다.
- 원점 센서 유무가 아직 정해지지 않은 창은 메뉴의 HOME SENS 옆에 WAIT로 보이고, 점검 화면에는 지금 입력(ON/OFF)을 그대로 보입니다. 설치할 때 센서를 맞추는 데 쓰라는 뜻입니다.
- 평소 화면의 위치 입력은 지금 위치(움직이는 중이면 목표)를 0.5 단위로 맞춘 값에서 시작합니다. 위치를 모르는 창은 0.0(CLOSE)에서 시작합니다.
- 확인하지 않은 고장이나 에러로 작은 글씨 화면이 떠 있는 동안 엔코더를 돌리면 에러음만 냅니다. 먼저 짧게 눌러 확인합니다.
- 원점을 찾는 동안은 작은 글씨 화면을 보이고, 그때 돌려서 입력을 시작하면 입력하는 동안은 입력 화면을 보입니다.
- 개별 항목에서 넣은 이동은 도착할 때까지 개별 화면에 둡니다. 움직이지 않았으면 5초 규칙으로 위치 동기로 돌아갑니다.
- 마지막으로 고른 항목은 온도와 위치 동기 둘 중 하나로 저장합니다(개별 항목은 위치 동기로 저장). 플래시 키는 `uiitem`입니다.
- 설정 메뉴의 상한 리밋은 99.9를 OFF로 보입니다.
- 설정 저장 모양 버전 1(리밋이 초)은 읽을 때 버전 2로 옮깁니다. 초 리밋은 그때의 최대 위치로 백분율로 바꾸고, LINK는 켬으로 둡니다.
- 시험용 OLED 화면에서도 사용자 글자(큰 글씨, 가로 두 배 숫자, ≠)를 키워 그립니다.
