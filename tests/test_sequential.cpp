#include <memory>
#include <vector>
#include "Simulator.h"
#include "TestFramework.h"

namespace
{
    constexpr LogicValue L = LogicValue::LOW;
    constexpr LogicValue H = LogicValue::HIGH;
    constexpr LogicValue X = LogicValue::UNKNOWN;
    using FF = FlipFlop::Type;
} // namespace

TEST(dff_captures_on_rising_edge_only)
{
    Simulator sim;
    Net& d = sim.addNet("d");
    Net& clk = sim.addNet("clk");
    Net& q = sim.addNet("q");
    Net& qn = sim.addNet("qn");
    Input& din = sim.addInput(&d, "din", L);
    Input& cin = sim.addInput(&clk, "cin", L);
    sim.addFlipFlop(FF::D, {&d, &clk}, {&q, &qn});

    sim.runUntilIdle();
    CHECK_EQ(q.getValue(), L);            // power-up state
    CHECK_EQ(qn.getValue(), H);

    sim.setInput(din, H);
    sim.runUntilIdle();
    CHECK_EQ(q.getValue(), L);            // no edge yet

    sim.setInput(cin, H);                 // rising edge
    sim.runUntilIdle();
    CHECK_EQ(q.getValue(), H);
    CHECK_EQ(qn.getValue(), L);

    sim.setInput(din, L);
    sim.runUntilIdle();
    CHECK_EQ(q.getValue(), H);            // holds while clock high

    sim.setInput(cin, L);                 // falling edge: ignored
    sim.runUntilIdle();
    CHECK_EQ(q.getValue(), H);

    sim.setInput(cin, H);
    sim.runUntilIdle();
    CHECK_EQ(q.getValue(), L);
}

TEST(dff_falling_edge_variant)
{
    Simulator sim;
    Net& d = sim.addNet("d");
    Net& clk = sim.addNet("clk");
    Net& q = sim.addNet("q");
    Input& din = sim.addInput(&d, "din", H);
    Input& cin = sim.addInput(&clk, "cin", L);
    sim.addFlipFlop(FF::D, {&d, &clk}, {&q}, "ff", FlipFlop::Edge::Falling);

    sim.setInput(cin, H);
    sim.runUntilIdle();
    CHECK_EQ(q.getValue(), L);
    sim.setInput(cin, L);
    sim.runUntilIdle();
    CHECK_EQ(q.getValue(), H);
    (void)din;
}

TEST(dff_async_set_reset)
{
    Simulator sim;
    Net& d = sim.addNet("d");
    Net& clk = sim.addNet("clk");
    Net& set = sim.addNet("set");
    Net& rst = sim.addNet("rst");
    Net& q = sim.addNet("q");
    sim.addInput(&d, "din", L);
    sim.addInput(&clk, "cin", L);
    Input& sin = sim.addInput(&set, "sin", L);
    Input& rin = sim.addInput(&rst, "rin", L);
    sim.addFlipFlop(FF::D, {&d, &clk, &set, &rst}, {&q});

    sim.setInput(sin, H);
    sim.runUntilIdle();
    CHECK_EQ(q.getValue(), H);            // set without any clock

    sim.setInput(rin, H);
    sim.runUntilIdle();
    CHECK_EQ(q.getValue(), L);            // reset wins over set

    sim.setInput(sin, L);
    sim.setInput(rin, X);
    sim.runUntilIdle();
    CHECK_EQ(q.getValue(), L);            // X reset cannot change a 0

    sim.setInput(rin, L);
    sim.setInput(sin, X);
    sim.runUntilIdle();
    CHECK_EQ(q.getValue(), X);            // X set might have set it
}

TEST(dff_unknown_clock_is_pessimistic)
{
    Simulator sim;
    Net& d = sim.addNet("d");
    Net& clk = sim.addNet("clk");
    Net& q = sim.addNet("q");
    Input& din = sim.addInput(&d, "din", L);
    Input& cin = sim.addInput(&clk, "cin", L);
    sim.addFlipFlop(FF::D, {&d, &clk}, {&q});

    sim.setInput(cin, X);                 // L -> X with D equal to Q: harmless
    sim.runUntilIdle();
    CHECK_EQ(q.getValue(), L);

    sim.setInput(cin, L);
    sim.setInput(din, H);
    sim.runUntilIdle();
    sim.setInput(cin, X);                 // L -> X with D != Q: unknown
    sim.runUntilIdle();
    CHECK_EQ(q.getValue(), X);
}

