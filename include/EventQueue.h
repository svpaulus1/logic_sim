/**
 * @file EventQueue.h
 * @author Sebastian Paulus
 * @date 2026/09/13
 * @brief Two-tier event queue: timing wheel plus far-future overflow heap.
 */

#ifndef EVENTQUEUE_H
#define EVENTQUEUE_H

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>
#include "Event.h"
#include "Types.h"

class Gate;

/**
 * @brief Which pass within a timestep an event belongs to.
 *
 * Within one timestep the queue always works on the first non-empty region in
 * declaration order, so a later region only runs once every earlier region is
 * empty (including anything appended while draining), and any work a later
 * region creates in an earlier one is finished before moving on again.
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
 *
 * Events for the same time and region fire in the order they were scheduled,
 * whether they went through the wheel or the overflow heap, so a run is fully
 * deterministic.
 *
 * @note Not re-entrant: event handlers may schedule() but must not call
 *       settle(), run(), clear(), reset() or cancel().
 */
class EventQueue
{
public:
    /**
     * @param wheel_size Number of timesteps in the wheel; a power of two >= 2.
     * @throws std::invalid_argument if @p wheel_size is not a power of two.
     */
    explicit EventQueue(std::size_t wheel_size = 1024);

    EventQueue(const EventQueue&) = delete;
    EventQueue& operator=(const EventQueue&) = delete;

    /// The current simulation time.
    uint64_t now() const { return now_; }

    bool empty() const { return wheel_pending_ == 0 && overflow_.empty(); }

    /// Number of events waiting, including superseded ones not yet dropped.
    std::size_t size() const { return wheel_pending_ + overflow_.size(); }

    /**
     * @brief Schedule @p e to fire at absolute time @p time in @p region.
     * @throws std::invalid_argument if @p time is before now().
     */
    void schedule(uint64_t time, Event e, Region region = Region::Active);

    /// Zero-delay work in the current timestep -- i.e. a delta cycle.
    void scheduleNow(Event e, Region region = Region::Active)
    { schedule(now_, e, region); }

    /// Time of the earliest pending event, or std::nullopt if none.
    std::optional<uint64_t> nextEventTime() const;

    /**
     * @brief Moves time forward to the earliest pending event, never past
     *        @p limit and never backwards.
     * @return true if events are waiting at the (possibly new) now().
     */
    bool advance(uint64_t limit);

    /**
     * @brief Processes every event at now(), across all regions and delta
     *        cycles, until the timestep is quiet. Time does not move.
     * @return false if the delta-cycle guard tripped (combinational loop);
     *         the unprocessed events stay queued.
     */
    bool settle();

    /**
     * @brief Processes every event up to and including @p until_time.
     *
     * On success now() == until_time afterwards (or is unchanged if
     * @p until_time is already in the past), so the caller can schedule
     * stimulus "now" and have it land exactly where expected.
     *
     * @return false if the delta-cycle guard tripped; now() is then the
     *         timestep that failed to settle.
     */
    bool run(uint64_t until_time);

    /// True if the last settle() / run() bailed out on an oscillation.
    bool oscillationDetected() const { return oscillated_; }

    /**
     * @brief The distinct gates with events still waiting at now().
     *
     * After an oscillation these are the gates caught in the loop, which is
     * what a GUI wants to highlight.
     */
    std::vector<Gate*> pendingGatesNow() const;

    /// Delta-cycle passes allowed in one timestep before declaring oscillation.
    uint32_t maxDeltaCycles() const { return max_delta_cycles_; }
    void setMaxDeltaCycles(uint32_t n) { max_delta_cycles_ = n ? n : 1; }

    /// Drops every pending event. Time is unchanged.
    void clear();

    /// Drops every pending event and rewinds time to zero.
    void reset();

    /**
     * @brief Drops every pending event aimed at @p gate.
     *
     * Must be called before a gate is destroyed, or the queue would later
     * call into freed memory.
     *
     * @return How many events were removed.
     */
    std::size_t cancel(const Gate* gate);

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
        uint64_t seq;   ///< Scheduling order, breaks ties between equal times.
        Region region;
        Event e;

        /// Heap ordering: earliest time first, then earliest scheduled.
        bool operator>(const FutureEvent& o) const
        {
            return time != o.time ? time > o.time : seq > o.seq;
        }
    };

    std::size_t mask() const { return wheel_.size() - 1; }

    /// Move any overflow events now inside the horizon into the wheel.
    void migrateOverflow_();

    std::vector<TimeSlot> wheel_;

    /// Min-heap on (time, seq), kept with std::push_heap / std::pop_heap so
    /// cancel() can filter it.
    std::vector<FutureEvent> overflow_;
    uint64_t overflow_seq_ = 0;

    /// Reused across drain passes so the steady state is allocation-free.
    std::vector<Event> scratch_;

    uint64_t now_ = 0;
    std::size_t wheel_pending_ = 0;
    bool oscillated_ = false;
    uint32_t max_delta_cycles_ = 10000;
};

#endif
