/**
 * @file Input.h
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
 * This is how stimulus enters a simulation: a GUI switch or a testbench
 * calls Simulator::setInput(), which lands as an ordinary event at the
 * current time, so the net goes through normal driver resolution (unlike
 * Net::forceValue, which overrides it).
 *
 * The value survives Simulator::reset(), like a physical switch would.
 */
class Input : public Gate
{
public:
    Input(Net* output, std::string name, LogicValue initial = LogicValue::LOW)
        : Gate({}, {output}, std::move(name)),
          value_(initial)
    {}

    /// The value this input is set to (it reaches the net at the next run).
    LogicValue value() const { return value_; }

    /// Changes the value and schedules it at @p now. Used by Simulator.
    void setValue(LogicValue v, uint64_t now, EventQueue& eq)
    {
        value_ = v;
        evaluate(now, eq);
    }

    uint32_t intrinsicStages() const override { return 0; }
    const char* typeName() const override { return "input"; }
    bool usesTimingModel() const override { return false; }
    void computeOutputs(std::vector<LogicValue>& out) override { out[0] = value_; }

private:
    LogicValue value_;
};

#endif
