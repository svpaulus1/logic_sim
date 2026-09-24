#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "EventQueue.h"
#include "Gate.h"
#include "TestFramework.h"

namespace
{
    /// Gate with no logic whose commits run a callback. Events scheduled by
    /// hand with serial 0 always match (the gate never bumps its serial).
    struct Hook : Gate
    {
        std::function<void(LogicValue, uint64_t, EventQueue&)> on_commit;

        explicit Hook(Net* out) : Gate({}, {out}, "hook") {}
        void computeOutputs(std::vector<LogicValue>& out) override { out[0] = LogicValue::HIGHZ; }
        uint32_t intrinsicStages() const override { return 0; }
        const char* typeName() const override { return "hook"; }

    protected:
        void onCommitted(std::size_t, LogicValue v, uint64_t now, EventQueue& eq) override
        {
            if (on_commit) on_commit(v, now, eq);
        }
    };

    Event ev(Gate& g, LogicValue v = LogicValue::HIGH)
    {
        return Event{&g, 0, v, 0};
    }
} // namespace

TEST(queue_rejects_bad_wheel_size)
{
    CHECK_THROWS(EventQueue(0));
    CHECK_THROWS(EventQueue(1));
    CHECK_THROWS(EventQueue(1000));
    EventQueue ok(2);
    CHECK(ok.empty());
}

TEST(queue_fires_in_time_order_across_wheel_and_overflow)
{
    EventQueue eq(8);
    Net n;
    Hook g(&n);
    std::vector<uint64_t> fired;
    g.on_commit = [&](LogicValue, uint64_t now, EventQueue&) { fired.push_back(now); };

    for (uint64_t t : {500u, 3u, 7u, 100u, 0u, 8u, 9u})
        eq.schedule(t, ev(g));
    CHECK_EQ(eq.size(), std::size_t{7});
    CHECK(eq.nextEventTime() == uint64_t{0});

    CHECK(eq.run(1000));
    CHECK_EQ(fired, (std::vector<uint64_t>{0, 3, 7, 8, 9, 100, 500}));
    CHECK(eq.empty());
}

TEST(queue_run_leaves_now_at_until_time)
{
    EventQueue eq(8);
    Net n;
    Hook g(&n);
    int fired = 0;
    g.on_commit = [&](LogicValue, uint64_t, EventQueue&) { ++fired; };

    // Queue drains early: time still moves to the requested point.
    eq.schedule(2, ev(g));
    CHECK(eq.run(10));
    CHECK_EQ(eq.now(), uint64_t{10});

    // Idle jump must not overshoot a limit that falls inside the gap.
    eq.schedule(5000, ev(g));
    CHECK(eq.run(100));
    CHECK_EQ(eq.now(), uint64_t{100});
    CHECK_EQ(fired, 1);

    CHECK(eq.run(10000));
    CHECK_EQ(eq.now(), uint64_t{10000});
    CHECK_EQ(fired, 2);

    // Running to the past is a no-op.
    CHECK(eq.run(50));
    CHECK_EQ(eq.now(), uint64_t{10000});
}

TEST(queue_rejects_scheduling_into_the_past)
{
    EventQueue eq(8);
    Net n;
    Hook g(&n);
    eq.run(20);
    CHECK_THROWS(eq.schedule(19, ev(g)));
    eq.schedule(20, ev(g));   // "now" is fine
    CHECK_EQ(eq.size(), std::size_t{1});
}

TEST(queue_same_time_events_fire_in_schedule_order)
{
    // Far-future events travel through the heap; they must still keep order,
    // and come before anything scheduled for that time after they migrated.
    EventQueue eq(4);
    Net n;
    std::vector<int> order;
    std::vector<std::unique_ptr<Hook>> hooks;
    for (int i = 0; i < 20; ++i)
    {
        hooks.push_back(std::make_unique<Hook>(&n));
        hooks.back()->on_commit = [&order, i](LogicValue, uint64_t, EventQueue&) { order.push_back(i); };
    }
    for (int i = 0; i < 10; ++i)
        eq.schedule(1000, ev(*hooks[i]));

    eq.run(998);                        // 1000 is now inside the wheel
    for (int i = 10; i < 20; ++i)
        eq.schedule(1000, ev(*hooks[i]));
    eq.run(1000);

    std::vector<int> want;
    for (int i = 0; i < 20; ++i) want.push_back(i);
    CHECK_EQ(order.size(), want.size());
    CHECK(order == want);
}

