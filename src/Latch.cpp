// File: Latch.cpp
#include "Latch.h"
#include <stdexcept>

namespace
{
    std::vector<Net*> padded(std::vector<Net*> pins, std::size_t n, const char* what)
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
