#!/bin/sh
# 아두이노 쪽 파일까지 PC에서 문법만 확인한다 (test/stubs의 가짜 헤더 사용).
# ESP32용 컴파일을 대신하지 않는다. 실제 확인은 아두이노 IDE에서 한다.
set -e
cd "$(dirname "$0")/.."
CXX=${CXX:-g++}
# 두 번째는 UI 시험 빌드 (-DWINCTRL_UISIM)
for def in "" "-DWINCTRL_UISIM"; do
  for f in *.cpp; do
    $CXX -std=gnu++17 -Wall -Wextra -Werror -Wno-unknown-warning-option -Wno-format-truncation -fsyntax-only -Itest/stubs -I. $def "$f"
  done
  $CXX -std=gnu++17 -Wall -Wextra -Werror -Wno-unknown-warning-option -Wno-format-truncation -fsyntax-only -Itest/stubs -I. $def -x c++ WinCtrl.ino
done
echo "문법 확인 통과"
