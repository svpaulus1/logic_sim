/**
 * @file Latch.h
 * @author Sebastian Paulus
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
 * Pin layout:
 *  - inputs:  {D, EN}
 *  - outputs: {Q, QN}
 *
 * Trailing pins may be omitted; they are padded with unconnected pins.
 *
 * Behaviour:
 *  - EN active: Q follows D.
 *  - EN inactive or floating (HIGHZ / unconnected): Q holds.
 *  - EN UNKNOWN: Q becomes UNKNOWN unless D already equals Q.
 *  - A floating D reads as UNKNOWN.
 *  - Like FlipFlop, outputs commit in the Update region.
 *
 * @code
 *   // Transparent while en is HIGH.
 *   Latch& l = sim.addLatch({&d, &en}, {&q}, "latch0");
 * @endcode
 */
class Latch : public Gate
{
public:
    /**
     * @brief Builds the latch and wires it into the netlist.
     *
     * @param inputs      {D, EN}; nullptr leaves a pin unconnected.
     * @param outputs     {Q, QN}; either may be omitted or nullptr.
     * @param name        Descriptive name (diagnostics, GUI, VCD output).
     * @param active_high true: transparent while EN is HIGH; false: while LOW.
     *
     * @throws std::invalid_argument if more pins are given than the layout
     *         has.
     */
    Latch(std::vector<Net*> inputs,
          std::vector<Net*> outputs,
          std::string name,
          bool active_high = true);

    /// @brief Which EN level makes the latch transparent.
    /// @return true if transparent while EN is HIGH, false if while LOW.
    bool activeHigh() const { return active_high_; }

    /// @brief The stored bit, for display (e.g. a GUI showing the contents).
    /// @return The current state.
    LogicValue state() const { return state_; }

    /**
     * @brief State at power-up and after Simulator::reset().
     *
     * Also sets the current state, so call it before simulating (or reset).
     *
     * @param v The state; LOW by default.
     */
    void setInitialState(LogicValue v) { initial_ = state_ = v; }

    /// @brief State at power-up and after Simulator::reset().
    /// @return The value set by setInitialState(), LOW by default.
    LogicValue initialState() const { return initial_; }

    /// @brief Stage count from D to Q.
    /// @return 2.
    uint32_t intrinsicStages() const override { return 2; }

    /// @brief Type name used in diagnostics, generated gate names and the GUI.
    /// @return "dlatch".
    const char* typeName() const override { return "dlatch"; }

    /// @brief Sequential element: outputs commit after every reader sampled.
    /// @return Region::Update.
    Region outputRegion() const override { return Region::Update; }

    /**
     * @brief Updates the state from D and EN and sets Q and QN.
     *
     * See the class description for the rules, including UNKNOWN inputs.
     *
     * @param out {Q, QN}.
     */
    void computeOutputs(std::vector<LogicValue>& out) override;

protected:
    /// Restores initialState().
    void onReset() override { state_ = initial_; }

private:
    bool active_high_;                     ///< Transparent while EN is HIGH.
    LogicValue initial_ = LogicValue::LOW; ///< Power-up state.
    LogicValue state_ = LogicValue::LOW;   ///< The stored bit.
};

#endif
