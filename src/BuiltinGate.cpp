#include "BuiltinGate.h"
#include <stdexcept>

namespace
{
    LogicValue fAnd(LogicValue a, LogicValue b) { return a & b; }
    LogicValue fOr (LogicValue a, LogicValue b) { return a | b; }
    LogicValue fXor(LogicValue a, LogicValue b) { return a ^ b; }
    LogicValue fId(LogicValue a, LogicValue) { return a; }

    std::size_t index(BuiltinGate::Type t) { return static_cast<std::size_t>(t); }
} // namespace

const BuiltinGate::Info& BuiltinGate::info_(Type t)
{
    constexpr LogicValue H = LogicValue::HIGH;
    constexpr LogicValue L = LogicValue::LOW;
    constexpr LogicValue X = LogicValue::UNKNOWN;

    // Indexed by Type.
    static const Info table[static_cast<std::size_t>(Type::COUNT)] = {
        //  name     stages   shape        fold    invert enable const
        {  "and",      2, Shape::Fold,     fAnd,    false,  X,     X },
        {  "nand",     1, Shape::Fold,     fAnd,    true,   X,     X },
        {  "or",       2, Shape::Fold,     fOr,     false,  X,     X },
        {  "nor",      1, Shape::Fold,     fOr,     true,   X,     X },
        {  "xor",      4, Shape::Fold,     fXor,    false,  X,     X },
        {  "xnor",     4, Shape::Fold,     fXor,    true,   X,     X },
        {  "buf",      2, Shape::Fold,     fId,     false,  X,     X },
        {  "not",      1, Shape::Fold,     fId,     true,   X,     X },
        {  "bufif0",   2, Shape::Enable,   nullptr, false,  L,     X },
        {  "bufif1",   2, Shape::Enable,   nullptr, false,  H,     X },
        {  "notif0",   2, Shape::Enable,   nullptr, true,   L,     X },
        {  "notif1",   2, Shape::Enable,   nullptr, true,   H,     X },
        {  "pullup",   0, Shape::Constant, nullptr, false,  X,     H },
        {  "pulldown", 0, Shape::Constant, nullptr, false,  X,     L },
    };

    const std::size_t i = index(t);
    if (i >= index(Type::COUNT))
        throw std::invalid_argument("BuiltinGate: bad gate type");
    return table[i];
}

void BuiltinGate::checkPins(Type t, std::size_t num_inputs, std::size_t num_outputs)
{
    const Info& gi = info_(t);
    const std::string who = std::string(gi.name) + ": ";
    const bool single = (t == Type::BUF || t == Type::NOT);

    switch (gi.shape)
    {
        case Shape::Fold:
            if (single && num_inputs != 1)
                throw std::invalid_argument(who + "takes exactly one input");
            if (num_inputs == 0)
                throw std::invalid_argument(who + "needs at least one input");
            if (num_outputs == 0)
                throw std::invalid_argument(who + "needs an output");
            if (!single && num_outputs != 1)
                throw std::invalid_argument(who + "only buf/not may have multiple outputs");
            break;

        case Shape::Enable:
            if (num_inputs != 2)
                throw std::invalid_argument(who + "takes {data, enable}");
            if (num_outputs != 1)
                throw std::invalid_argument(who + "drives exactly one output");
            break;

        case Shape::Constant:
            if (num_inputs != 0)
                throw std::invalid_argument(who + "takes no inputs");
            if (num_outputs == 0)
                throw std::invalid_argument(who + "needs an output");
            break;
    }
}

BuiltinGate::BuiltinGate(Type t,
                         std::vector<Net*> inputs,
                         std::vector<Net*> outputs,
                         std::string name)
    : Gate(std::move(inputs), std::move(outputs), std::move(name),
           info_(t).shape == Shape::Constant ? Strength::Pull : Strength::Strong),
      type_(t)
{
    // Runs after Gate registered the pins; if it throws, ~Gate unregisters.
    checkPins(t, inputs_.size(), outputs_.size());
}

const char* BuiltinGate::gateName() const { return info_(type_).name; }

const char* BuiltinGate::gateName(Type t) { return info_(t).name; }

std::optional<BuiltinGate::Type> BuiltinGate::typeFromName(std::string_view name)
{
    for (std::size_t i = 0; i < index(Type::COUNT); ++i)
    {
        const Type t = static_cast<Type>(i);
        if (name == info_(t).name) return t;
    }
    return std::nullopt;
}

uint32_t BuiltinGate::intrinsicStages() const { return info_(type_).stages; }

bool BuiltinGate::canTristate() const
{
    return info_(type_).shape == Shape::Enable;
}

bool BuiltinGate::usesTimingModel() const
{
    // pullup / pulldown are resistors, not logic: no propagation delay.
    return info_(type_).shape != Shape::Constant;
}

void BuiltinGate::computeOutputs(std::vector<LogicValue>& out)
{
    const Info& gi = info_(type_);

    switch (gi.shape)
    {
        case Shape::Fold:
        {
            // asInput() turns a floating input into X, so a buf / single-input
            // gate never passes HIGHZ through as if it were a tri-state driver.
            LogicValue acc = asInput(readInput_(0));
            for (std::size_t i = 1; i < inputs_.size(); ++i)
                acc = gi.fold(acc, asInput(readInput_(i)));
            if (gi.invert) acc = ~acc;
            for (LogicValue& o : out) o = acc;
            break;
        }

        case Shape::Enable:
        {
            const LogicValue data = asInput(readInput_(0));
            const LogicValue en = readInput_(1);
            const LogicValue off = (gi.enable_active == LogicValue::HIGH)
                ? LogicValue::LOW : LogicValue::HIGH;

            if (en == gi.enable_active)
                out[0] = gi.invert ? ~data : data;   // driving
            else if (en == off)
                out[0] = LogicValue::HIGHZ;          // released
            else
                out[0] = LogicValue::UNKNOWN;        // enable itself is X or Z
            break;
        }

        case Shape::Constant:
            for (LogicValue& o : out) o = gi.constant;
            break;
    }
}
