#include <memory>
#include <string>
#include <tuple>
#include <vector>
#include "Simulator.h"
#include "TestFramework.h"

namespace
{
    constexpr LogicValue L = LogicValue::LOW;
    constexpr LogicValue H = LogicValue::HIGH;
    constexpr LogicValue X = LogicValue::UNKNOWN;
    constexpr LogicValue Z = LogicValue::HIGHZ;
    using T = BuiltinGate::Type;
    using R = Simulator::Result;

    std::string show(R r)
    {
        switch (r)
        {
            case R::Ok: return "Ok";
            case R::Idle: return "Idle";
            case R::Oscillation: return "Oscillation";
        }
        return "?";
    }
} // namespace

TEST(sim_unit_delay_propagation_timing)
{
    // a -> NOT -> NOT -> NOT -> y, one tick per gate.
    Simulator sim;
    Net& a = sim.addNet("a");
    Net& b = sim.addNet("b");
    Net& c = sim.addNet("c");
    Net& y = sim.addNet("y");
    Input& in = sim.addInput(&a, "in", L);
    sim.addGate(T::NOT, {&a}, {&b});
    sim.addGate(T::NOT, {&b}, {&c});
    sim.addGate(T::NOT, {&c}, {&y});

    CHECK_EQ(show(sim.run(10)), show(R::Idle));
    CHECK_EQ(y.getValue(), H);

    sim.setInput(in, H);                   // at t=10
    sim.run(12);
    CHECK_EQ(y.getValue(), H);             // not there yet
    sim.run(13);
    CHECK_EQ(y.getValue(), L);             // three gate delays later
}

TEST(sim_inertial_delay_swallows_short_pulses)
{
    Simulator sim;
    Net& a = sim.addNet("a");
    Net& y = sim.addNet("y");
    Input& in = sim.addInput(&a, "in", L);
    BuiltinGate& buf = sim.addGate(T::BUF, {&a}, {&y});
    buf.setFixedDelays(5, 5, 5);

    sim.run(20);
    CHECK_EQ(y.getValue(), L);

    sim.scheduleInput(in, H, 30);
    sim.scheduleInput(in, L, 32);          // 2-tick pulse < 5-tick delay
    int changes = 0;
    sim.addChangeListener([&](const Net& n, LogicValue, uint64_t)
    {
        if (&n == &y) ++changes;
    });
    sim.run(100);
    CHECK_EQ(changes, 0);
    CHECK_EQ(y.getValue(), L);

    sim.scheduleInput(in, H, 110);
    sim.scheduleInput(in, L, 120);         // long enough to pass
    sim.run(200);
    CHECK_EQ(changes, 2);
}

TEST(sim_scheduled_stimulus_lands_at_exact_times)
{
    Simulator sim;
    Net& a = sim.addNet("a");
    Input& in = sim.addInput(&a, "in", L);
    std::vector<std::pair<uint64_t, LogicValue>> seen;
    sim.addChangeListener([&](const Net& n, LogicValue, uint64_t t)
    {
        if (&n == &a) seen.emplace_back(t, n.getValue());
    });

    sim.scheduleInput(in, H, 7);
    sim.scheduleInput(in, L, 3000);        // well past the wheel horizon
    sim.scheduleInput(in, X, 3001);
    sim.run(5000);

    CHECK_EQ(sim.now(), uint64_t{5000});
    CHECK_EQ(seen.size(), std::size_t{4});
    if (seen.size() == 4)
    {
        CHECK_EQ(seen[0].first, uint64_t{0});  CHECK_EQ(seen[0].second, L);
        CHECK_EQ(seen[1].first, uint64_t{7});  CHECK_EQ(seen[1].second, H);
        CHECK_EQ(seen[2].first, uint64_t{3000}); CHECK_EQ(seen[2].second, L);
        CHECK_EQ(seen[3].first, uint64_t{3001}); CHECK_EQ(seen[3].second, X);
    }
    CHECK_THROWS(sim.scheduleInput(in, H, 10));   // in the past now
}

