// 시리얼 디버그 단계 명령을 PC에서 시험한다 (test/stubs의 가짜 Serial)
#include "../minitest.h"
#include "../../io_Debug.h"

static void test_levels() {
    CHECK(Dbg::level() == 1);                 // 켤 때는 늘 1
    CHECK(Dbg::on(Dbg::BASIC) && !Dbg::on(Dbg::ACTION));
    CHECK(Dbg::command("d3"));
    CHECK(Dbg::level() == 3 && Dbg::on(Dbg::DETAIL) && Dbg::on(Dbg::ACTION));
    CHECK(Dbg::command(" D2 "));             // 빈칸, 대문자도
    CHECK(Dbg::level() == 2 && !Dbg::on(Dbg::DETAIL));
    CHECK(Dbg::command("d?") && Dbg::command("d"));
    CHECK(Dbg::level() == 2);                 // 도움말은 단계를 바꾸지 않음
    CHECK(!Dbg::command("d4") && !Dbg::command("d0") && !Dbg::command("x") && !Dbg::command(""));
    CHECK(Dbg::level() == 2);
    CHECK(Dbg::command("d1") && Dbg::level() == 1);
}

int main() {
    RUN(test_levels);
    return finish();
}
