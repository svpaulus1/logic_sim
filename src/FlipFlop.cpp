// File: FlipFlop.cpp
#include "FlipFlop.h"
#include <stdexcept>

namespace
{
    std::vector<Net*> padded(std::vector<Net*> pins, std::size_t n, const char* what)
    {
        if (pins.size() > n)
            throw std::invalid_argument(std::string("FlipFlop: too many ") + what);
        pins.resize(n, nullptr);
        return pins;
    }

    /// Combines a possible-but-uncertain change with the current state.
    LogicValue maybe(LogicValue current, LogicValue next)
    {
        return next == current ? current : LogicValue::UNKNOWN;
    }
} // namespace

std::size_t FlipFlop::dataPinCount(Type t)
{
    return (t == Type::JK || t == Type::SR) ? 2 : 1;
}

FlipFlop::FlipFlop(Type t,
                   std::vector<Net*> inputs,
                   std::vector<Net*> outputs,
                   std::string name,
                   Edge edge)
    : Gate(padded(std::move(inputs), inputCount(t), "inputs"),
           padded(std::move(outputs), 2, "outputs"),
           std::move(name)),
      type_(t),
      edge_(edge)
{
}

const char* FlipFlop::typeName() const
{
    switch (type_)
    {
        case Type::D:  return "dff";
        case Type::T:  return "tff";
        case Type::JK: return "jkff";
        case Type::SR: return "srff";
    }
    return "ff";
}

void FlipFlop::onReset()
{
    state_ = initial_;
    last_clock_ = LogicValue::HIGHZ;
}

LogicValue FlipFlop::nextState_() const
{
    const LogicValue a = asInput(readInput_(0));

    switch (type_)
    {
        case Type::D:
            return a;

        case Type::T:
            if (a == LogicValue::LOW)  return state_;
            if (a == LogicValue::HIGH) return ~state_;
            return maybe(state_, ~state_);

        case Type::JK:
        {
            const LogicValue k = asInput(readInput_(1));
            if (!isKnown(a) || !isKnown(k)) return LogicValue::UNKNOWN;
            if (a == LogicValue::LOW && k == LogicValue::LOW) return state_;
            if (a == LogicValue::HIGH && k == LogicValue::HIGH) return ~state_;
            return a;   // J=1,K=0 sets; J=0,K=1 resets
        }

        case Type::SR:
        {
            const LogicValue r = asInput(readInput_(1));
            if (!isKnown(a) || !isKnown(r)) return LogicValue::UNKNOWN;
            if (a == LogicValue::HIGH && r == LogicValue::HIGH) return LogicValue::UNKNOWN;
            if (a == LogicValue::HIGH) return LogicValue::HIGH;
            if (r == LogicValue::HIGH) return LogicValue::LOW;
            return state_;
        }
    }
    return LogicValue::UNKNOWN;
}

void FlipFlop::computeOutputs(std::vector<LogicValue>& out)
{
    const LogicValue clk = readInput_(clockPin());
    const LogicValue set = readInput_(setPin());
    const LogicValue rst = readInput_(resetPin());

    // Edge detection. Always remember the new clock level, even when an async
    // control overrides, so releasing it does not replay an old edge.
    const LogicValue from = (edge_ == Edge::Rising) ? LogicValue::LOW : LogicValue::HIGH;
    const LogicValue to   = ~from;
    const LogicValue prev = last_clock_;
    const bool clean_edge = (prev == from && clk == to);
    const bool maybe_edge = (prev == from && clk == LogicValue::UNKNOWN) ||
                            (prev == LogicValue::UNKNOWN && clk == to);
    if (clk != LogicValue::HIGHZ) last_clock_ = clk;

    if (rst == LogicValue::HIGH)
        state_ = LogicValue::LOW;
    else if (set == LogicValue::HIGH)
        state_ = LogicValue::HIGH;
    else if (rst == LogicValue::UNKNOWN || set == LogicValue::UNKNOWN)
    {
        // Might be held in set or reset: only a state that satisfies every
        // possibility survives.
        LogicValue s = state_;
        if (rst == LogicValue::UNKNOWN) s = maybe(s, LogicValue::LOW);
        if (set == LogicValue::UNKNOWN) s = maybe(s, LogicValue::HIGH);
        state_ = s;
    }
    else if (clean_edge)
        state_ = nextState_();
    else if (maybe_edge)
        state_ = maybe(state_, nextState_());

    out[0] = state_;
    out[1] = ~state_;
}
