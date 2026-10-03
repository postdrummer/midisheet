#include "Check.h"

#include <cstdlib>

int main()
{
    runFormulaTests();
    runEngineTests();
    runSheetTests();
    if (testFailures() == 0)
        std::puts("all tests passed");
    else
        std::fprintf(stderr, "%d check(s) failed\n", testFailures());
    return testFailures() == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