TEST(shift_register_has_no_race_with_zero_delay_and_skewed_clock)
{
    // ff1 -> ff2, with ff2's clock passing through a buffer. With zero delay
    // the buffered clock arrives a delta cycle after ff1 has sampled; ff2
    // must still see ff1's OLD output. This is what the Update region is for.
    Simulator sim;
    sim.setTimingModel(std::make_unique<ZeroDelayModel>());
    Net& d = sim.addNet("d");
    Net& clk = sim.addNet("clk");
    Net& clk2 = sim.addNet("clk2");
    Net& q1 = sim.addNet("q1");
    Net& q2 = sim.addNet("q2");
    Input& din = sim.addInput(&d, "din", L);
    Input& cin = sim.addInput(&clk, "cin", L);
    sim.addFlipFlop(FF::D, {&d, &clk}, {&q1}, "ff1");
    sim.addGate(BuiltinGate::Type::BUF, {&clk}, {&clk2}, "clkbuf");
    sim.addFlipFlop(FF::D, {&q1, &clk2}, {&q2}, "ff2");
    sim.runUntilIdle();

    sim.setInput(din, H);
    sim.runUntilIdle();
    sim.setInput(cin, H);
    sim.runUntilIdle();
    CHECK_EQ(q1.getValue(), H);
    CHECK_EQ(q2.getValue(), L);           // took the old q1, not the new one

    sim.setInput(cin, L);
    sim.runUntilIdle();
    sim.setInput(cin, H);
    sim.runUntilIdle();
    CHECK_EQ(q2.getValue(), H);
}

TEST(tff_counter_with_clock)
{
    // 3-bit ripple counter: each T flip-flop toggles on the falling edge of
    // the previous stage.
    Simulator sim;
    Net& clk = sim.addNet("clk");
    Net& one = sim.addNet("one");
    sim.addGate(BuiltinGate::Type::PULLUP, {}, {&one});
    sim.addClock(&clk, "clk", 5);

    std::vector<Net*> q;
    Net* prev = &clk;
    for (int i = 0; i < 3; ++i)
    {
        q.push_back(&sim.addNet("q" + std::to_string(i)));
        sim.addFlipFlop(FF::T, {&one, prev}, {q.back()}, "t" + std::to_string(i),
                        FlipFlop::Edge::Falling);
        prev = q.back();
    }

    auto count = [&]
    {
        int v = 0;
        for (int i = 0; i < 3; ++i)
            if (q[i]->getValue() == H) v |= 1 << i;
        return v;
    };

    // Falling edges at 10, 20, 30, ...; sample mid-period after ripple.
    for (int n = 1; n <= 9; ++n)
    {
        sim.run(10 * static_cast<uint64_t>(n) + 6);
        CHECK_EQ(count(), n % 8);
    }
}

TEST(first_clock_level_is_not_an_edge)
{
    // Setting the clock HIGH before the circuit ever ran: the flip-flop never
    // saw it LOW, so there was no rising edge.
    Simulator sim;
    Net& d = sim.addNet("d");
    Net& clk = sim.addNet("clk");
    Net& q = sim.addNet("q");
    sim.addInput(&d, "din", H);
    Input& cin = sim.addInput(&clk, "cin", L);
    sim.addFlipFlop(FF::D, {&d, &clk}, {&q});

    sim.setInput(cin, H);
    sim.runUntilIdle();
    CHECK_EQ(q.getValue(), L);
}

