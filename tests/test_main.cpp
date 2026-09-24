#include <cstring>
#include "TestFramework.h"

int main(int argc, char** argv)
{
    const char* filter = argc > 1 ? argv[1] : nullptr;
    int run = 0;
    int failed_cases = 0;

    for (const test::Case& c : test::registry())
    {
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

    std::cout << '\n' << run - failed_cases << '/' << run << " tests passed\n";
    return failed_cases == 0 ? 0 : 1;
}
