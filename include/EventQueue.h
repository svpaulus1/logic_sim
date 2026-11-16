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
 * @enum Region
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
    Update = 1,   ///< State committing after every reader sampled: Q, Qbar.
    Monitor = 2,  ///< Read-only, once the tick settled: waveforms, assertions.
    Count = 3     ///< Not a region -- array sizing only.
};

/**
 * @class EventQueue
 * @brief Holds every scheduled gate-output change and fires them in time
 *        order.
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
 *
 * @code
 *   EventQueue eq;                       // 1024-slot wheel
 *   eq.schedule(5, Event{&gate, 0, LogicValue::HIGH, serial});
 *   eq.run(100);                         // fires it at t = 5; now() == 100
 * @endcode
 */
class EventQueue
{
public:
    /**
     * @brief Creates an empty queue at time 0.
     *
     * @param wheel_size Number of timesteps in the wheel; a power of two >= 2.
     *                   Events further ahead than this go in the overflow heap.
     *
     * @throws std::invalid_argument if @p wheel_size is not a power of two.
     */
    explicit EventQueue(std::size_t wheel_size = 1024);

    /// Not copyable: events point at gates owned by someone else.
    EventQueue(const EventQueue&) = delete;

    /// Not copyable: events point at gates owned by someone else.
    EventQueue& operator=(const EventQueue&) = delete;

    /// @brief The current simulation time.
    /// @return Ticks since the start, or since the last reset().
    uint64_t now() const { return now_; }

    /// @brief Tells whether any event is waiting.
    /// @return true if nothing is scheduled.
    bool empty() const { return wheel_pending_ == 0 && overflow_.empty(); }

    /// @brief Number of events waiting, including superseded ones not yet
    ///        dropped.
    /// @return Events in the wheel plus events in the overflow heap.
    std::size_t size() const { return wheel_pending_ + overflow_.size(); }

    /**
     * @brief Schedules an event at an absolute time.
     *
     * @param time   When the event fires; now() or later.
     * @param e      The event.
     * @param region Which pass of that timestep it fires in.
     *
     * @throws std::invalid_argument if @p time is before now().
     */
    void schedule(uint64_t time, Event e, Region region = Region::Active);

    /**
     * @brief Schedules zero-delay work in the current timestep, i.e. a delta
     *        cycle.
     *
     * @param e      The event.
     * @param region Which pass of the current timestep it fires in.
     */
    void scheduleNow(Event e, Region region = Region::Active)
    { schedule(now_, e, region); }

    /// @brief Time of the earliest pending event.
    /// @return That time, or std::nullopt if the queue is empty.
    std::optional<uint64_t> nextEventTime() const;

    /**
     * @brief Moves time forward to the earliest pending event, never past
     *        @p limit and never backwards.
     *
     * @param limit Latest time to move to; with nothing pending before it,
     *              time moves exactly to @p limit.
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
     * @param until_time Last time to process.
     * @return false if the delta-cycle guard tripped; now() is then the
     *         timestep that failed to settle.
     */
    bool run(uint64_t until_time);

    /// @brief Tells whether the last settle() / run() bailed out on an
    ///        oscillation.
    /// @return true if the delta-cycle guard tripped.
    bool oscillationDetected() const { return oscillated_; }

    /**
     * @brief The distinct gates with events still waiting at now().
     *
     * After an oscillation these are the gates caught in the loop, which is
     * what a GUI wants to highlight.
     *
     * @return Each gate once, in the order its first event was found.
     */
    std::vector<Gate*> pendingGatesNow() const;

    /// @brief Delta-cycle passes allowed in one timestep before declaring an
    ///        oscillation.
    /// @return The limit; 10000 by default.
    uint32_t maxDeltaCycles() const { return max_delta_cycles_; }

    /// @brief Sets the delta-cycle limit (see maxDeltaCycles()).
    /// @param n Passes allowed per timestep; 0 is treated as 1.
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
     * @param gate The gate whose events go.
     * @return How many events were removed.
     */
    std::size_t cancel(const Gate* gate);

private:
    /// One timestep of the wheel: a bucket of events per Region.
    struct TimeSlot
    {
        /// Events for this timestep, one bucket per Region.
        std::vector<Event> bucket[static_cast<std::size_t>(Region::Count)];

        /// @brief Tells whether every bucket is empty.
        /// @return true if this timestep has nothing scheduled.
        bool empty() const
        {
            for (const auto& b : bucket)
                if (!b.empty()) return false;
            return true;
        }
    };

    /// An event past the wheel's horizon, waiting in the overflow heap.
    struct FutureEvent
    {
        uint64_t time;  ///< When it fires.
        uint64_t seq;   ///< Scheduling order, breaks ties between equal times.
        Region region;  ///< Which pass of that timestep it fires in.
        Event e;        ///< The event itself.

        /**
         * @brief Heap ordering: earliest time first, then earliest scheduled.
         * @param o The event to compare with.
         * @return true if this event fires after @p o.
         */
        bool operator>(const FutureEvent& o) const
        {
            return time != o.time ? time > o.time : seq > o.seq;
        }
    };

    /// @brief Maps a time to its wheel slot (the wheel size is a power of two).
    /// @return wheel_.size() - 1.
    std::size_t mask() const { return wheel_.size() - 1; }

    /// Move any overflow events now inside the horizon into the wheel.
    void migrateOverflow_();

    std::vector<TimeSlot> wheel_;  ///< Circular array of near-term timesteps.

    /// Min-heap on (time, seq), kept with std::push_heap / std::pop_heap so
    /// cancel() can filter it.
    std::vector<FutureEvent> overflow_;
    uint64_t overflow_seq_ = 0;    ///< Next FutureEvent::seq to hand out.

    /// Reused across drain passes so the steady state is allocation-free.
    std::vector<Event> scratch_;

    uint64_t now_ = 0;                  ///< Current simulation time.
    std::size_t wheel_pending_ = 0;     ///< Events currently in the wheel.
    bool oscillated_ = false;           ///< Set when settle() gives up.
    uint32_t max_delta_cycles_ = 10000; ///< See maxDeltaCycles().
};

#endif
