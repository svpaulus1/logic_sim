#include <memory>
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

    /// Output of a single gate of type @p t for the given input values.
    LogicValue eval(T t, std::vector<LogicValue> values)
    {
        Simulator sim;
        std::vector<Net*> ins;
        std::vector<Input*> srcs;
        for (std::size_t i = 0; i < values.size(); ++i)
        {
            Net& n = sim.addNet();
            ins.push_back(&n);
            srcs.push_back(&sim.addInput(&n));
        }
        Net& y = sim.addNet("y");
        sim.addGate(t, ins, {&y});
        for (std::size_t i = 0; i < values.size(); ++i)
            sim.setInput(*srcs[i], values[i]);
        sim.runUntilIdle();
        return y.getValue();
    }
} // namespace

TEST(gate_two_input_truth_tables)
{
    struct Row { T type; LogicValue ll, lh, hl, hh; };
    const Row rows[] = {
        {T::AND,  L, L, L, H},
        {T::NAND, H, H, H, L},
        {T::OR,   L, H, H, H},
        {T::NOR,  H, L, L, L},
        {T::XOR,  L, H, H, L},
        {T::XNOR, H, L, L, H},
    };
    for (const Row& r : rows)
    {
        CHECK_EQ(eval(r.type, {L, L}), r.ll);
        CHECK_EQ(eval(r.type, {L, H}), r.lh);
        CHECK_EQ(eval(r.type, {H, L}), r.hl);
        CHECK_EQ(eval(r.type, {H, H}), r.hh);
    }
}

TEST(gate_unknowns_and_dominance)
{
    CHECK_EQ(eval(T::AND, {L, X}), L);   // 0 dominates AND
    CHECK_EQ(eval(T::AND, {H, X}), X);
    CHECK_EQ(eval(T::OR,  {H, Z}), H);   // 1 dominates OR
    CHECK_EQ(eval(T::OR,  {L, Z}), X);
    CHECK_EQ(eval(T::NAND, {L, X}), H);
    CHECK_EQ(eval(T::XOR, {H, X}), X);
}

TEST(gate_wide_folds)
{
    CHECK_EQ(eval(T::AND, {H, H, H, H, H}), H);
    CHECK_EQ(eval(T::AND, {H, H, L, H, H}), L);
    CHECK_EQ(eval(T::XOR, {H, H, H}), H);   // odd parity
    CHECK_EQ(eval(T::NOR, {L, L, L}), H);
}

TEST(gate_never_passes_highz_through)
{
    // A floating input must read as X; only enable gates may output Z.
    CHECK_EQ(eval(T::BUF, {Z}), X);
    CHECK_EQ(eval(T::NOT, {Z}), X);
    CHECK_EQ(eval(T::AND, {Z}), X);      // single-input fold
    CHECK_EQ(eval(T::BUFIF1, {Z, H}), X);
}

TEST(gate_buf_not_drive_every_output)
{
    Simulator sim;
    Net& a = sim.addNet("a");
    Net& y1 = sim.addNet("y1");
    Net& y2 = sim.addNet("y2");
    Net& y3 = sim.addNet("y3");
    Input& in = sim.addInput(&a);
    sim.addGate(T::NOT, {&a}, {&y1, &y2, &y3});

    sim.setInput(in, H);
    sim.runUntilIdle();
    CHECK_EQ(y1.getValue(), L);
    CHECK_EQ(y2.getValue(), L);
    CHECK_EQ(y3.getValue(), L);
}

TEST(gate_enable_gates)
{
    CHECK_EQ(eval(T::BUFIF1, {H, H}), H);
    CHECK_EQ(eval(T::BUFIF1, {L, H}), L);
    CHECK_EQ(eval(T::BUFIF1, {H, L}), Z);
    CHECK_EQ(eval(T::BUFIF1, {H, X}), X);
    CHECK_EQ(eval(T::BUFIF0, {H, L}), H);
    CHECK_EQ(eval(T::BUFIF0, {H, H}), Z);
    CHECK_EQ(eval(T::NOTIF1, {H, H}), L);
    CHECK_EQ(eval(T::NOTIF1, {H, L}), Z);
    CHECK_EQ(eval(T::NOTIF0, {L, L}), H);
    CHECK_EQ(eval(T::NOTIF0, {L, Z}), X);
}

