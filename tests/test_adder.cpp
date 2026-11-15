#include <memory>
#include <string>
#include <vector>
#include "Adder.h"
#include "Simulator.h"
#include "TestFramework.h"

namespace
{
    constexpr LogicValue L = LogicValue::LOW;
    constexpr LogicValue H = LogicValue::HIGH;
    constexpr LogicValue X = LogicValue::UNKNOWN;

    /// N-bit adder with an Input on every pin (or no carry-in input at all).
    struct Bench
    {
        Simulator sim;
        std::vector<Input*> a, b;
        Input* cin = nullptr;
        std::vector<Net*> s;
        Net* cout = nullptr;

        explicit Bench(std::size_t n, bool with_cin = true)
        {
            std::vector<Net*> ins, outs;
            for (auto* v : {&a, &b})
                for (std::size_t i = 0; i < n; ++i)
                {
                    Net& net = sim.addNet();
                    ins.push_back(&net);
                    v->push_back(&sim.addInput(&net));
                }
            if (with_cin)
            {
                Net& net = sim.addNet("cin");
                ins.push_back(&net);
                cin = &sim.addInput(&net);
            }
            for (std::size_t i = 0; i <= n; ++i)
                outs.push_back(&sim.addNet());
            cout = outs.back();
            s.assign(outs.begin(), outs.end() - 1);
            sim.emplace<Adder>(ins, outs, "add", n);
        }

        void set(unsigned x, unsigned y, bool c)
        {
            for (std::size_t i = 0; i < a.size(); ++i)
            {
                sim.setInput(*a[i], toLogic((x >> i) & 1));
                sim.setInput(*b[i], toLogic((y >> i) & 1));
            }
            if (cin) sim.setInput(*cin, toLogic(c));
            sim.runUntilIdle();
        }

        int result() const   // -1 if any output is not 0/1
        {
            int v = 0;
            for (std::size_t i = 0; i < s.size(); ++i)
            {
                if (!isKnown(s[i]->getValue())) return -1;
                if (s[i]->getValue() == H) v |= 1 << i;
            }
            if (!isKnown(cout->getValue())) return -1;
            if (cout->getValue() == H) v |= 1 << s.size();
            return v;
        }
    };
} // namespace

TEST(adder_exhaustive_4bit)
{
    Bench t(4);
    for (unsigned x = 0; x < 16; ++x)
        for (unsigned y = 0; y < 16; ++y)
            for (int c = 0; c < 2; ++c)
            {
                t.set(x, y, c);
                CHECK_EQ(t.result(), static_cast<int>(x + y + c));
            }
}

TEST(adder_one_bit_has_carry_out)
{
    Bench t(1);
    t.set(1, 1, false);
    CHECK_EQ(t.s[0]->getValue(), L);
    CHECK_EQ(t.cout->getValue(), H);
}

TEST(adder_unconnected_carry_in_is_zero)
{
    Bench t(4, /*with_cin=*/false);
    t.set(9, 5, false);
    CHECK_EQ(t.result(), 14);
}

TEST(adder_carry_formulas_differ_on_x)
{
    // a = 1, cin = 1, b = X: the carry is 1 whatever b is.
    const LogicValue a = H, b = X, c = H;
    CHECK_EQ((a & b) | (a & c) | (b & c), H);   // majority: exact
    CHECK_EQ((a & b) | (c & (a ^ b)), X);       // xor form: pessimistic

    Bench t(1);
    t.sim.setInput(*t.a[0], H);
    t.sim.setInput(*t.b[0], X);
    t.sim.setInput(*t.cin, H);
    t.sim.runUntilIdle();
    CHECK_EQ(t.s[0]->getValue(), X);
    CHECK_EQ(t.cout->getValue(), H);
}

TEST(adder_validation)
{
    Simulator sim;
    Net& n = sim.addNet();
    CHECK_THROWS(sim.emplace<Adder>(std::vector<Net*>{}, std::vector<Net*>{}, "a0", std::size_t{0}));
    CHECK_THROWS(sim.emplace<Adder>(std::vector<Net*>{&n, &n, &n, &n}, std::vector<Net*>{}, "a1", std::size_t{1}));
    CHECK_THROWS(sim.emplace<Adder>(std::vector<Net*>{}, std::vector<Net*>{&n, &n, &n}, "a2", std::size_t{1}));
    CHECK_EQ(n.sinkCount(), std::size_t{0});    // failed builds leave nothing behind
}

TEST(adder_timing_vs_gate_level_ripple)
{
    using Names = std::vector<std::string>;
    using T = BuiltinGate::Type;
    auto fa = std::make_shared<Module>("fa", Names{"a", "b", "cin"}, Names{"s", "cout"});
    fa->addGate(T::XOR, {"a", "b"}, {"p"});
    fa->addGate(T::XOR, {"p", "cin"}, {"s"});
    fa->addGate(T::AND, {"a", "b"}, {"g"});
    fa->addGate(T::AND, {"p", "cin"}, {"t"});
    fa->addGate(T::OR, {"g", "t"}, {"cout"});

    // 1111 + 0001: the carry has to ripple through all four bits.
    auto settle_time = [](Simulator& sim, Net* watch, const std::vector<Input*>& b0)
    {
        sim.runUntilIdle();
        sim.run(100);
        uint64_t last = 0;
        sim.addChangeListener([&](const Net& n, LogicValue, uint64_t t) { if (&n == watch) last = t; });
        sim.setInput(*b0[0], H);
        sim.runUntilIdle();
        return last - 100;
    };

    Bench prim(4);
    prim.set(15, 0, false);
    const uint64_t t_prim = settle_time(prim.sim, prim.cout, prim.b);

    Simulator sim;
    std::vector<Input*> a, b;
    std::vector<Net*> an, bn;
    for (int i = 0; i < 4; ++i)
    {
        an.push_back(&sim.addNet()); a.push_back(&sim.addInput(an.back(), "", H));
        bn.push_back(&sim.addNet()); b.push_back(&sim.addInput(bn.back(), "", L));
    }
    Net* carry = &sim.addNet("c0");
    sim.addInput(carry, "cin", L);
    for (int i = 0; i < 4; ++i)
    {
        Net& s = sim.addNet();
        Net& c = sim.addNet();
        sim.instantiate(*fa, {an[i], bn[i], carry}, {&s, &c});
        carry = &c;
    }
    const uint64_t t_mod = settle_time(sim, carry, b);

    CHECK_EQ(t_prim, uint64_t{1});    // one gate delay, whatever the width
    CHECK(t_mod > 4);                 // carry ripples gate by gate
    std::cout << "    carry-out settles after: primitive " << t_prim
              << " tick(s), gate-level module " << t_mod << " ticks\n";
}
