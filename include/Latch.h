/**
 * @file Latch.h
 * @date 2026/09/24
 * @brief Level-sensitive (gated) D latch.
 */

#ifndef LATCH_H
#define LATCH_H

#include <string>
#include <vector>
#include "Gate.h"
#include "Types.h"

/**
 * @class Latch
 * @brief Gated D latch: transparent while EN is at its active level.
 *
 * Inputs {D, EN}, outputs {Q, QN}; trailing pins may be omitted.
 *  - EN active:   Q follows D.
 *  - EN inactive or floating (HIGHZ): Q holds.
 *  - EN UNKNOWN:  Q becomes UNKNOWN unless D already equals Q.
 *
 * Like FlipFlop, outputs commit in the Update region.
 */
class Latch : public Gate
{
public:
    /**
     * @param active_high true: transparent while EN is HIGH; false: while LOW.
     * @throws std::invalid_argument if there are too many pins.
     */
    Latch(std::vector<Net*> inputs,
          std::vector<Net*> outputs,
          std::string name,
          bool active_high = true);

    bool activeHigh() const { return active_high_; }

    LogicValue state() const { return state_; }

    /// State at power-up and after Simulator::reset(). Defaults to LOW.
    /// Also sets the current state, so call it before simulating (or reset).
    void setInitialState(LogicValue v) { initial_ = state_ = v; }
    LogicValue initialState() const { return initial_; }

    uint32_t intrinsicStages() const override { return 2; }
    const char* typeName() const override { return "dlatch"; }
    Region outputRegion() const override { return Region::Update; }
    void computeOutputs(std::vector<LogicValue>& out) override;

protected:
    void onReset() override { state_ = initial_; }

private:
    bool active_high_;
    LogicValue initial_ = LogicValue::LOW;
    LogicValue state_ = LogicValue::LOW;
};

#endif