TEST(sim_step_and_idle)
{
    Simulator sim;
    Net& a = sim.addNet("a");
    Net& y = sim.addNet("y");
    Input& in = sim.addInput(&a, "in", L);
    sim.addGate(T::NOT, {&a}, {&y}).setFixedDelays(4, 4, 4);

    CHECK_EQ(show(sim.step()), show(R::Ok));      // t=0: input commits
    CHECK_EQ(sim.now(), uint64_t{0});
    CHECK_EQ(show(sim.step()), show(R::Idle));    // t=4: y settles, then quiet
    CHECK_EQ(sim.now(), uint64_t{4});
    CHECK_EQ(y.getValue(), H);
    CHECK_EQ(show(sim.step()), show(R::Idle));    // nothing left
    CHECK_EQ(sim.now(), uint64_t{4});

    sim.setInput(in, H);
    CHECK(!sim.idle());
    sim.step();
    sim.step();
    CHECK_EQ(sim.now(), uint64_t{8});
    CHECK_EQ(y.getValue(), L);
}

TEST(sim_zero_delay_ring_oscillator_is_detected)
{
    Simulator sim;
    sim.setTimingModel(std::make_unique<ZeroDelayModel>());
    sim.setMaxDeltaCycles(200);
    Net& a = sim.addNet("a");
    Net& b = sim.addNet("b");
    Net& c = sim.addNet("c");
    Net& en = sim.addNet("en");
    Input& go = sim.addInput(&en, "go", L);
    sim.addGate(T::NAND, {&en, &c}, {&a});   // enable-gated odd loop
    sim.addGate(T::NOT, {&a}, {&b});
    sim.addGate(T::NOT, {&b}, {&c});

    CHECK_EQ(show(sim.runUntilIdle()), show(R::Idle));   // disabled: stable

    sim.setInput(go, H);
    CHECK_EQ(show(sim.run(10)), show(R::Oscillation));
    CHECK(sim.oscillationDetected());
    CHECK_EQ(sim.now(), uint64_t{0});
    CHECK(!sim.oscillatingGates().empty());

    // Fix the circuit: break the loop, and the same timestep settles.
    sim.setInput(go, L);
    CHECK_EQ(show(sim.run(10)), show(R::Idle));
    CHECK_EQ(a.getValue(), H);
}

TEST(sim_unit_delay_ring_oscillator_just_toggles)
{
    Simulator sim;
    Net& a = sim.addNet("a");
    Net& b = sim.addNet("b");
    Net& c = sim.addNet("c");
    Net& en = sim.addNet("en");
    Input& go = sim.addInput(&en, "go", L);
    sim.addGate(T::NAND, {&en, &c}, {&a});
    sim.addGate(T::NOT, {&a}, {&b});
    sim.addGate(T::NOT, {&b}, {&c});

    // Started from X a ring stays X forever, so settle it disabled first.
    CHECK_EQ(show(sim.runUntilIdle()), show(R::Idle));
    sim.setInput(go, H);

    int toggles = 0;
    sim.addChangeListener([&](const Net& n, LogicValue old, uint64_t)
    {
        if (&n == &a && isKnown(old)) ++toggles;
    });
    CHECK_EQ(show(sim.runUntilIdle(100)), show(R::Ok));  // never goes quiet
    CHECK(toggles > 10);
}

