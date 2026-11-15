#include <memory>
#include <string>
#include <vector>
#include "Simulator.h"
#include "TestFramework.h"

namespace
{
    constexpr LogicValue L = LogicValue::LOW;
    constexpr LogicValue H = LogicValue::HIGH;
    using T = BuiltinGate::Type;
    using Names = std::vector<std::string>;

    std::shared_ptr<Module> halfAdder()
    {
        auto m = std::make_shared<Module>("half_adder", Names{"a", "b"}, Names{"s", "c"});
        m->addGate(T::XOR, {"a", "b"}, {"s"}, "x");
        m->addGate(T::AND, {"a", "b"}, {"c"}, "g");
        return m;
    }

    std::shared_ptr<Module> fullAdder()
    {
        auto ha = halfAdder();
        auto m = std::make_shared<Module>("full_adder", Names{"a", "b", "cin"}, Names{"s", "cout"});
        m->addInstance(ha, {"a", "b"}, {"s1", "c1"}, "ha0");
        m->addInstance(ha, {"s1", "cin"}, {"s", "c2"}, "ha1");
        m->addGate(T::OR, {"c1", "c2"}, {"cout"}, "or0");
        return m;
    }
} // namespace

TEST(module_full_adder_exhaustive)
{
    Simulator sim;
    Net& a = sim.addNet("a");
    Net& b = sim.addNet("b");
    Net& ci = sim.addNet("ci");
    Net& s = sim.addNet("s");
    Net& co = sim.addNet("co");
    Input& ia = sim.addInput(&a);
    Input& ib = sim.addInput(&b);
    Input& ic = sim.addInput(&ci);
    const Simulator::Instance inst = sim.instantiate(*fullAdder(), {&a, &b, &ci}, {&s, &co}, "fa");

    CHECK_EQ(inst.gate_ids.size(), std::size_t{5});
    CHECK(sim.findNet("fa.s1") != nullptr);          // private nets are named
    CHECK(sim.findNet("fa.ha0.a") == nullptr);       // ports are not re-created
    CHECK(sim.findGate("fa.ha1.x") != nullptr);

    for (int v = 0; v < 8; ++v)
    {
        sim.setInput(ia, toLogic(v & 1));
        sim.setInput(ib, toLogic(v & 2));
        sim.setInput(ic, toLogic(v & 4));
        sim.runUntilIdle();
        const int sum = (v & 1) + ((v >> 1) & 1) + ((v >> 2) & 1);
        CHECK_EQ(s.getValue(), toLogic(sum & 1));
        CHECK_EQ(co.getValue(), toLogic(sum >> 1));
    }
}

TEST(module_nested_ripple_carry_adder)
{
    // 4-bit adder made of full adders made of half adders.
    auto fa = fullAdder();
    auto add4 = std::make_shared<Module>("add4",
        Names{"a0", "a1", "a2", "a3", "b0", "b1", "b2", "b3", "cin"},
        Names{"s0", "s1", "s2", "s3", "cout"});
    std::string carry = "cin";
    for (int i = 0; i < 4; ++i)
    {
        const std::string n = std::to_string(i);
        const std::string next = (i == 3) ? "cout" : "c" + n;
        add4->addInstance(fa, {"a" + n, "b" + n, carry}, {"s" + n, next}, "fa" + n);
        carry = next;
    }

    Simulator sim;
    std::vector<Net*> ins, outs;
    std::vector<Input*> src;
    for (int i = 0; i < 9; ++i)
    {
        ins.push_back(&sim.addNet("in" + std::to_string(i)));
        src.push_back(&sim.addInput(ins.back()));
    }
    for (int i = 0; i < 5; ++i)
        outs.push_back(&sim.addNet("out" + std::to_string(i)));
    sim.instantiate(*add4, ins, outs, "adder");
    CHECK_EQ(sim.gateCount(), std::size_t{9 + 4 * 5});

    for (int a = 0; a < 16; ++a)
    {
        for (int b = 0; b < 16; b += 3)
        {
            for (int i = 0; i < 4; ++i)
            {
                sim.setInput(*src[i], toLogic((a >> i) & 1));
                sim.setInput(*src[4 + i], toLogic((b >> i) & 1));
            }
            sim.setInput(*src[8], L);
            sim.runUntilIdle();

            int got = 0;
            for (int i = 0; i < 5; ++i)
                if (outs[i]->getValue() == H) got |= 1 << i;
            CHECK_EQ(got, a + b);
        }
    }
}

