#include <memory>
#include <string>
#include <vector>
#include "Register.h"
#include "Simulator.h"
#include "TestFramework.h"

namespace
{
    constexpr LogicValue L = LogicValue::LOW;
    constexpr LogicValue H = LogicValue::HIGH;
    constexpr LogicValue X = LogicValue::UNKNOWN;
    constexpr LogicValue Z = LogicValue::HIGHZ;

    /// n-bit register with an Input on D and CLK, and optionally on EN,
    /// RESET and OE (left unconnected otherwise).
    struct Bench
    {
        Simulator sim;
        std::vector<Input*> d;
        Input* clk = nullptr;
        Input* en = nullptr;
        Input* rst = nullptr;
        Input* oe = nullptr;
        std::vector<Net*> q;
        Register* reg = nullptr;

        explicit Bench(std::size_t n, Register::Edge edge = Register::Edge::Rising, bool controls = true)
        {
            std::vector<Net*> ins;
            for (std::size_t i = 0; i < n; ++i)
            {
                ins.push_back(&sim.addNet());
                d.push_back(&sim.addInput(ins.back()));
            }
            ins.push_back(&sim.addNet("clk"));
            clk = &sim.addInput(ins.back(), "CLK", L);
            if (controls)
            {
                ins.push_back(&sim.addNet("en"));
                en = &sim.addInput(ins.back(), "EN", H);
                ins.push_back(&sim.addNet("rst"));
                rst = &sim.addInput(ins.back(), "RESET", L);
                ins.push_back(&sim.addNet("oe"));
                oe = &sim.addInput(ins.back(), "OE", H);
            }
            for (std::size_t i = 0; i < n; ++i) q.push_back(&sim.addNet());
            reg = &sim.emplace<Register>(ins, q, "reg", n, edge);
            sim.runUntilIdle();
        }

        void data(unsigned v)
        {
            for (std::size_t i = 0; i < d.size(); ++i) sim.setInput(*d[i], toLogic((v >> i) & 1));
            sim.runUntilIdle();
        }
        void set(Input* in, LogicValue v)
        {
            sim.setInput(*in, v);
            sim.runUntilIdle();
        }
        void pulse()
        {
            set(clk, H);
            set(clk, L);
        }
        int out() const   // -1 if any output is not 0/1
        {
            int v = 0;
            for (std::size_t i = 0; i < q.size(); ++i)
            {
                if (!isKnown(q[i]->getValue())) return -1;
                if (q[i]->getValue() == H) v |= 1 << i;
            }
            return v;
        }
        std::string bits() const   // Q0 first, e.g. "1XX0"
        {
            std::string s;
            for (Net* n : q) s += toChar(n->getValue());
            return s;
        }
    };
} // namespace

TEST(register_loads_on_rising_edge_only)
{
    Bench b(4);
    CHECK_EQ(b.out(), 0);                 // power-up value
    b.data(0b1010);
    CHECK_EQ(b.out(), 0);                 // no edge yet
    b.set(b.clk, H);
    CHECK_EQ(b.out(), 0b1010);
    b.data(0b0101);
    CHECK_EQ(b.out(), 0b1010);            // holds while the clock is high
    b.set(b.clk, L);
    CHECK_EQ(b.out(), 0b1010);            // falling edge ignored
    b.set(b.clk, H);
    CHECK_EQ(b.out(), 0b0101);
}

TEST(register_enable_holds_and_unconnected_enable_loads)
{
    Bench b(4);
    b.set(b.en, L);
    b.data(9);
    b.pulse();
    CHECK_EQ(b.out(), 0);                 // disabled: holds
    b.set(b.en, H);
    b.pulse();
    CHECK_EQ(b.out(), 9);

    Bench plain(4, Register::Edge::Rising, /*controls=*/false);   // D and CLK only
    plain.data(6);
    plain.pulse();
    CHECK_EQ(plain.out(), 6);
}

TEST(register_async_reset)
{
    Bench b(4);
    b.data(15);
    b.pulse();
    CHECK_EQ(b.out(), 15);
    b.set(b.rst, H);
    CHECK_EQ(b.out(), 0);                 // cleared with no clock edge
    b.set(b.clk, H);
    CHECK_EQ(b.out(), 0);                 // reset wins over an edge
    b.set(b.rst, L);
    CHECK_EQ(b.out(), 0);                 // releasing it does not replay the edge
    b.set(b.clk, L);
    b.pulse();
    CHECK_EQ(b.out(), 15);
}

