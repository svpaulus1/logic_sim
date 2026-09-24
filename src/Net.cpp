// File: Net.cpp
#include "Net.h"
#include "Gate.h"
#include "EventQueue.h"
 
bool Net::resolve_()
{
    if (drivers_.empty()) return false;
 
    LogicValue resolved = LogicValue::HIGHZ;
 
    for (const auto& d : drivers_)
    {
        if (d.second == LogicValue::HIGHZ) continue;
 
        if (d.second == LogicValue::UNKNOWN)
        {
            resolved = LogicValue::UNKNOWN;
            break;
        }
 
        if (resolved == LogicValue::HIGHZ)
        {
            resolved = d.second;
        }
        else if (resolved != d.second)
        {
            resolved = LogicValue::UNKNOWN;
            break;
        }
    }
 
    if (resolved == value_) return false;
    value_ = resolved;
    return true;
}
 
void Net::wakeSinks_(uint64_t now, EventQueue& eq)
{
    for (Gate* sink : output_)
        sink->evaluate(now, eq);
}
 
void Net::driveFrom(std::size_t driver_index,
                    LogicValue v,
                    uint64_t now,
                    EventQueue& eq)
{
    assert(driver_index < drivers_.size() && "unregistered driver index");
 
    if (drivers_[driver_index].second == v) return;
    drivers_[driver_index].second = v;

    if (resolve_()) wakeSinks_(now, eq);
}
 
void Net::forceValue(LogicValue v, uint64_t now, EventQueue& eq)
{
    if (value_ == v) return;
    value_ = v;
    wakeSinks_(now, eq);
}
 
