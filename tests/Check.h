#pragma once

#include <cstdio>

// Minimal test harness: no dependencies, runs in CI with plain ctest.
inline int& testFailures()
{
    static int failures = 0;
    return failures;
}

#define CHECK(cond)                                                                        \
    do {                                                                                   \
        if (!(cond)) {                                                                     \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond);  \
            ++testFailures();                                                              \
        }                                                                                  \
    } while (0)

void runEngineTests();
void runFormulaTests();
void runSheetTests();