TEST(sim_listener_sees_settled_values_only)
{
    // y = a AND NOT a has a zero-delay glitch on a rising edge of a, which
    // listeners must not see.
    Simulator sim;
    sim.setTimingModel(std::make_unique<ZeroDelayModel>());
    Net& a = sim.addNet("a");
    Net& na = sim.addNet("na");
    Net& y = sim.addNet("y");
    Input& in = sim.addInput(&a, "in", L);
    // AND first, so it reacts to a before NOT does and really glitches.
    sim.addGate(T::AND, {&a, &na}, {&y});
    sim.addGate(T::NOT, {&a}, {&na});
    sim.runUntilIdle();
    CHECK_EQ(y.getValue(), L);

    std::vector<std::tuple<std::string, LogicValue, LogicValue>> log;
    sim.addChangeListener([&](const Net& n, LogicValue old, uint64_t)
    {
        log.emplace_back(n.getName(), old, n.getValue());
    });
    sim.setInput(in, H);
    sim.runUntilIdle();

    CHECK_EQ(y.getValue(), L);
    for (const auto& e : log)
        CHECK(std::get<0>(e) != "y");
    CHECK_EQ(log.size(), std::size_t{2});     // a and na only
}

TEST(sim_remove_gate_mid_simulation)
{
    Simulator sim;
    Net& a = sim.addNet("a");
    Net& y = sim.addNet("y");
    Input& in = sim.addInput(&a, "in", L);
    BuiltinGate& g = sim.addGate(T::NOT, {&a}, {&y});
    g.setFixedDelays(50, 50, 50);
    sim.run(100);
    CHECK_EQ(y.getValue(), H);

    sim.setInput(in, H);                   // schedules y=0 at t=150
    sim.run(120);
    const uint32_t id = g.id();
    sim.removeGate(g);                     // pending event must be dropped
    CHECK(sim.gate(id) == nullptr);
    CHECK_EQ(sim.gateCount(), std::size_t{1});

    sim.run(300);
    CHECK_EQ(y.getValue(), Z);             // nothing drives y any more
    CHECK_EQ(a.sinkCount(), std::size_t{0});
}

TEST(sim_rewire_output_and_merge_nets)
{
    Simulator sim;
    Net& a = sim.addNet("a");
    Net& y1 = sim.addNet("y1");
    Net& y2 = sim.addNet("y2");
    Net& z = sim.addNet("z");
    sim.addInput(&a, "in", H);
    BuiltinGate& buf = sim.addGate(T::BUF, {&a}, {&y1});
    sim.addGate(T::NOT, {&y2}, {&z});
    sim.runUntilIdle();
    CHECK_EQ(y1.getValue(), H);
    CHECK_EQ(z.getValue(), X);             // y2 floating

    sim.connectOutput(buf, 0, &y2);        // move the wire
    sim.runUntilIdle();
    CHECK_EQ(y1.getValue(), Z);
    CHECK_EQ(y2.getValue(), H);
    CHECK_EQ(z.getValue(), L);

    // Join y1 into y2: y1 disappears, everything lands on y2.
    Net& extra_reader = sim.addNet("r");
    sim.addGate(T::BUF, {&y1}, {&extra_reader});
    const uint32_t y1_id = y1.id();
    sim.mergeNets(y2, y1);
    CHECK(sim.net(y1_id) == nullptr);
    sim.runUntilIdle();
    CHECK_EQ(extra_reader.getValue(), H);
    CHECK_EQ(y2.sinkCount(), std::size_t{2});
}

TEST(sim_remove_net_disconnects_pins)
{
    Simulator sim;
    Net& a = sim.addNet("a");
    Net& y = sim.addNet("y");
    sim.addInput(&a, "in", H);
    BuiltinGate& g = sim.addGate(T::BUF, {&a}, {&y});
    sim.runUntilIdle();

    sim.removeNet(a);
    CHECK(g.getInputs()[0] == nullptr);
    CHECK_EQ(sim.netCount(), std::size_t{1});
    sim.runUntilIdle();
    CHECK_EQ(y.getValue(), X);             // input now floating
}

TEST(sim_rejects_foreign_nets_and_gates)
{
    Simulator s1, s2;
    Net& a1 = s1.addNet("a");
    Net& y2 = s2.addNet("y");
    CHECK_THROWS(s2.addGate(T::BUF, {&a1}, {&y2}));
    CHECK_EQ(a1.sinkCount(), std::size_t{0});     // no dangling registration

    BuiltinGate& g = s1.addGate(T::BUF, {&a1}, {nullptr});
    CHECK_THROWS(s2.removeGate(g));
    CHECK_THROWS(s1.connectOutput(g, 0, &y2));
    CHECK_THROWS(s1.connectOutput(g, 5, nullptr));

    Net standalone;
    CHECK_THROWS(s1.forceNet(standalone, H));
}

