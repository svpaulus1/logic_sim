#include <cstring>
#include <string>
#include "TestFramework.h"

int main(int argc, char** argv)
{
    // logicsim_tests [--file STEM] [NAME_SUBSTRING]
    const char* file = nullptr;
    const char* filter = nullptr;
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--file") == 0 && i + 1 < argc)
            file = argv[++i];
        else
            filter = argv[i];
    }

    int run = 0;
    int failed_cases = 0;

    for (const test::Case& c : test::registry())
    {
        if (file && c.file != file) continue;
        if (filter && !std::strstr(c.name, filter)) continue;

        const int before = test::failures();
        try
        {
            c.fn();
        }
        catch (const std::exception& e)
        {
            test::fail(c.name, 0, std::string("unexpected exception: ") + e.what());
        }
        ++run;

        const bool ok = test::failures() == before;
        if (!ok) ++failed_cases;
        std::cout << (ok ? "  ok   " : "  FAIL ") << c.name << '\n';
    }

    // Selecting nothing is an error, so a renamed or mistyped file shows up
    // as a failing CTest entry instead of a silent pass.
    if (run == 0)
    {
        std::cout << "no tests matched\n";
        return 1;
    }
    
    std::cout << '\n' << run - failed_cases << '/' << run << " tests passed\n";
    return failed_cases == 0 ? 0 : 1;
}
