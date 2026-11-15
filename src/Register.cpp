// File: Register.cpp
#include "Register.h"
#include <algorithm>
#include <stdexcept>

namespace
{
    std::vector<Net*> padded(std::vector<Net*> pins,
                             std::size_t n,
                             const char* what)
    {
        if (pins.size() > n)
            throw std::invalid_argument(std::string("Register: too many ") + what);
        pins.resize(n, nullptr);
        return pins;
    }

    std::size_t checkedSize(std::size_t size)
    {
        if (size == 0)
            throw std::invalid_argument("Register: size must be at least 1");
        return size;
    }

    /// Combines a possible-but-uncertain change with the current value.
    LogicValue maybe(LogicValue current, LogicValue next)
    {
        return next == current ? current : LogicValue::UNKNOWN;
    }
} // namespace

Register::Register(std::vector<Net*> inputs,
                   std::vector<Net*> outputs,
                   std::string name,
                   std::size_t size,
                   Edge edge)
    : Gate(padded(std::move(inputs), checkedSize(size) + 4, "inputs"),
           padded(std::move(outputs), size, "outputs"),
           std::move(name)),
      size_(size),
      edge_(edge),
      state_(size, LogicValue::LOW)
{}

void Register::setInitialState(LogicValue v)
{
    initial_ = v;
    std::fill(state_.begin(), state_.end(), v);
}

void Register::onReset()
{
    std::fill(state_.begin(), state_.end(), initial_);
    last_clock_ = LogicValue::HIGHZ;
}

void Register::computeOutputs(std::vector<LogicValue>& out)
{
    const LogicValue clk = readInput_(clockPin());
    const LogicValue en  = readInput_(enablePin());
    const LogicValue rst = readInput_(resetPin());
    const LogicValue oe  = readInput_(outputEnablePin());

    // Edge detection, as in FlipFlop. Always remember the new clock level,
    // even when RESET overrides, so releasing it does not replay an old edge.
    const LogicValue from = (edge_ == Edge::Rising) ? LogicValue::LOW : LogicValue::HIGH;
    const LogicValue to   = ~from;
    const LogicValue prev = last_clock_;
    const bool clean_edge = (prev == from && clk == to);
    const bool maybe_edge = (prev == from && clk == LogicValue::UNKNOWN) ||
                            (prev == LogicValue::UNKNOWN && clk == to);
    if (clk != LogicValue::HIGHZ) last_clock_ = clk;

    // An unconnected EN means "always load".
    const bool enabled = (en == LogicValue::HIGH || en == LogicValue::HIGHZ);
    const bool maybe_enabled = (en == LogicValue::UNKNOWN);

    if (rst == LogicValue::HIGH)
    {
        std::fill(state_.begin(), state_.end(), LogicValue::LOW);
    }
    else if (rst == LogicValue::UNKNOWN)
    {
        for (LogicValue& bit : state_)
            bit = maybe(bit, LogicValue::LOW);
    }
    else if (clean_edge && enabled)
    {
        for (std::size_t i = 0; i < size_; ++i)
            state_[i] = asInput(readInput_(i));
    }
    else if ((clean_edge && maybe_enabled) ||
             (maybe_edge && (enabled || maybe_enabled)))
    {
        // Might have loaded: only bits that would not change stay known.
        for (std::size_t i = 0; i < size_; ++i)
            state_[i] = maybe(state_[i], asInput(readInput_(i)));
    }

    // An unconnected OE means "always drive".
    for (std::size_t i = 0; i < size_; ++i)
    {
        if (oe == LogicValue::HIGH || oe == LogicValue::HIGHZ)
            out[i] = state_[i];
        else if (oe == LogicValue::LOW)
            out[i] = LogicValue::HIGHZ;
        else
            out[i] = LogicValue::UNKNOWN;
    }
}