TEST(sim_force_and_release)
{
    Simulator sim;
    Net& a = sim.addNet("a");
    Net& y = sim.addNet("y");
    sim.addInput(&a, "in", L);
    sim.addGate(T::NOT, {&a}, {&y});
    sim.runUntilIdle();
    CHECK_EQ(y.getValue(), H);

    sim.forceNet(a, H);
    sim.runUntilIdle();
    CHECK_EQ(y.getValue(), L);

    sim.releaseNet(a);
    sim.runUntilIdle();
    CHECK_EQ(y.getValue(), H);
}

TEST(sim_clock_period_and_pause)
{
    Simulator sim;
    Net& clk = sim.addNet("clk");
    Clock& c = sim.addClock(&clk, "clk", 3, 2);   // HIGH 3, LOW 2
    std::vector<uint64_t> edges;
    sim.addChangeListener([&](const Net& n, LogicValue, uint64_t t)
    {
        if (&n == &clk) edges.push_back(t);
    });

    sim.run(12);
    // Starts LOW at 0, HIGH at 2, LOW at 5, HIGH at 7, LOW at 10, HIGH at 12.
    CHECK(edges == (std::vector<uint64_t>{0, 2, 5, 7, 10, 12}));

    sim.setClockRunning(c, false);
    sim.run(100);
    CHECK_EQ(edges.size(), std::size_t{6});
    CHECK_EQ(clk.getValue(), H);
    CHECK(sim.idle());

    sim.setClockRunning(c, true);          // next edge a half-period later
    sim.run(104);
    CHECK_EQ(edges.back(), uint64_t{103});

    CHECK_THROWS(sim.addClock(&clk, "bad", 0));
}

TEST(sim_stage_delay_model_uses_fanout)
{
    Simulator sim;
    StageDelayModel* m = new StageDelayModel();
    m->ticks_per_stage = 2;
    m->ticks_per_load = 1;
    sim.setTimingModel(std::unique_ptr<TimingModel>(m));

    Net& a = sim.addNet("a");
    Net& y = sim.addNet("y");
    sim.addInput(&a, "in", L);
    BuiltinGate& nand = sim.addGate(T::NAND, {&a, &a}, {&y});
    sim.addGate(T::BUF, {&y}, {nullptr});
    sim.addGate(T::BUF, {&y}, {nullptr});
    sim.initialize();
    CHECK_EQ(nand.riseDelay(), uint32_t{1 * 2 + 2 * 1});   // 1 stage, fanout 2

    sim.addGate(T::BUF, {&y}, {nullptr});                  // fanout 3 now
    sim.initialize();
    CHECK_EQ(nand.riseDelay(), uint32_t{5});

    nand.setFixedDelays(9, 9, 9);                          // pinned
    sim.addGate(T::BUF, {&y}, {nullptr});
    sim.initialize();
    CHECK_EQ(nand.riseDelay(), uint32_t{9});
}

TEST(sim_ids_names_and_lookup)
{
    Simulator sim;
    Net& a = sim.addNet();
    Net& b = sim.addNet("bee");
    BuiltinGate& g = sim.addGate(T::AND, {&a, &b}, {nullptr});
    CHECK_EQ(a.getName(), std::string("n0"));
    CHECK_EQ(g.getName(), std::string("and0"));
    CHECK(sim.findNet("bee") == &b);
    CHECK(sim.findGate("and0") == &g);
    CHECK(sim.net(b.id()) == &b);
    CHECK(sim.gate(g.id()) == &g);
    CHECK(sim.net(999) == nullptr);
    CHECK_EQ(sim.nets().size(), std::size_t{2});
    CHECK_EQ(sim.gates().size(), std::size_t{1});
}
