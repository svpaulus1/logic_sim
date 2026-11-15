/**
 * @file Input.h
 * @author Sebastian Paulus
 * @date 2026/09/24
 * @brief An externally controlled signal source (switch, button, input pin).
 */

#ifndef INPUT_H
#define INPUT_H

#include <string>
#include "Gate.h"
#include "Types.h"

/**
 * @class Input
 * @brief Drives one net with a value set from outside the circuit.
 *
 * Pin layout:
 *  - inputs:  none
 *  - outputs: {Y}
 *
 * Behaviour:
 *  - This is how stimulus enters a simulation: a GUI switch or a testbench
 *    calls Simulator::setInput() (or scheduleInput() for a future time), which
 *    lands as an ordinary event, so the net goes through normal driver
 *    resolution (unlike Net::forceValue(), which overrides it).
 *  - The new value reaches the net with no delay, at the next run or step.
 *  - The value survives Simulator::reset(), like a physical switch would.
 *
 * @code
 *   Input& sw = sim.addInput(&a, "switch_a", LogicValue::LOW);
 *   sim.setInput(sw, LogicValue::HIGH);
 *   sim.runUntilIdle();
 * @endcode
 */
class Input : public Gate
{
public:
    /**
     * @brief Builds the input and wires it into the netlist.
     *
     * @param output  Net to drive; nullptr leaves it unconnected.
     * @param name    Descriptive name (diagnostics, GUI, VCD output).
     * @param initial Value driven from the start.
     */
    Input(Net* output, std::string name, LogicValue initial = LogicValue::LOW)
        : Gate({}, {output}, std::move(name)),
          value_(initial)
    {}

    /// @brief The value this input is set to (it reaches the net at the next
    ///        run).
    /// @return The current setting.
    LogicValue value() const { return value_; }

    /**
     * @brief Changes the value and schedules it at @p now. Used by Simulator.
     *
     * @param v   The new value.
     * @param now Current simulation time.
     * @param eq  Queue to schedule the change into.
     */
    void setValue(LogicValue v, uint64_t now, EventQueue& eq)
    {
        value_ = v;
        evaluate(now, eq);
    }

    /// @brief A source has no logic stages.
    /// @return 0.
    uint32_t intrinsicStages() const override { return 0; }

    /// @brief Type name used in diagnostics, generated gate names and the GUI.
    /// @return "input".
    const char* typeName() const override { return "input"; }

    /// @brief A source has no propagation delay of its own.
    /// @return false.
    bool usesTimingModel() const override { return false; }

    /// @brief Drives the current setting.
    /// @param out {Y}.
    void computeOutputs(std::vector<LogicValue>& out) override { out[0] = value_; }

private:
    LogicValue value_; ///< The current setting.
};

#endif