TEST(gate_pullup_holds_released_bus_and_loses_to_drivers)
{
    Simulator sim;
    Net& d0 = sim.addNet("d0");
    Net& d1 = sim.addNet("d1");
    Net& e0 = sim.addNet("e0");
    Net& e1 = sim.addNet("e1");
    Net& bus = sim.addNet("bus");
    Input& in0 = sim.addInput(&d0, "in0", L);
    Input& in1 = sim.addInput(&d1, "in1", H);
    Input& en0 = sim.addInput(&e0, "en0", L);
    Input& en1 = sim.addInput(&e1, "en1", L);
    sim.addGate(T::BUFIF1, {&d0, &e0}, {&bus});
    sim.addGate(T::BUFIF1, {&d1, &e1}, {&bus});
    sim.addGate(T::PULLUP, {}, {&bus});

    sim.runUntilIdle();
    CHECK_EQ(bus.getValue(), H);          // nobody enabled: pullup

    sim.setInput(en0, H);
    sim.runUntilIdle();
    CHECK_EQ(bus.getValue(), L);          // driver 0 pulls it low

    sim.setInput(en1, H);
    sim.runUntilIdle();
    CHECK_EQ(bus.getValue(), X);          // bus fight
    CHECK(bus.hasConflict());

    sim.setInput(in0, H);
    sim.runUntilIdle();
    CHECK_EQ(bus.getValue(), H);          // both drive 1: fine

    sim.setInput(en0, L);
    sim.setInput(en1, L);
    sim.setInput(in1, L);
    sim.runUntilIdle();
    CHECK_EQ(bus.getValue(), H);          // released again
}

TEST(gate_pulldown_alone)
{
    Simulator sim;
    Net& n = sim.addNet();
    sim.addGate(T::PULLDOWN, {}, {&n});
    sim.runUntilIdle();
    CHECK_EQ(n.getValue(), L);
}

TEST(gate_pin_count_validation)
{
    Simulator sim;
    Net& a = sim.addNet();
    Net& b = sim.addNet();
    Net& y = sim.addNet();
    Net& y2 = sim.addNet();

    CHECK_THROWS(sim.addGate(T::AND, {}, {&y}));
    CHECK_THROWS(sim.addGate(T::AND, {&a, &b}, {}));
    CHECK_THROWS(sim.addGate(T::AND, {&a, &b}, {&y, &y2}));
    CHECK_THROWS(sim.addGate(T::BUF, {&a, &b}, {&y}));
    CHECK_THROWS(sim.addGate(T::BUFIF1, {&a}, {&y}));
    CHECK_THROWS(sim.addGate(T::BUFIF1, {&a, &b}, {&y, &y2}));
    CHECK_THROWS(sim.addGate(T::PULLUP, {&a}, {&y}));
    CHECK_THROWS(sim.addGate(T::PULLUP, {}, {}));

    // A failed construction must not leave the gate registered on any net.
    CHECK_EQ(a.sinkCount(), std::size_t{0});
    CHECK_EQ(y.driverCount(), std::size_t{0});
    CHECK_EQ(sim.gateCount(), std::size_t{0});
}

TEST(gate_names_round_trip)
{
    for (std::size_t i = 0; i < static_cast<std::size_t>(T::COUNT); ++i)
    {
        const T t = static_cast<T>(i);
        CHECK(BuiltinGate::typeFromName(BuiltinGate::gateName(t)) == t);
    }
    CHECK(!BuiltinGate::typeFromName("frobnicate").has_value());
}

TEST(gate_unconnected_pins)
{
    // A gate may be placed before it is wired (GUI workflow).
    Simulator sim;
    Net& y = sim.addNet("y");
    BuiltinGate& g = sim.addGate(T::AND, {nullptr, nullptr}, {&y});
    sim.runUntilIdle();
    CHECK_EQ(y.getValue(), X);

    Net& a = sim.addNet("a");
    Net& b = sim.addNet("b");
    sim.addInput(&a, "a", H);
    sim.addInput(&b, "b", H);
    sim.connectInput(g, 0, &a);
    sim.connectInput(g, 1, &b);
    sim.runUntilIdle();
    CHECK_EQ(y.getValue(), H);

    sim.connectInput(g, 1, nullptr);
    sim.runUntilIdle();
    CHECK_EQ(y.getValue(), X);
}