TEST(jk_and_sr_flip_flops)
{
    Simulator sim;
    Net& j = sim.addNet("j");
    Net& k = sim.addNet("k");
    Net& clk = sim.addNet("clk");
    Net& q = sim.addNet("q");
    Net& sq = sim.addNet("sq");
    Input& jin = sim.addInput(&j, "jin", L);
    Input& kin = sim.addInput(&k, "kin", L);
    Input& cin = sim.addInput(&clk, "cin", L);
    sim.addFlipFlop(FF::JK, {&j, &k, &clk}, {&q});
    sim.addFlipFlop(FF::SR, {&j, &k, &clk}, {&sq});

    auto pulse = [&]
    {
        sim.setInput(cin, H);
        sim.runUntilIdle();
        sim.setInput(cin, L);
        sim.runUntilIdle();
    };
    sim.runUntilIdle();                     // clock must be seen LOW first

    sim.setInput(jin, H); pulse();          // J=1 K=0: set
    CHECK_EQ(q.getValue(), H);
    CHECK_EQ(sq.getValue(), H);

    sim.setInput(jin, L); pulse();          // hold
    CHECK_EQ(q.getValue(), H);
    CHECK_EQ(sq.getValue(), H);

    sim.setInput(kin, H); pulse();          // J=0 K=1: reset
    CHECK_EQ(q.getValue(), L);
    CHECK_EQ(sq.getValue(), L);

    sim.setInput(jin, H); pulse();          // J=K=1: JK toggles, SR invalid
    CHECK_EQ(q.getValue(), H);
    CHECK_EQ(sq.getValue(), X);
    pulse();
    CHECK_EQ(q.getValue(), L);
}

TEST(d_latch_is_transparent_while_enabled)
{
    Simulator sim;
    Net& d = sim.addNet("d");
    Net& en = sim.addNet("en");
    Net& q = sim.addNet("q");
    Input& din = sim.addInput(&d, "din", L);
    Input& ein = sim.addInput(&en, "ein", H);
    sim.addLatch({&d, &en}, {&q});

    sim.setInput(din, H);
    sim.runUntilIdle();
    CHECK_EQ(q.getValue(), H);             // follows D

    sim.setInput(ein, L);
    sim.setInput(din, L);
    sim.runUntilIdle();
    CHECK_EQ(q.getValue(), H);             // holds

    sim.setInput(ein, X);
    sim.runUntilIdle();
    CHECK_EQ(q.getValue(), X);             // might be open, D differs
}

TEST(sr_latch_from_nor_gates)
{
    // Classic cross-coupled NOR latch built from primitives.
    Simulator sim;
    Net& s = sim.addNet("s");
    Net& r = sim.addNet("r");
    Net& q = sim.addNet("q");
    Net& qn = sim.addNet("qn");
    Input& sin = sim.addInput(&s, "sin", L);
    Input& rin = sim.addInput(&r, "rin", H);
    sim.addGate(BuiltinGate::Type::NOR, {&r, &qn}, {&q});
    sim.addGate(BuiltinGate::Type::NOR, {&s, &q}, {&qn});

    CHECK(sim.runUntilIdle() == Simulator::Result::Idle);
    CHECK_EQ(q.getValue(), L);

    sim.setInput(rin, L);                  // hold
    sim.runUntilIdle();
    CHECK_EQ(q.getValue(), L);
    CHECK_EQ(qn.getValue(), H);

    sim.setInput(sin, H);                  // set
    sim.runUntilIdle();
    sim.setInput(sin, L);
    sim.runUntilIdle();
    CHECK_EQ(q.getValue(), H);
    CHECK_EQ(qn.getValue(), L);
}

TEST(flip_flop_rejects_too_many_pins)
{
    Simulator sim;
    Net& a = sim.addNet();
    CHECK_THROWS(sim.addFlipFlop(FF::D, {&a, &a, &a, &a, &a}, {&a}));
    CHECK_THROWS(sim.addFlipFlop(FF::D, {&a}, {&a, &a, &a}));
    CHECK_THROWS(sim.addLatch({&a, &a, &a}, {}));
}

TEST(reset_restores_power_on_state)
{
    Simulator sim;
    Net& d = sim.addNet("d");
    Net& clk = sim.addNet("clk");
    Net& q = sim.addNet("q");
    sim.addInput(&d, "din", H);
    Input& cin = sim.addInput(&clk, "cin", L);
    FlipFlop& ff = sim.addFlipFlop(FF::D, {&d, &clk}, {&q});

    sim.runUntilIdle();
    sim.setInput(cin, H);
    sim.runUntilIdle();
    CHECK_EQ(q.getValue(), H);
    CHECK(sim.now() > 0);

    sim.reset();
    CHECK_EQ(sim.now(), uint64_t{0});
    CHECK_EQ(ff.state(), L);
    sim.runUntilIdle();
    // cin is still HIGH (inputs keep their value), but reset forgot the old
    // clock level, so the first HIGH after reset is not an edge.
    CHECK_EQ(q.getValue(), L);

    ff.setInitialState(X);
    sim.reset();
    sim.runUntilIdle();
    CHECK_EQ(q.getValue(), X);
}
