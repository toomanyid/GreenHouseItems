#!/bin/sh
# PC에서 2층과 3층 로직을 시험한다. 필요한 것: g++ (C++11 이상)
set -e
cd "$(dirname "$0")/.."
OUT=$(mktemp -d)
CXX=${CXX:-g++}
FLAGS="-std=c++11 -Wall -Wextra -Werror -Wno-unknown-warning-option -Wno-format-truncation -O1 -I."
$CXX $FLAGS test/test_window/test_window.cpp ctl_Window.cpp ctl_Safety.cpp Settings.cpp -o "$OUT/test_window"
"$OUT/test_window"
# 책상 시험 빌드(-DWINCTRL_BENCH)의 릴레이 판정
$CXX $FLAGS -DWINCTRL_BENCH test/test_window/test_window.cpp ctl_Window.cpp ctl_Safety.cpp Settings.cpp -o "$OUT/test_window_bench"
"$OUT/test_window_bench"
if [ -f test/test_logic/test_logic.cpp ]; then
  $CXX $FLAGS test/test_logic/test_logic.cpp app_TempControl.cpp ErrorLog.cpp io_Format.cpp Settings.cpp app_Dispatcher.cpp ctl_Window.cpp ctl_Safety.cpp -o "$OUT/test_logic"
  "$OUT/test_logic"
fi
rm -rf "$OUT"
