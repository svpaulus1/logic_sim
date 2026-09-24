#include "TestFramework.h"
#include "Types.h"

namespace
{
    constexpr LogicValue L = LogicValue::LOW;
    constexpr LogicValue H = LogicValue::HIGH;
    constexpr LogicValue X = LogicValue::UNKNOWN;
    constexpr LogicValue Z = LogicValue::HIGHZ;
    const LogicValue kAll[] = {L, H, X, Z};
} // namespace

TEST(types_and_truth_table)
{
    // Row = a, column = b, order L H X Z.
    const LogicValue want[4][4] = {
        {L, L, L, L},
        {L, H, X, X},
        {L, X, X, X},
        {L, X, X, X},
    };
    for (int a = 0; a < 4; ++a)
        for (int b = 0; b < 4; ++b)
            CHECK_EQ(kAll[a] & kAll[b], want[a][b]);
}

TEST(types_or_truth_table)
{
    const LogicValue want[4][4] = {
        {L, H, X, X},
        {H, H, H, H},
        {X, H, X, X},
        {X, H, X, X},
    };
    for (int a = 0; a < 4; ++a)
        for (int b = 0; b < 4; ++b)
            CHECK_EQ(kAll[a] | kAll[b], want[a][b]);
}

TEST(types_xor_truth_table)
{
    const LogicValue want[4][4] = {
        {L, H, X, X},
        {H, L, X, X},
        {X, X, X, X},
        {X, X, X, X},
    };
    for (int a = 0; a < 4; ++a)
        for (int b = 0; b < 4; ++b)
            CHECK_EQ(kAll[a] ^ kAll[b], want[a][b]);
}

TEST(types_not_and_helpers)
{
    CHECK_EQ(~L, H);
    CHECK_EQ(~H, L);
    CHECK_EQ(~X, X);
    CHECK_EQ(~Z, X);

    CHECK(isKnown(L) && isKnown(H) && !isKnown(X) && !isKnown(Z));
    CHECK_EQ(toLogic(true), H);
    CHECK_EQ(toLogic(false), L);
    CHECK_EQ(asInput(Z), X);
    CHECK_EQ(asInput(H), H);

    for (LogicValue v : kAll)
        CHECK(logicFromChar(toChar(v)) == v);
    CHECK(logicFromChar('x') == X);
    CHECK(logicFromChar('z') == Z);
    CHECK(!logicFromChar('2').has_value());
}
