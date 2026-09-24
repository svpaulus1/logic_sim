// File: Net.cpp
#include "Net.h"
#include <algorithm>
#include "Gate.h"
#include "EventQueue.h"

bool Net::resolve_()
{
    LogicValue resolved = LogicValue::HIGHZ;

    if (forced_)
    {
        resolved = forced_value_;
    }
    else
    {
        Strength best = Strength::Pull;
        bool any = false;

        for (const Driver& d : drivers_)
        {
            if (!d.gate || d.value == LogicValue::HIGHZ) continue;

            if (!any || d.strength > best)
            {
                // First driver seen, or one that overpowers everything so far.
                any = true;
                best = d.strength;
                resolved = d.value;
            }
            else if (d.strength == best && d.value != resolved)
            {
                resolved = LogicValue::UNKNOWN;
            }
        }
    }

    if (resolved == value_) return false;
    value_ = resolved;

    if (change_log_ && !logged_)
    {
        logged_ = true;
        change_log_->push_back(this);
    }
    return true;
}

void Net::wakeSinks_(uint64_t now, EventQueue& eq)
{
    for (Gate* sink : sinks_)
        sink->evaluate(now, eq);
}

bool Net::hasConflict() const
{
    Strength best = Strength::Pull;
    bool any = false;
    bool seen_low = false;
    bool seen_high = false;

    for (const Driver& d : drivers_)
    {
        if (!d.gate || d.value == LogicValue::HIGHZ) continue;
        if (!any || d.strength > best)
        {
            any = true;
            best = d.strength;
            seen_low = seen_high = false;
        }
        if (d.strength != best) continue;
        if (d.value == LogicValue::LOW)  seen_low = true;
        if (d.value == LogicValue::HIGH) seen_high = true;
    }
    return seen_low && seen_high;
}

void Net::removeSink(Gate* gate)
{
    const auto it = std::find(sinks_.begin(), sinks_.end(), gate);
    assert(it != sinks_.end() && "gate is not a sink of this net");
    if (it != sinks_.end()) sinks_.erase(it);
}

std::size_t Net::addDriver(Gate* gate, Strength strength, LogicValue initial)
{
    assert(gate && "null driver");
    ++active_drivers_;

    if (!free_slots_.empty())
    {
        const std::size_t i = free_slots_.back();
        free_slots_.pop_back();
        drivers_[i] = Driver{gate, initial, strength};
        return i;
    }
    drivers_.push_back(Driver{gate, initial, strength});
    return drivers_.size() - 1;
}

void Net::removeDriver(std::size_t driver_index)
{
    assert(driver_index < drivers_.size() && drivers_[driver_index].gate &&
           "unregistered driver index");
    drivers_[driver_index] = Driver{};
    free_slots_.push_back(driver_index);
    --active_drivers_;
}

void Net::driveFrom(std::size_t driver_index,
                    LogicValue v,
                    uint64_t now,
                    EventQueue& eq)
{
    assert(driver_index < drivers_.size() && "unregistered driver index");

    if (drivers_[driver_index].value == v) return;
    drivers_[driver_index].value = v;

    if (resolve_()) wakeSinks_(now, eq);
}

void Net::refresh(uint64_t now, EventQueue& eq)
{
    if (resolve_()) wakeSinks_(now, eq);
}

void Net::forceValue(LogicValue v, uint64_t now, EventQueue& eq)
{
    forced_ = true;
    forced_value_ = v;
    refresh(now, eq);
}

void Net::release(uint64_t now, EventQueue& eq)
{
    if (!forced_) return;
    forced_ = false;
    refresh(now, eq);
}

void Net::resetState()
{
    for (Driver& d : drivers_) d.value = LogicValue::HIGHZ;
    forced_ = false;
    value_ = LogicValue::HIGHZ;
    reported_ = LogicValue::HIGHZ;
    logged_ = false;
}
