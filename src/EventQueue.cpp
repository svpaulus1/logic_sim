// File: EventQueue.cpp

#include "EventQueue.h"
#include "Gate.h"
 
void EventQueue::schedule(uint64_t time, Event e, Region region)
{
    assert(time >= now_ && "cannot schedule into the past");
    assert(region != Region::Count && "Count is not a region");
 
    if (time - now_ < wheel_.size())
    {
        wheel_[time & mask()].bucket[static_cast<std::size_t>(region)].push_back(e);
        ++wheel_pending_;
    }
    else
    {
        overflow_.push(FutureEvent{time, region, e});
    }
}
 
void EventQueue::migrateOverflow_()
{
    while (!overflow_.empty() && overflow_.top().time - now_ < wheel_.size())
    {
        const FutureEvent& f = overflow_.top();
        wheel_[f.time & mask()].bucket[static_cast<std::size_t>(f.region)].push_back(f.e);
        ++wheel_pending_;
        overflow_.pop();
    }
}
 
bool EventQueue::drainRegion_(TimeSlot& slot, Region region)
{
    auto& bucket = slot.bucket[static_cast<std::size_t>(region)];
 
    for (uint32_t pass = 0; !bucket.empty(); ++pass)
    {
        if (pass >= kMaxDeltaCycles)
        {
            oscillated_ = true;
            return false;
        }
 
        // Swap rather than iterate in place, so handlers appending during the
        // pass land in a fresh bucket instead of mutating what we are walking.
        scratch_.clear();
        scratch_.swap(bucket);
 
        for (const Event& e : scratch_)
        {
            --wheel_pending_;
            e.gate->commit(e.out_index, e.value, e.serial, now_, *this);
        }
    }
    return true;
}

bool EventQueue::run(uint64_t until_time)
{
    oscillated_ = false;
 
    while (now_ <= until_time)
    {
        migrateOverflow_();
 
        if (wheel_pending_ == 0)
        {
            if (overflow_.empty()) break;
            now_ = overflow_.top().time;
            continue;
        }
 
        TimeSlot& slot = wheel_[now_ & mask()];
        if (slot.empty())
        {
            ++now_;
            continue;
        }
 
        while (!slot.empty())
        {
            if (!drainRegion_(slot, Region::Active))  return false;
            if (!drainRegion_(slot, Region::Update))  return false;
            if (!drainRegion_(slot, Region::Monitor)) return false;
        }
 
        ++now_;
    }
    return true;
}
