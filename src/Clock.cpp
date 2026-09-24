// File: Clock.cpp
#include "Clock.h"
#include <stdexcept>

Clock::Clock(Net* output,
             std::string name,
             uint64_t high_ticks,
             uint64_t low_ticks,
             LogicValue initial)
    : Gate({}, {output}, std::move(name)),
      high_ticks_(1),
      low_ticks_(1),
      initial_(initial)
{
    if (!isKnown(initial))
        throw std::invalid_argument("Clock: initial level must be LOW or HIGH");
    setPeriod(high_ticks, low_ticks);
}

void Clock::setPeriod(uint64_t high_ticks, uint64_t low_ticks)
{
    // A zero half-period would toggle forever inside one timestep.
    if (high_ticks == 0)
        throw std::invalid_argument("Clock: half-period must be at least 1 tick");
    high_ticks_ = high_ticks;
    low_ticks_ = low_ticks ? low_ticks : high_ticks;
}

void Clock::computeOutputs(std::vector<LogicValue>& out)
{
    // Once started, keep heading wherever the edge schedule says, so a
    // re-evaluation (e.g. after a wiring change) never disturbs the phase.
    out[0] = (target_[0] == LogicValue::HIGHZ) ? initial_ : target_[0];
}

void Clock::onCommitted(std::size_t pin, LogicValue v, uint64_t now, EventQueue& eq)
{
    if (!running_ || !isKnown(v)) return;
    const uint64_t hold = (v == LogicValue::HIGH) ? high_ticks_ : low_ticks_;
    scheduleOutput_(pin, ~v, now + hold, eq);
}

void Clock::setRunning(bool run, uint64_t now, EventQueue& eq)
{
    if (run == running_) return;
    running_ = run;

    if (!run)
    {
        cancelPending_(0);
        return;
    }

    const LogicValue level = current_[0];
    if (isKnown(level))
        onCommitted(0, level, now, eq);   // next edge one half-period from now
    else
        evaluate(now, eq);                // never started: start at initial
}
