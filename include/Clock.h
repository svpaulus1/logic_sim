/**
 * @file Clock.h
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
 * Starts at @p initial at time 0 (or when the simulation initialises) and
 * toggles after staying HIGH for high_ticks / LOW for low_ticks. Each edge
 * schedules the next one, so the queue always holds exactly one clock event.
 */
class Clock : public Gate
{
public:
    /**
     * @param high_ticks Ticks spent HIGH per period (>= 1).
     * @param low_ticks  Ticks spent LOW per period (>= 1); 0 means same as high.
     * @throws std::invalid_argument if @p high_ticks is 0 or @p initial is not
     *         LOW or HIGH.
     */
    Clock(Net* output,
          std::string name,
          uint64_t high_ticks,
          uint64_t low_ticks = 0,
          LogicValue initial = LogicValue::LOW);

    uint64_t highTicks() const { return high_ticks_; }
    uint64_t lowTicks() const { return low_ticks_; }
    uint64_t period() const { return high_ticks_ + low_ticks_; }

    /// New timing, used from the next edge on.
    /// @throws std::invalid_argument if @p high_ticks is 0.
    void setPeriod(uint64_t high_ticks, uint64_t low_ticks = 0);

    bool running() const { return running_; }

    /**
     * @brief Pauses (holding the current level) or resumes the clock at
     *        @p now. Used by Simulator::setClockRunning().
     */
    void setRunning(bool run, uint64_t now, EventQueue& eq);

    uint32_t intrinsicStages() const override { return 0; }
    const char* typeName() const override { return "clock"; }
    bool usesTimingModel() const override { return false; }
    void computeOutputs(std::vector<LogicValue>& out) override;

protected:
    void onCommitted(std::size_t pin, LogicValue v, uint64_t now, EventQueue& eq) override;

private:
    uint64_t high_ticks_;
    uint64_t low_ticks_;
    LogicValue initial_;
    bool running_ = true;
};

#endif
