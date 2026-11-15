#include <vector>
#include "Mux.h"
#include "Simulator.h"
#include "TestFramework.h"

namespace
{
    constexpr LogicValue L = LogicValue::LOW;
    constexpr LogicValue H = LogicValue::HIGH;
    constexpr LogicValue X = LogicValue::UNKNOWN;
    constexpr LogicValue Z = LogicValue::HIGHZ;

    /// w-input, b-bit mux with an Input on every data and select pin.
    struct Bench
    {
        Simulator sim;
        std::vector<std::vector<Input*>> d;   // d[input][bit]
        std::vector<Input*> s;
        std::vector<Net*> y;
        Mux* mux = nullptr;

        Bench(std::size_t w, std::size_t b)
        {
            std::vector<Net*> ins;
            d.resize(w);
            for (std::size_t i = 0; i < w; ++i)
                for (std::size_t j = 0; j < b; ++j)
                {
                    ins.push_back(&sim.addNet());
                    d[i].push_back(&sim.addInput(ins.back()));
                }
            std::size_t k = 0;
            while ((std::size_t{1} << k) < w) ++k;
            for (std::size_t j = 0; j < k; ++j)
            {
                ins.push_back(&sim.addNet());
                s.push_back(&sim.addInput(ins.back()));
            }
            for (std::size_t j = 0; j < b; ++j) y.push_back(&sim.addNet());
            mux = &sim.emplace<Mux>(ins, y, "mux", w, b);
        }

        void data(std::size_t i, unsigned value)
        {
            for (std::size_t j = 0; j < d[i].size(); ++j)
                sim.setInput(*d[i][j], toLogic((value >> j) & 1));
        }
        void select(std::vector<LogicValue> bits)   // S0 first
        {
            for (std::size_t j = 0; j < bits.size(); ++j) sim.setInput(*s[j], bits[j]);
            sim.runUntilIdle();
        }
        void select(unsigned value)
        {
            std::vector<LogicValue> bits;
            for (std::size_t j = 0; j < s.size(); ++j) bits.push_back(toLogic((value >> j) & 1));
            select(bits);
        }
        int out() const   // -1 if any output bit is not 0/1
        {
            int v = 0;
            for (std::size_t j = 0; j < y.size(); ++j)
            {
                if (!isKnown(y[j]->getValue())) return -1;
                if (y[j]->getValue() == H) v |= 1 << j;
            }
            return v;
        }
    };
} // namespace

TEST(mux_selects_each_input)
{
    Bench t(4, 3);                        // 4 inputs, 3 bits each
    const unsigned vals[] = {5, 2, 7, 0};
    for (std::size_t i = 0; i < 4; ++i) t.data(i, vals[i]);
    CHECK_EQ(t.mux->selectBits(), std::size_t{2});
    for (unsigned sel = 0; sel < 4; ++sel)
    {
        t.select(sel);
        CHECK_EQ(t.out(), static_cast<int>(vals[sel]));
    }
}

TEST(mux_wide_select_exhaustive)
{
    Bench t(8, 1);                        // 8:1, one bit
    for (unsigned pattern = 0; pattern < 256; pattern += 37)
    {
        for (std::size_t i = 0; i < 8; ++i) t.data(i, (pattern >> i) & 1);
        for (unsigned sel = 0; sel < 8; ++sel)
        {
            t.select(sel);
            CHECK_EQ(t.out(), static_cast<int>((pattern >> sel) & 1));
        }
    }
}

TEST(mux_out_of_range_select_is_x)
{
    Bench t(3, 2);                        // 3 inputs need 2 select bits
    t.data(0, 1); t.data(1, 2); t.data(2, 3);
    t.select(2u);
    CHECK_EQ(t.out(), 3);
    t.select(3u);                         // no input 3
    CHECK_EQ(t.y[0]->getValue(), X);
    CHECK_EQ(t.y[1]->getValue(), X);
}

TEST(mux_unknown_select_keeps_agreeing_bits)
{
    Bench t(4, 2);
    t.data(0, 0b01); t.data(1, 0b11); t.data(2, 0b00); t.data(3, 0b10);
    // S0 = X, S1 = 0: input 0 or 1. They agree on bit 0 (1), differ on bit 1.
    t.select({X, L});
    CHECK_EQ(t.y[0]->getValue(), H);
    CHECK_EQ(t.y[1]->getValue(), X);
    // S0 = 1, S1 = X: input 1 or 3. They agree on bit 1 (1), differ on bit 0.
    t.select({H, X});
    CHECK_EQ(t.y[0]->getValue(), X);
    CHECK_EQ(t.y[1]->getValue(), H);
    // Both unknown: all four inputs; no bit agrees.
    t.select({X, X});
    CHECK_EQ(t.out(), -1);
    // All inputs equal: the select doesn't matter.
    for (std::size_t i = 0; i < 4; ++i) t.data(i, 0b10);
    t.select({X, X});
    CHECK_EQ(t.out(), 0b10);
}

TEST(mux_unknown_select_that_could_reach_past_the_end_is_x)
{
    Bench t(3, 1);
    for (std::size_t i = 0; i < 3; ++i) t.data(i, 1);   // all inputs agree...
    t.select({X, H});                     // ...but input 2 or nonexistent 3
    CHECK_EQ(t.y[0]->getValue(), X);
    t.select({X, L});                     // input 0 or 1: both 1
    CHECK_EQ(t.y[0]->getValue(), H);
}

TEST(mux_floating_pins)
{
    Simulator sim;
    Net& y = sim.addNet("y");
    Net& a = sim.addNet("a");
    sim.addInput(&a, "A", H);
    // 2:1 mux, D0 = a, D1 and the select left unconnected.
    sim.emplace<Mux>(std::vector<Net*>{&a}, std::vector<Net*>{&y}, "m", std::size_t{2}, std::size_t{1});
    sim.runUntilIdle();
    CHECK_EQ(y.getValue(), X);            // floating select, inputs differ (1 vs X)
    (void)Z;
}

TEST(mux_validation)
{
    Simulator sim;
    Net& n = sim.addNet();
    using V = std::vector<Net*>;
    CHECK_THROWS(sim.emplace<Mux>(V{}, V{}, "a", std::size_t{0}, std::size_t{1}));
    CHECK_THROWS(sim.emplace<Mux>(V{}, V{}, "a1", std::size_t{1}, std::size_t{1}));
    CHECK_THROWS(sim.emplace<Mux>(V{}, V{}, "b", std::size_t{2}, std::size_t{0}));
    CHECK_THROWS(sim.emplace<Mux>(V{&n, &n, &n, &n}, V{}, "c", std::size_t{2}, std::size_t{1}));
    CHECK_THROWS(sim.emplace<Mux>(V{}, V{&n, &n}, "d", std::size_t{2}, std::size_t{1}));
    CHECK_EQ(n.sinkCount(), std::size_t{0});
}
