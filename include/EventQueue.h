/**
 * @file EventQueue.h
 * @author Sebastian Paulus
 * @date 2026/09/13
 * @brief Two-tier event queue: timing wheel plus far-future overflow heap.
 */

#ifndef EVENTQUEUE_H
#define EVENTQUEUE_H

#include <cassert>
#include <cstdint>
#include <functional>
#include <queue>
#include <vector>
#include "Event.h"
#include "Types.h"

class Gate;

/**
 * @brief Which pass within a timestep an event belongs to.
 *
 * Drained in declaration order; each region is drained to empty (including
 * anything appended while draining) before the next begins.
 */
enum class Region : uint8_t
{
    Active = 0,   ///< Value propagating forward: gate outputs, clock, stimulus.
    Update = 1,   ///< State committing after every reader sampled: Q, Qbar
    Monitor = 2,  ///< Read-only, once the tick settled: waveforms, assertions.
    Count = 3     ///< Not a region -- array sizing only.
};

/**
 * @brief Event queue.
 *
 * Near-term events go in a circular array of timesteps (O(1) insert and pop).
 * Events past the wheel's horizon go in a min-heap and migrate in as time
 * advances, so scheduling distance is unbounded and long idle gaps are skipped
 * in one jump rather than ticked through one at a time.
 *
 * Total path delay never interacts with the horizon: a gate always schedules at
 * now + its own delay, so a 10,000-stage chain of delay-5 gates never puts
 * anything more than 5 ticks ahead of the current time.
 */
class EventQueue
{
public:
    EventQueue(std::size_t wheel_size = 1024)
        : wheel_(wheel_size)
    {
        assert(wheel_size >= 2 && (wheel_size & (wheel_size - 1)) == 0 &&
               "wheel size must be a power of two");
    }
 
    uint64_t now() const { return now_; }
    bool empty() const { return wheel_pending_ == 0 && overflow_.empty(); }
 
    /// Schedule @p e to fire at absolute time @p time in @p region.
    void schedule(uint64_t time, Event e, Region region = Region::Active);
 
    /// Zero-delay work in the current timestep -- i.e. a delta cycle.
    void scheduleNow(Event e, Region region = Region::Active)
    { schedule(now_, e, region); }
 
    /**
     * @brief Run until the queue drains or time passes @p until_time.
     * @return false if the delta-cycle guard tripped (combinational loop).
     */
    bool run(uint64_t until_time);
 
    /// True if the last run() bailed out on an oscillation.
    bool oscillationDetected() const { return oscillated_; }
 
private:
    struct TimeSlot
    {
        std::vector<Event> bucket[static_cast<std::size_t>(Region::Count)];
 
        bool empty() const
        {
            for (const auto& b : bucket)
                if (!b.empty()) return false;
            return true;
        }
    };
 
    struct FutureEvent
    {
        uint64_t time;
        Region region;
        Event e;
        bool operator>(const FutureEvent& o) const { return time > o.time; }
    };
 
    std::size_t mask() const { return wheel_.size() - 1; }
 
    /// Move any overflow events now inside the horizon into the wheel.
    void migrateOverflow_();
 
    /// Drain one region to exhaustion, re-checking because handlers append.
    bool drainRegion_(TimeSlot& slot, Region region);
 
    std::vector<TimeSlot> wheel_;
    std::priority_queue<FutureEvent,
                        std::vector<FutureEvent>,
                        std::greater<FutureEvent>> overflow_;
 
    /// Reused across drain passes so the steady state is allocation-free.
    std::vector<Event> scratch_;
 
    uint64_t now_ = 0;
    std::size_t wheel_pending_ = 0;
    bool oscillated_ = false;
 
    static constexpr uint32_t kMaxDeltaCycles = 10000;
};

#endif
