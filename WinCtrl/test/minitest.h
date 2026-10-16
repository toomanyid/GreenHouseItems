#pragma once
// PC에서 돌리는 아주 작은 시험 도구. 외부 라이브러리 없이 g++만으로 빌드한다.
#include <cstdio>
#include <cstdlib>

static int g_checks = 0;
static int g_fails = 0;

#define CHECK(cond) do { g_checks++; if (!(cond)) { g_fails++; \
    std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

#define CHECK_NEAR(a, b, tol) do { g_checks++; long _a = (long)(a), _b = (long)(b); \
    if (_a - _b > (tol) || _b - _a > (tol)) { g_fails++; \
    std::printf("  FAIL %s:%d: %s = %ld, expected %ld +- %ld\n", __FILE__, __LINE__, #a, _a, _b, (long)(tol)); } } while (0)

#define RUN(fn) do { std::printf("%s\n", #fn); fn(); } while (0)

static int finish() {
    std::printf("\n%d checks, %d failed\n", g_checks, g_fails);
    return g_fails ? 1 : 0;
}
