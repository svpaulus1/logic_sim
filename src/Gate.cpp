// File: Gate.cpp

#include "Gate.h"
#include "EventQueue.h"
 
Gate::Gate(std::vector<Net*> inputs,
           std::vector<Net*> outputs,
           std::string name)
    : inputs_(std::move(inputs)),
      outputs_(std::move(outputs)),
      name_(std::move(name))
{
    for (Net* in : inputs_)
    {
        assert(in && "null input net");
        in->addOutput(this);
    }
 
    driver_ids_.reserve(outputs_.size());
    for (Net* out : outputs_)
    {
        assert(out && "null output net");
        driver_ids_.push_back(out->addDriver(this));
    }
 
    assert(outputs_.size() <= 0xFFFFu && "output count exceeds uint16_t width");
    pending_serial_.assign(outputs_.size(), 0);
    target_.assign(outputs_.size(), LogicValue::HIGHZ);
}
 
uint32_t Gate::delayFor(LogicValue to) const
{
    switch (to)
    {
        case LogicValue::LOW: return fall_delay_;
        case LogicValue::HIGH: return rise_delay_;
        case LogicValue::HIGHZ: return decay_delay_;
        default:
            return ((rise_delay_ < fall_delay_) ? rise_delay_ : fall_delay_);
    }
}
 
std::size_t Gate::maxFanout() const
{
    std::size_t n = 0;
    for (const Net* net : outputs_)
        if (net && net->sinkCount() > n) n = net->sinkCount();
    return n;
}
 
void Gate::evaluate(uint64_t now, EventQueue& eq)
{
    std::vector<LogicValue> next(outputs_.size());
    computeOutputs(next);
 
    for (std::size_t i = 0; i < outputs_.size(); ++i)
    {
        if (next[i] == target_[i]) continue;
 
        target_[i] = next[i];
        const uint32_t serial = ++pending_serial_[i];
 
        eq.schedule(now + delayFor(next[i]),
                    Event{this, static_cast<uint16_t>(i), next[i], serial},
                    Region::Active);
    }
}
 
void Gate::commit(uint16_t out_index,
                  LogicValue value,
                  uint32_t serial,
                  uint64_t now,
                  EventQueue& eq)
{
    if (serial != pending_serial_[out_index]) return;
 
    outputs_[out_index]->driveFrom(driver_ids_[out_index], value, now, eq);
}
