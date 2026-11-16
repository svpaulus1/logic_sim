/**
 * @file Clock.h
 * @author Sebastian Paulus
 * @date 2026/09/24
 * @brief Free-running square-wave source.
 */

#ifndef CLOCK_H
#define CLOCK_H

#include <cstdint>
#include <string>
#include "Gate.h"
#include "Types.h"

/**
 * @class Clock
 * @brief Drives one net with a periodic square wave.
 *
 * Pin layout:
 *  - inputs:  none
 *  - outputs: {CLK}
 *
 * Behaviour:
 *  - Starts at its initial level at time 0 (or when the simulation
 *    initialises) and toggles after staying HIGH for highTicks() and LOW for
 *    lowTicks().
 *  - Each edge schedules the next one, so the queue always holds exactly one
 *    clock event.
 *  - setRunning(false) holds the current level; resuming schedules the next
 *    edge one half-period later.
 *  - Runs in simulated time and has no delay of its own. For a clock that a
 *    GUI ticks in real time, use an Input instead.
 *
 * @code
 *   // 10 ticks HIGH, 10 ticks LOW: rising edges at 10, 30, 50, ...
 *   Clock& clk = sim.addClock(&clk_net, "clk", 10);
 * @endcode
 */
class Clock : public Gate
{
public:
    /**
     * @brief Builds the clock and wires it into the netlist.
     *
     * @param output     Net to drive; nullptr leaves it unconnected.
     * @param name       Descriptive name (diagnostics, GUI, VCD output).
     * @param high_ticks Ticks spent HIGH per period (>= 1).
     * @param low_ticks  Ticks spent LOW per period (>= 1); 0 = same as high.
     * @param initial    Level at time 0, LOW or HIGH.
     *
     * @throws std::invalid_argument if @p high_ticks is 0 or @p initial is not
     *         LOW or HIGH.
     */
    Clock(Net* output,
          std::string name,
          uint64_t high_ticks,
          uint64_t low_ticks = 0,
          LogicValue initial = LogicValue::LOW);

    /// @brief Ticks spent HIGH per period.
    /// @return At least 1.
    uint64_t highTicks() const { return high_ticks_; }

    /// @brief Ticks spent LOW per period.
    /// @return At least 1.
    uint64_t lowTicks() const { return low_ticks_; }

    /// @brief Length of one full period.
    /// @return highTicks() + lowTicks().
    uint64_t period() const { return high_ticks_ + low_ticks_; }

    /**
     * @brief New timing, used from the next edge on.
     *
     * @param high_ticks Ticks spent HIGH per period (>= 1).
     * @param low_ticks  Ticks spent LOW per period; 0 = same as high.
     *
     * @throws std::invalid_argument if @p high_ticks is 0.
     */
    void setPeriod(uint64_t high_ticks, uint64_t low_ticks = 0);

    /// @brief Tells whether the clock is toggling.
    /// @return false while paused by setRunning().
    bool running() const { return running_; }

    /**
     * @brief Pauses (holding the current level) or resumes the clock at
     *        @p now. Used by Simulator::setClockRunning().
     *
     * @param run true to resume, false to pause.
     * @param now Current simulation time.
     * @param eq  Queue to schedule the next edge into.
     */
    void setRunning(bool run, uint64_t now, EventQueue& eq);

    /// @brief A source has no logic stages.
    /// @return 0.
    uint32_t intrinsicStages() const override { return 0; }

    /// @brief Type name used in diagnostics, generated gate names and the GUI.
    /// @return "clock".
    const char* typeName() const override { return "clock"; }

    /// @brief A source has no propagation delay of its own.
    /// @return false.
    bool usesTimingModel() const override { return false; }

    /**
     * @brief Sets the output to the level the edge schedule is heading for
     *        (the initial level before the first edge).
     * @param out {CLK}.
     */
    void computeOutputs(std::vector<LogicValue>& out) override;

protected:
    /**
     * @brief Schedules the next edge after each committed one.
     *
     * @param pin Output pin that changed.
     * @param v   Level it now drives.
     * @param now Current simulation time.
     * @param eq  Queue to schedule the next edge into.
     */
    void onCommitted(std::size_t pin,
                     LogicValue v,
                     uint64_t now,
                     EventQueue& eq) override;

private:
    uint64_t high_ticks_; ///< Ticks spent HIGH per period.
    uint64_t low_ticks_;  ///< Ticks spent LOW per period.
    LogicValue initial_;  ///< Level at time 0.
    bool running_ = true; ///< false while paused.
};

#endif
