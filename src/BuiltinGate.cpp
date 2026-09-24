#include "BuiltinGate.h"
#include <cassert>

namespace
{
    LogicValue fAnd(LogicValue a, LogicValue b) { return a & b; }
    LogicValue fOr (LogicValue a, LogicValue b) { return a | b; }
    LogicValue fXor(LogicValue a, LogicValue b) { return a ^ b; }
    LogicValue fId(LogicValue a, LogicValue) { return a; }
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

    const std::size_t i = static_cast<std::size_t>(t);
    assert(i < static_cast<std::size_t>(Type::COUNT) && "bad gate type");
    return table[i];
}

BuiltinGate::BuiltinGate(Type t,
                         std::vector<Net*> inputs,
                         std::vector<Net*> outputs,
                         std::string name)
    : Gate(std::move(inputs), std::move(outputs), std::move(name)),
      type_(t)
{
    const Info& gi = info_(t);
    (void)gi;

    switch (gi.shape)
    {
        case Shape::Fold:
            assert(!inputs_.empty() && "fold gate needs at least one input");
            assert(!outputs_.empty() && "gate needs an output");
            assert((outputs_.size() == 1 ||
                    t == Type::BUF || t == Type::NOT) &&
                   "only buf/not may have multiple outputs");
            assert((inputs_.size() == 1 ||
                    (t != Type::BUF && t != Type::NOT)) &&
                   "buf/not take exactly one input");
            break;

        case Shape::Enable:
            assert(inputs_.size() == 2 && "enable gate takes {data, enable}");
            assert(outputs_.size() == 1 && "enable gate drives one output");
            break;

        case Shape::Constant:
            assert(inputs_.empty() && "pullup/pulldown take no inputs");
            assert(!outputs_.empty() && "gate needs an output");
            break;
    }
}

const char* BuiltinGate::gateName() const { return info_(type_).name; }

unsigned BuiltinGate::intrinsicStages() const { return info_(type_).stages; }

bool BuiltinGate::canTristate() const
{
    return info_(type_).shape == Shape::Enable;
}

void BuiltinGate::computeOutputs(std::vector<LogicValue>& out) const
{
    const Info& gi = info_(type_);

    switch (gi.shape)
    {
        case Shape::Fold:
            LogicValue acc = inputs_.front()->getValue();
            for (std::size_t i = 1; i < inputs_.size(); ++i)
                acc = gi.fold(acc, inputs_[i]->getValue());
            if (gi.invert) acc = ~acc;
            for (LogicValue& o : out) o = acc;
            break;

        case Shape::Enable: 
            const LogicValue data = inputs_[0]->getValue();
            const LogicValue en = inputs_[1]->getValue();
            const LogicValue off = (gi.enable_active == LogicValue::HIGH)
                ? LogicValue::LOW : LogicValue::HIGH;

            if (en == gi.enable_active)
                out[0] = gi.invert ? ~data : data;   // driving
            else if (en == off)
                out[0] = LogicValue::HIGHZ;          // released
            else
                out[0] = LogicValue::UNKNOWN;        // enable itself is X or Z
            break;
    

        case Shape::Constant:
            for (LogicValue& o : out) o = gi.constant;
            break;
    }
}
