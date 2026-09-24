#include <sstream>
#include <string>
#include "Simulator.h"
#include "TestFramework.h"
#include "VcdWriter.h"

TEST(vcd_header_scopes_and_changes)
{
    Simulator sim;
    Net& a = sim.addNet("a");
    Net& y = sim.addNet("u1.inner y");        // whitespace must be sanitised
    Input& in = sim.addInput(&a, "in", LogicValue::LOW);
    sim.addGate(BuiltinGate::Type::NOT, {&a}, {&y});

    std::ostringstream out;
    {
        VcdWriter vcd(out, "1ns");
        vcd.attach(sim);
        sim.run(5);
        sim.setInput(in, LogicValue::HIGH);
        sim.run(10);
    }   // detaches and flushes

    const std::string text = out.str();
    auto has = [&](const std::string& s) { return text.find(s) != std::string::npos; };

    CHECK(has("$timescale 1ns $end"));
    CHECK(has("$scope module top $end"));
    CHECK(has("$scope module u1 $end"));
    CHECK(has("$var wire 1 ! a $end"));
    CHECK(has("$var wire 1 \" inner_y $end"));
    CHECK(has("$enddefinitions $end"));
    CHECK(has("$dumpvars"));
    CHECK(has("#0\n"));
    CHECK(has("#1\n1\"\n"));                  // y = NOT a settles after 1 tick
    CHECK(has("#5\n1!\n"));                   // a rises at 5
    CHECK(has("#6\n0\"\n"));                  // y falls one tick later
}

TEST(vcd_stops_after_detach)
{
    Simulator sim;
    Net& a = sim.addNet("a");
    Input& in = sim.addInput(&a);
    std::ostringstream out;
    VcdWriter vcd(out);
    vcd.attach(sim);
    sim.run(1);
    vcd.detach();
    const std::size_t len = out.str().size();
    sim.setInput(in, LogicValue::HIGH);
    sim.run(10);
    CHECK_EQ(out.str().size(), len);
}
