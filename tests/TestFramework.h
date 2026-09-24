/**
 * @file TestFramework.h
 * @brief Minimal self-registering unit test harness (no dependencies).
 *
 *   TEST(name) { CHECK(cond); CHECK_EQ(a, b); CHECK_THROWS(expr); }
 *
 * Run the test binary with a substring argument to run only matching tests.
 */

#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include "Types.h"

namespace test
{
    struct Case
    {
        const char* name;
        std::function<void()> fn;
    };

    inline std::vector<Case>& registry()
    {
        static std::vector<Case> cases;
        return cases;
    }

    inline int& failures()
    {
        static int n = 0;
        return n;
    }

    struct Registrar
    {
        Registrar(const char* name, std::function<void()> fn)
        {
            registry().push_back(Case{name, std::move(fn)});
        }
    };

    template <class T>
    std::string show(const T& v)
    {
        std::ostringstream os;
        os << v;
        return os.str();
    }

    inline std::string show(LogicValue v) { return std::string(1, toChar(v)); }

    template <class T>
    std::string show(const std::vector<T>& v)
    {
        std::string s = "{";
        for (std::size_t i = 0; i < v.size(); ++i)
            s += (i ? ", " : "") + show(v[i]);
        return s + "}";
    }

    inline void fail(const char* file, int line, const std::string& what)
    {
        ++failures();
        std::cerr << "    FAIL " << file << ':' << line << ": " << what << '\n';
    }
} // namespace test

#define TEST_CONCAT2(a, b) a##b
#define TEST_CONCAT(a, b) TEST_CONCAT2(a, b)

#define TEST(name)                                                        \
    static void name();                                                   \
    static test::Registrar TEST_CONCAT(registrar_, name)(#name, name);    \
    static void name()

#define CHECK(cond)                                                       \
    do {                                                                  \
        if (!(cond)) test::fail(__FILE__, __LINE__, "CHECK(" #cond ")");  \
    } while (0)

#define CHECK_EQ(actual, expected)                                        \
    do {                                                                  \
        const auto& a_ = (actual);                                        \
        const auto& e_ = (expected);                                      \
        if (!(a_ == e_))                                                  \
            test::fail(__FILE__, __LINE__,                                \
                       #actual " == " #expected "  (got " +               \
                       test::show(a_) + ", want " + test::show(e_) + ")");\
    } while (0)

#define CHECK_THROWS(expr)                                                \
    do {                                                                  \
        bool threw_ = false;                                              \
        try { (void)(expr); } catch (...) { threw_ = true; }              \
        if (!threw_)                                                      \
            test::fail(__FILE__, __LINE__, "expected throw: " #expr);     \
    } while (0)

#endif