TEST(queue_regions_drain_in_order_and_monitor_waits_for_everything)
{
    EventQueue eq(8);
    Net n;
    Hook active(&n), update(&n), monitor(&n), late(&n);
    std::vector<std::string> log;

    active.on_commit = [&](LogicValue, uint64_t, EventQueue&) { log.push_back("active"); };
    monitor.on_commit = [&](LogicValue, uint64_t, EventQueue&) { log.push_back("monitor"); };
    late.on_commit = [&](LogicValue, uint64_t, EventQueue&) { log.push_back("late-active"); };
    update.on_commit = [&](LogicValue, uint64_t now, EventQueue& q)
    {
        log.push_back("update");
        q.schedule(now, ev(late), Region::Active);   // new Active work
    };

    eq.schedule(3, ev(monitor), Region::Monitor);
    eq.schedule(3, ev(update), Region::Update);
    eq.schedule(3, ev(active), Region::Active);
    CHECK(eq.run(3));

    CHECK(log == (std::vector<std::string>{"active", "update", "late-active", "monitor"}));
}

TEST(queue_detects_active_update_ping_pong)
{
    EventQueue eq(8);
    eq.setMaxDeltaCycles(50);
    Net n;
    Hook a(&n), b(&n);
    a.on_commit = [&](LogicValue, uint64_t now, EventQueue& q) { q.schedule(now, ev(b), Region::Update); };
    b.on_commit = [&](LogicValue, uint64_t now, EventQueue& q) { q.schedule(now, ev(a), Region::Active); };

    eq.schedule(5, ev(a));
    CHECK(!eq.run(100));
    CHECK(eq.oscillationDetected());
    CHECK_EQ(eq.now(), uint64_t{5});

    const std::vector<Gate*> stuck = eq.pendingGatesNow();
    CHECK_EQ(stuck.size(), std::size_t{1});

    // Breaking the loop lets the same timestep finish.
    b.on_commit = nullptr;
    CHECK(eq.run(100));
    CHECK(!eq.oscillationDetected());
    CHECK_EQ(eq.now(), uint64_t{100});
}

TEST(queue_cancel_clear_reset)
{
    EventQueue eq(8);
    Net n;
    Hook a(&n), b(&n);
    int fired_a = 0, fired_b = 0;
    a.on_commit = [&](LogicValue, uint64_t, EventQueue&) { ++fired_a; };
    b.on_commit = [&](LogicValue, uint64_t, EventQueue&) { ++fired_b; };

    eq.schedule(1, ev(a));
    eq.schedule(2, ev(b));
    eq.schedule(900, ev(a));    // overflow
    eq.schedule(901, ev(b));

    CHECK_EQ(eq.cancel(&a), std::size_t{2});
    CHECK_EQ(eq.size(), std::size_t{2});
    eq.run(1000);
    CHECK_EQ(fired_a, 0);
    CHECK_EQ(fired_b, 2);

    eq.schedule(1005, ev(a));
    eq.schedule(5000, ev(a));
    eq.clear();
    CHECK(eq.empty());
    CHECK_EQ(eq.now(), uint64_t{1000});

    eq.reset();
    CHECK_EQ(eq.now(), uint64_t{0});
    CHECK(!eq.nextEventTime().has_value());
}

TEST(queue_advance_and_next_event_time)
{
    EventQueue eq(8);
    Net n;
    Hook g(&n);
    eq.schedule(4, ev(g));
    eq.schedule(40, ev(g));

    CHECK(eq.nextEventTime() == uint64_t{4});
    CHECK(!eq.advance(3));             // stops at the limit, nothing there
    CHECK_EQ(eq.now(), uint64_t{3});
    CHECK(eq.advance(100));            // lands on the event
    CHECK_EQ(eq.now(), uint64_t{4});
    CHECK(eq.settle());
    CHECK(eq.nextEventTime() == uint64_t{40});
}