TEST(register_falling_edge)
{
    Bench b(4, Register::Edge::Falling);
    b.data(6);
    b.set(b.clk, H);
    CHECK_EQ(b.out(), 0);
    b.set(b.clk, L);
    CHECK_EQ(b.out(), 6);
}

TEST(register_output_enable_shares_a_bus)
{
    // Two 4-bit registers driving the same 4-bit bus, picked by OE.
    Simulator sim;
    std::vector<Net*> bus;
    for (int i = 0; i < 4; ++i) bus.push_back(&sim.addNet("bus" + std::to_string(i)));
    Net& clk = sim.addNet("clk");
    Input& cin = sim.addInput(&clk, "CLK", L);

    struct Side { std::vector<Input*> d; Input* en; Input* oe; };
    auto make = [&](const std::string& tag)
    {
        Side s;
        std::vector<Net*> ins;
        for (int i = 0; i < 4; ++i)
        {
            ins.push_back(&sim.addNet());
            s.d.push_back(&sim.addInput(ins.back()));
        }
        ins.push_back(&clk);
        ins.push_back(&sim.addNet()); s.en = &sim.addInput(ins.back(), "", L);
        ins.push_back(nullptr);       // RESET unconnected
        ins.push_back(&sim.addNet()); s.oe = &sim.addInput(ins.back(), "", L);
        sim.emplace<Register>(ins, bus, tag, std::size_t{4});
        return s;
    };
    Side a = make("a");
    Side b = make("b");
    auto store = [&](Side& s, unsigned v)
    {
        for (int i = 0; i < 4; ++i) sim.setInput(*s.d[i], toLogic((v >> i) & 1));
        sim.setInput(*s.en, H);
        sim.setInput(cin, H); sim.runUntilIdle();
        sim.setInput(cin, L); sim.runUntilIdle();
        sim.setInput(*s.en, L); sim.runUntilIdle();
    };
    auto busValue = [&]
    {
        std::string s;
        for (Net* n : bus) s += toChar(n->getValue());
        return s;   // bit 0 first
    };

    sim.runUntilIdle();   // let CLK settle LOW first, or the first pulse is no edge
    store(a, 0b0011);
    store(b, 0b1100);
    CHECK_EQ(busValue(), std::string("ZZZZ"));   // nobody driving

    sim.setInput(*a.oe, H); sim.runUntilIdle();
    CHECK_EQ(busValue(), std::string("1100"));   // a = 0b0011

    sim.setInput(*a.oe, L); sim.setInput(*b.oe, H); sim.runUntilIdle();
    CHECK_EQ(busValue(), std::string("0011"));   // b = 0b1100

    sim.setInput(*a.oe, H); sim.runUntilIdle();  // both drive: every bit differs
    CHECK_EQ(busValue(), std::string("XXXX"));
    CHECK(bus[0]->hasConflict());
}

TEST(register_unknown_inputs_are_per_bit)
{
    Bench b(4);
    b.data(0b0011);
    b.pulse();

    // Clock L -> X with D = 0b0101: only bits where D equals the stored
    // value stay known.  Q0..Q3 = 1, X, X, 0.
    b.data(0b0101);
    b.set(b.clk, X);
    CHECK_EQ(b.bits(), std::string("1XX0"));

    // EN unknown on a clean edge: same rule.
    b.set(b.rst, H);
    b.set(b.rst, L);
    b.set(b.clk, L);
    b.data(0b0110);
    b.set(b.en, X);
    b.set(b.clk, H);
    CHECK_EQ(b.bits(), std::string("0XX0"));

    // RESET unknown: bits already 0 stay 0.
    b.set(b.clk, L);
    b.set(b.en, H);
    b.data(0b1001);
    b.pulse();
    b.set(b.rst, X);
    CHECK_EQ(b.bits(), std::string("X00X"));

    // OE unknown: outputs X, stored value untouched.
    Bench c(4);
    c.data(15);
    c.pulse();
    c.set(c.oe, X);
    CHECK_EQ(c.bits(), std::string("XXXX"));
    CHECK(c.reg->state() == std::vector<LogicValue>(4, H));
    c.set(c.oe, H);
    CHECK_EQ(c.out(), 15);
}

