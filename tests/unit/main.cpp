// SPDX-License-Identifier: GPL-3.0-or-later
#include "tests/unit/test_framework.h"

#include <cstdio>

namespace ldaw::test {

std::vector<Case>& registry() {
    static std::vector<Case> cases;
    return cases;
}

static int g_failures = 0;

void fail(const char* file, int line, const char* expression) {
    ++g_failures;
    std::printf("  FAILED %s(%d): %s\n", file, line, expression);
}

} // namespace ldaw::test

int main() {
    int failedCases = 0;
    for (const auto& testCase : ldaw::test::registry()) {
        const int before = ldaw::test::g_failures;
        testCase.fn();
        const bool passed = ldaw::test::g_failures == before;
        std::printf("[%s] %s\n", passed ? "PASS" : "FAIL", testCase.name);
        if (!passed) ++failedCases;
    }
    std::printf("%zu cases, %d failed\n", ldaw::test::registry().size(), failedCases);
    return failedCases == 0 ? 0 : 1;
}
