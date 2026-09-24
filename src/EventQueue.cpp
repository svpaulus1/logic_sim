// File: EventQueue.cpp

#include "EventQueue.h"
#include <algorithm>
#include <cassert>
#include <functional>
#include <stdexcept>
#include "Gate.h"

EventQueue::EventQueue(std::size_t wheel_size)
    : wheel_(wheel_size)
{
    if (wheel_size < 2 || (wheel_size & (wheel_size - 1)) != 0)
        throw std::invalid_argument("EventQueue: wheel size must be a power of two >= 2");
}

void EventQueue::schedule(uint64_t time, Event e, Region region)
{
    // A hard check, not an assert: in a release build a past time would wrap
    // the distance below, land in the overflow heap, and later drag now_
    // backwards when the heap is drained.
    if (time < now_)
        throw std::invalid_argument("EventQueue: cannot schedule into the past");
    assert(region != Region::Count && "Count is not a region");
    assert(e.gate && "event without a gate");

    if (time - now_ < wheel_.size())
    {
        wheel_[time & mask()].bucket[static_cast<std::size_t>(region)].push_back(e);
        ++wheel_pending_;
    }
    else
    {
        overflow_.push_back(FutureEvent{time, overflow_seq_++, region, e});
        std::push_heap(overflow_.begin(), overflow_.end(), std::greater<FutureEvent>());
    }
}

void EventQueue::migrateOverflow_()
{
    while (!overflow_.empty() && overflow_.front().time - now_ < wheel_.size())
    {
        std::pop_heap(overflow_.begin(), overflow_.end(), std::greater<FutureEvent>());
        const FutureEvent& f = overflow_.back();
        wheel_[f.time & mask()].bucket[static_cast<std::size_t>(f.region)].push_back(f.e);
        ++wheel_pending_;
        overflow_.pop_back();
    }
}

std::optional<uint64_t> EventQueue::nextEventTime() const
{
    std::optional<uint64_t> best;

    if (wheel_pending_ != 0)
    {
        for (std::size_t d = 0; d < wheel_.size(); ++d)
        {
            if (!wheel_[(now_ + d) & mask()].empty())
            {
                best = now_ + d;
                break;
            }
        }
    }
    if (!overflow_.empty() && (!best || overflow_.front().time < *best))
        best = overflow_.front().time;

    return best;
}

bool EventQueue::advance(uint64_t limit)
{
    if (limit < now_) limit = now_;

    for (;;)
    {
        migrateOverflow_();

        if (!wheel_[now_ & mask()].empty()) return true;
        if (now_ >= limit) return false;

        if (wheel_pending_ == 0)
        {
            // Nothing in the wheel: jump straight over the idle gap, but never
            // past the caller's limit.
            now_ = overflow_.empty() ? limit
                                     : std::min(overflow_.front().time, limit);
            continue;
        }
        ++now_;
    }
}

bool EventQueue::settle()
{
    oscillated_ = false;
    migrateOverflow_();
    TimeSlot& slot = wheel_[now_ & mask()];
    uint32_t passes = 0;

    for (;;)
    {
        // Always work on the earliest non-empty region, so Monitor only ever
        // sees a timestep whose Active and Update work is completely done.
        std::vector<Event>* bucket = nullptr;
        for (auto& b : slot.bucket)
        {
            if (!b.empty())
            {
                bucket = &b;
                break;
            }
        }
        if (!bucket) break;

        // One guard across all regions: an Active <-> Update ping-pong is
        // just as much a loop as an Active-only one.
        if (++passes > max_delta_cycles_)
        {
            oscillated_ = true;
            return false;
        }

        // Swap rather than iterate in place, so handlers appending during the
        // pass land in a fresh bucket instead of mutating what we are walking.
        scratch_.clear();
        scratch_.swap(*bucket);
        wheel_pending_ -= scratch_.size();

        for (const Event& e : scratch_)
            e.gate->commit(e.out_index, e.value, e.serial, now_, *this);
    }
    return true;
}

bool EventQueue::run(uint64_t until_time)
{
    oscillated_ = false;
    if (until_time < now_) return true;

    for (;;)
    {
        if (advance(until_time) && !settle()) return false;
        if (now_ >= until_time) return true;
    }
}

std::vector<Gate*> EventQueue::pendingGatesNow() const
{
    std::vector<Gate*> gates;
    for (const auto& b : wheel_[now_ & mask()].bucket)
        for (const Event& e : b)
            if (std::find(gates.begin(), gates.end(), e.gate) == gates.end())
                gates.push_back(e.gate);
    return gates;
}

void EventQueue::clear()
{
    for (TimeSlot& slot : wheel_)
        for (auto& b : slot.bucket)
            b.clear();
    overflow_.clear();
    wheel_pending_ = 0;
    oscillated_ = false;
}

void EventQueue::reset()
{
    clear();
    now_ = 0;
    overflow_seq_ = 0;
}

std::size_t EventQueue::cancel(const Gate* gate)
{
    std::size_t removed = 0;
    auto aimed = [gate](const Event& e) { return e.gate == gate; };

    for (TimeSlot& slot : wheel_)
    {
        for (auto& b : slot.bucket)
        {
            const auto it = std::remove_if(b.begin(), b.end(), aimed);
            removed += static_cast<std::size_t>(b.end() - it);
            b.erase(it, b.end());
        }
    }
    wheel_pending_ -= removed;

    const auto it = std::remove_if(overflow_.begin(), overflow_.end(),
        [gate](const FutureEvent& f) { return f.e.gate == gate; });
    removed += static_cast<std::size_t>(overflow_.end() - it);
    overflow_.erase(it, overflow_.end());
    std::make_heap(overflow_.begin(), overflow_.end(), std::greater<FutureEvent>());

    return removed;
}
