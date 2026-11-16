/**
 * @file Latch.cpp
 * @author Sebastian Paulus
 * @date 2026/09/24
 * @brief Implementation of Latch (see Latch.h).
 */

#include "Latch.h"
#include <stdexcept>

namespace
{
    /// @brief Pads @p pins with unconnected (nullptr) pins up to @p n.
    /// @param pins The pins given.
    /// @param n    Pins in the full layout.
    /// @param what "inputs" or "outputs", for the error message.
    /// @return @p pins, resized to @p n.
    /// @throws std::invalid_argument if more than @p n pins were given.
    std::vector<Net*> padded(std::vector<Net*> pins,
                             std::size_t n,
                             const char* what)
    {
        if (pins.size() > n)
            throw std::invalid_argument(std::string("Latch: too many ") + what);
        pins.resize(n, nullptr);
        return pins;
    }
} // namespace

Latch::Latch(std::vector<Net*> inputs,
             std::vector<Net*> outputs,
             std::string name,
             bool active_high)
    : Gate(padded(std::move(inputs), 2, "inputs"),
           padded(std::move(outputs), 2, "outputs"),
           std::move(name)),
      active_high_(active_high)
{
}

void Latch::computeOutputs(std::vector<LogicValue>& out)
{
    const LogicValue d = asInput(readInput_(0));
    const LogicValue en = readInput_(1);
    const LogicValue active = active_high_ ? LogicValue::HIGH : LogicValue::LOW;

    if (en == active)
        state_ = d;
    else if (en == LogicValue::UNKNOWN && d != state_)
        state_ = LogicValue::UNKNOWN;

    out[0] = state_;
    out[1] = ~state_;
}
