// File: main.cpp
// Demo: a full adder built from a Module, then a clocked 4-bit counter.
//
//   logicsim              print both demos
//   logicsim wave.vcd     also dump the counter's waveform for GTKWave

#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include "Simulator.h"
#include "VcdWriter.h"

namespace
{
    using T = BuiltinGate::Type;
    using Names = std::vector<std::string>;

    std::shared_ptr<Module> makeFullAdder()
    {
        auto ha = std::make_shared<Module>("half_adder", Names{"a", "b"}, Names{"s", "c"});
        ha->addGate(T::XOR, {"a", "b"}, {"s"});
        ha->addGate(T::AND, {"a", "b"}, {"c"});

        auto fa = std::make_shared<Module>("full_adder", Names{"a", "b", "cin"}, Names{"s", "cout"});
        fa->addInstance(ha, {"a", "b"}, {"s1", "c1"}, "ha0");
        fa->addInstance(ha, {"s1", "cin"}, {"s", "c2"}, "ha1");
        fa->addGate(T::OR, {"c1", "c2"}, {"cout"});
        return fa;
    }

    void fullAdderTable()
    {
        Simulator sim;
        Net& a = sim.addNet("a");
        Net& b = sim.addNet("b");
        Net& cin = sim.addNet("cin");
        Net& s = sim.addNet("s");
        Net& cout = sim.addNet("cout");
        Input& ia = sim.addInput(&a);
        Input& ib = sim.addInput(&b);
        Input& ic = sim.addInput(&cin);
        sim.instantiate(*makeFullAdder(), {&a, &b, &cin}, {&s, &cout}, "fa");

        std::cout << "Full adder (" << sim.gateCount() << " gates)\n"
                  << "  a b cin | s cout\n";
        for (int v = 0; v < 8; ++v)
        {
            sim.setInput(ia, toLogic(v & 1));
            sim.setInput(ib, toLogic(v & 2));
            sim.setInput(ic, toLogic(v & 4));
            sim.runUntilIdle();
            std::cout << "  " << toChar(a.getValue()) << ' ' << toChar(b.getValue())
                      << "  " << toChar(cin.getValue()) << "  | " << toChar(s.getValue())
                      << "  " << toChar(cout.getValue()) << '\n';
        }
    }

    void counter(const char* vcd_path)
    {
        // Synchronous 4-bit counter: bit i toggles when all lower bits are 1.
        Simulator sim;
        Net& clk = sim.addNet("clk");
        Net& rst = sim.addNet("rst");
        sim.addClock(&clk, "clk", 10);
        Input& reset = sim.addInput(&rst, "reset", LogicValue::HIGH);

        std::vector<Net*> q;
        Net* enable = &sim.addNet("one");
        sim.addGate(T::PULLUP, {}, {enable});
        for (int i = 0; i < 4; ++i)
        {
            const std::string n = std::to_string(i);
            q.push_back(&sim.addNet("q" + n));
            sim.addFlipFlop(FlipFlop::Type::T, {enable, &clk, nullptr, &rst}, {q.back()}, "bit" + n);
            if (i < 3)
            {
                Net& next = sim.addNet("en" + std::to_string(i + 1));
                sim.addGate(T::AND, {enable, q.back()}, {&next});
                enable = &next;
            }
        }

        std::ofstream file;
        std::unique_ptr<VcdWriter> vcd;
        if (vcd_path)
        {
            file.open(vcd_path);
            if (!file)
            {
                std::cerr << "cannot write " << vcd_path << '\n';
                return;
            }
            vcd = std::make_unique<VcdWriter>(file);
            vcd->attach(sim);
        }

        sim.scheduleInput(reset, LogicValue::LOW, 15);

        std::cout << "\n4-bit counter, clock period 20\n  time  q3..q0\n";
        for (uint64_t t = 5; t <= 365; t += 20)
        {
            if (sim.run(t) == Simulator::Result::Oscillation)
            {
                std::cout << "  oscillation at " << sim.now() << '\n';
                return;
            }
            std::cout << "  " << t << '\t';
            for (int i = 3; i >= 0; --i) std::cout << toChar(q[i]->getValue());
            std::cout << '\n';
        }

        if (vcd) std::cout << "waveform written to " << vcd_path << '\n';
    }
} // namespace

int main(int argc, char** argv)
{
    fullAdderTable();
    counter(argc > 1 ? argv[1] : nullptr);
    return 0;
}
