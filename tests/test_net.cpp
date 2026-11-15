#include "BuiltinGate.h"
#include "EventQueue.h"
#include "Net.h"
#include "TestFramework.h"

namespace
{
    constexpr LogicValue L = LogicValue::LOW;
    constexpr LogicValue H = LogicValue::HIGH;
    constexpr LogicValue X = LogicValue::UNKNOWN;
    constexpr LogicValue Z = LogicValue::HIGHZ;

    /// A gate that does nothing, used only as a driver identity.
    struct Dummy : Gate
    {
        Dummy() : Gate({}, {}, "dummy") {}
        void computeOutputs(std::vector<LogicValue>&) override {}
        uint32_t intrinsicStages() const override { return 0; }
        const char* typeName() const override { return "dummy"; }
    };
} // namespace

TEST(net_resolution_rules)
{
    EventQueue eq;
    Net n("bus");
    Dummy g1, g2, g3;
    const std::size_t a = n.addDriver(&g1);
    const std::size_t b = n.addDriver(&g2);
    const std::size_t pull = n.addDriver(&g3, Strength::Pull);
    CHECK_EQ(n.driverCount(), std::size_t{3});

    n.refresh(0, eq);
    CHECK_EQ(n.getValue(), Z);                 // nothing driving

    n.driveFrom(pull, H, 0, eq);
    CHECK_EQ(n.getValue(), H);                 // pullup alone wins

    n.driveFrom(a, L, 0, eq);
    CHECK_EQ(n.getValue(), L);                 // strong beats pull
    CHECK(!n.hasConflict());

    n.driveFrom(b, H, 0, eq);
    CHECK_EQ(n.getValue(), X);                 // two strong disagree
    CHECK(n.hasConflict());

    n.driveFrom(b, L, 0, eq);
    CHECK_EQ(n.getValue(), L);                 // agree again

    n.driveFrom(a, X, 0, eq);
    CHECK_EQ(n.getValue(), X);                 // any strong X poisons it

    n.driveFrom(a, Z, 0, eq);
    n.driveFrom(b, Z, 0, eq);
    CHECK_EQ(n.getValue(), H);                 // released: pullup again
}

TEST(net_driver_slots_are_reused_and_stable)
{
    EventQueue eq;
    Net n;
    Dummy g1, g2, g3;
    const std::size_t a = n.addDriver(&g1);
    const std::size_t b = n.addDriver(&g2);
    n.driveFrom(b, H, 0, eq);

    n.removeDriver(a);
    CHECK_EQ(n.driverCount(), std::size_t{1});
    CHECK_EQ(n.driverValue(b), H);             // b's index still valid

    const std::size_t c = n.addDriver(&g3);
    CHECK_EQ(c, a);                            // freed slot reused
    CHECK_EQ(n.driverValue(c), Z);
}

TEST(net_force_holds_until_release)
{
    EventQueue eq;
    Net in("in"), out("out");
    BuiltinGate buf(BuiltinGate::Type::BUF, {&in}, {&out}, "buf");
    Dummy src;
    const std::size_t d = in.addDriver(&src);

    in.driveFrom(d, H, 0, eq);
    eq.run(5);
    CHECK_EQ(out.getValue(), H);

    in.forceValue(L, eq.now(), eq);
    CHECK(in.isForced());
    eq.run(10);
    CHECK_EQ(out.getValue(), L);

    in.driveFrom(d, X, eq.now(), eq);          // drivers are ignored while forced
    eq.run(15);
    CHECK_EQ(in.getValue(), L);
    CHECK_EQ(out.getValue(), L);

    in.release(eq.now(), eq);                  // back to the driver (X)
    eq.run(20);
    CHECK_EQ(in.getValue(), X);
    CHECK_EQ(out.getValue(), X);
}

TEST(net_sinks_track_gate_lifetime)
{
    Net a, b, y;
    {
        BuiltinGate g(BuiltinGate::Type::AND, {&a, &a}, {&y}, "g");
        CHECK_EQ(a.sinkCount(), std::size_t{2});   // one per pin
        CHECK_EQ(y.driverCount(), std::size_t{1});

        g.connectInput(1, &b);
        CHECK_EQ(a.sinkCount(), std::size_t{1});
        CHECK_EQ(b.sinkCount(), std::size_t{1});
    }
    // Destroying the gate unregisters it everywhere.
    CHECK_EQ(a.sinkCount(), std::size_t{0});
    CHECK_EQ(b.sinkCount(), std::size_t{0});
    CHECK_EQ(y.driverCount(), std::size_t{0});
}