TEST(module_sequential_parts_and_unconnected_ports)
{
    // A register bit with an internal net and an unconnected QN.
    auto reg = std::make_shared<Module>("regbit", Names{"d", "clk", "en"}, Names{"q"});
    reg->addGate(T::AND, {"clk", "en"}, {"gclk"});
    reg->addFlipFlop(FlipFlop::Type::D, {"d", "gclk"}, {"q", ""}, "ff");

    Simulator sim;
    Net& d = sim.addNet("d");
    Net& clk = sim.addNet("clk");
    Net& q = sim.addNet("q");
    Input& din = sim.addInput(&d, "din", H);
    Input& cin = sim.addInput(&clk, "cin", L);
    // "en" left unconnected: the instance gets a private floating net, so
    // AND(clk, en) goes LOW -> X when clk rises: only a possible edge.
    const auto inst = sim.instantiate(*reg, {&d, &clk, nullptr}, {&q}, "r0");
    CHECK(sim.findNet("r0.en") != nullptr);

    sim.runUntilIdle();
    sim.setInput(cin, H);
    sim.runUntilIdle();
    CHECK_EQ(q.getValue(), LogicValue::UNKNOWN);   // might have loaded d=1
    (void)din;

    sim.removeInstance(inst);
    CHECK_EQ(sim.gateCount(), std::size_t{2});    // just the two inputs
    CHECK(sim.findNet("r0.en") == nullptr);
    CHECK(sim.findNet("r0.gclk") == nullptr);
}

TEST(module_validation)
{
    CHECK_THROWS(Module("m", Names{"a", "a"}, Names{}));
    CHECK_THROWS(Module("m", Names{"a"}, Names{"a"}));
    CHECK_THROWS(Module("m", Names{""}, Names{}));

    Module m("m", Names{"a", "b"}, Names{"y"});
    CHECK_THROWS(m.addGate(T::AND, {}, {"y"}));
    CHECK_THROWS(m.addGate(T::NOT, {"a", "b"}, {"y"}));
    m.addGate(T::AND, {"a", "b"}, {"y"}, "g");
    CHECK_THROWS(m.addGate(T::OR, {"a", "b"}, {"y"}, "g"));     // duplicate name
    CHECK_THROWS(m.addInstance(nullptr, {}, {}));
    CHECK_THROWS(m.addInstance(halfAdder(), {"a"}, {"y"}));     // wrong arity
    CHECK_THROWS(m.addPrimitive(Module::Factory(), {}, {}));

    Simulator sim;
    Net& n = sim.addNet();
    CHECK_THROWS(sim.instantiate(m, {&n}, {&n}));              // wrong arity
}

TEST(module_custom_primitive_factory)
{
    Module m("inv", Names{"a"}, Names{"y"});
    m.addPrimitive([](std::vector<Net*> in, std::vector<Net*> out, std::string name)
                   {
                       return std::make_unique<BuiltinGate>(T::NOT, std::move(in), std::move(out), std::move(name));
                   },
                   {"a"}, {"y"}, "custom");
    Simulator sim;
    Net& a = sim.addNet("a");
    Net& y = sim.addNet("y");
    sim.addInput(&a, "in", H);
    sim.instantiate(m, {&a}, {&y}, "u");
    sim.runUntilIdle();
    CHECK_EQ(y.getValue(), L);
    CHECK(sim.findGate("u.custom") != nullptr);
}