TEST(register_no_race_with_zero_delay_and_skewed_clock)
{
    // reg1 -> reg2, reg2's clock through a buffer: reg2 must take reg1's OLD
    // value even though its clock arrives a delta cycle late.
    Simulator sim;
    sim.setTimingModel(std::make_unique<ZeroDelayModel>());
    std::vector<Net*> d, q1, q2;
    std::vector<Input*> din;
    for (int i = 0; i < 2; ++i)
    {
        d.push_back(&sim.addNet()); din.push_back(&sim.addInput(d.back()));
        q1.push_back(&sim.addNet());
        q2.push_back(&sim.addNet());
    }
    Net& clk = sim.addNet("clk");
    Net& clk2 = sim.addNet("clk2");
    Input& cin = sim.addInput(&clk, "CLK", L);
    sim.addGate(BuiltinGate::Type::BUF, {&clk}, {&clk2});
    sim.emplace<Register>(std::vector<Net*>{d[0], d[1], &clk}, q1, "r1", std::size_t{2});
    sim.emplace<Register>(std::vector<Net*>{q1[0], q1[1], &clk2}, q2, "r2", std::size_t{2});
    sim.runUntilIdle();

    sim.setInput(*din[0], H); sim.setInput(*din[1], H);
    sim.setInput(cin, H); sim.runUntilIdle();
    CHECK_EQ(q1[0]->getValue(), H);
    CHECK_EQ(q2[0]->getValue(), L);       // old value of r1
    sim.setInput(cin, L); sim.runUntilIdle();
    sim.setInput(cin, H); sim.runUntilIdle();
    CHECK_EQ(q2[0]->getValue(), H);
}

TEST(register_feedback_toggles_with_clock)
{
    // Q0 -> NOT -> D0: the register flips on every rising edge.
    Simulator sim;
    Net& q = sim.addNet("q");
    Net& nq = sim.addNet("nq");
    Net& clk = sim.addNet("clk");
    sim.addClock(&clk, "clk", 5);            // rising edges at 5, 15, 25, ...
    sim.addGate(BuiltinGate::Type::NOT, {&q}, {&nq});
    sim.emplace<Register>(std::vector<Net*>{&nq, &clk}, std::vector<Net*>{&q}, "t", std::size_t{1});

    sim.run(4);  CHECK_EQ(q.getValue(), L);
    sim.run(9);  CHECK_EQ(q.getValue(), H);
    sim.run(19); CHECK_EQ(q.getValue(), L);
    sim.run(29); CHECK_EQ(q.getValue(), H);
}

TEST(register_reset_and_initial_state)
{
    Bench b(4);
    b.data(7);
    b.pulse();
    CHECK_EQ(b.out(), 7);

    b.sim.reset();
    CHECK(b.reg->state() == std::vector<LogicValue>(4, L));
    b.sim.runUntilIdle();
    CHECK_EQ(b.out(), 0);

    b.reg->setInitialState(X);
    b.sim.reset();
    b.sim.runUntilIdle();
    CHECK_EQ(b.bits(), std::string("XXXX"));
}

TEST(register_pins_names_and_validation)
{
    Simulator sim;
    Register& r = sim.emplace<Register>(std::vector<Net*>{}, std::vector<Net*>{}, "r", std::size_t{8});
    CHECK_EQ(r.clockPin(), std::size_t{8});
    CHECK_EQ(r.enablePin(), std::size_t{9});
    CHECK_EQ(r.resetPin(), std::size_t{10});
    CHECK_EQ(r.outputEnablePin(), std::size_t{11});
    CHECK_EQ(r.numInputs(), std::size_t{12});
    CHECK_EQ(r.numOutputs(), std::size_t{8});
    CHECK_EQ(std::string(r.typeName()), std::string("register"));
    CHECK(r.outputRegion() == Region::Update);

    Net& n = sim.addNet();
    using V = std::vector<Net*>;
    CHECK_THROWS(sim.emplace<Register>(V{}, V{}, "a", std::size_t{0}));
    CHECK_THROWS(sim.emplace<Register>(V(6, &n), V{}, "b", std::size_t{1}));   // 1 + 4 pins max
    CHECK_THROWS(sim.emplace<Register>(V{}, V{&n, &n}, "c", std::size_t{1}));
    CHECK_EQ(n.sinkCount(), std::size_t{0});
    (void)Z;
}
