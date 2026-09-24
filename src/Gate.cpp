// File: Gate.cpp

#include "Gate.h"
#include <algorithm>
#include <stdexcept>
#include "EventQueue.h"

Gate::Gate(std::vector<Net*> inputs,
           std::vector<Net*> outputs,
           std::string name,
           Strength strength)
    : inputs_(std::move(inputs)),
      outputs_(std::move(outputs)),
      name_(std::move(name)),
      strength_(strength)
{
    // Checked before registering anywhere, so a throw leaves no dangling
    // pointers behind in the nets.
    if (outputs_.size() > 0xFFFFu)
        throw std::invalid_argument("Gate: output count exceeds uint16_t width");

    for (Net* in : inputs_)
        if (in) in->addSink(this);

    driver_ids_.assign(outputs_.size(), 0);
    for (std::size_t i = 0; i < outputs_.size(); ++i)
        if (outputs_[i]) driver_ids_[i] = outputs_[i]->addDriver(this, strength_);

    pending_serial_.assign(outputs_.size(), 0);
    target_.assign(outputs_.size(), LogicValue::HIGHZ);
    current_.assign(outputs_.size(), LogicValue::HIGHZ);
    next_.assign(outputs_.size(), LogicValue::UNKNOWN);
}

Gate::~Gate()
{
    for (Net* in : inputs_)
        if (in) in->removeSink(this);
    for (std::size_t i = 0; i < outputs_.size(); ++i)
        if (outputs_[i]) outputs_[i]->removeDriver(driver_ids_[i]);
}

uint32_t Gate::delayFor(LogicValue to) const
{
    switch (to)
    {
        case LogicValue::LOW: return fall_delay_;
        case LogicValue::HIGH: return rise_delay_;
        case LogicValue::HIGHZ: return decay_delay_;
        default:
            // Verilog: a transition to X takes the smallest of the delays.
            return std::min({rise_delay_, fall_delay_, decay_delay_});
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
    std::fill(next_.begin(), next_.end(), LogicValue::UNKNOWN);
    computeOutputs(next_);

    for (std::size_t i = 0; i < outputs_.size(); ++i)
    {
        if (next_[i] == target_[i]) continue;
        scheduleOutput_(i, next_[i], now + delayFor(next_[i]), eq);
    }
}

void Gate::scheduleOutput_(std::size_t pin, LogicValue v, uint64_t time, EventQueue& eq)
{
    target_[pin] = v;
    const uint32_t serial = ++pending_serial_[pin];
    eq.schedule(time,
                Event{this, static_cast<uint16_t>(pin), v, serial},
                outputRegion());
}

void Gate::cancelPending_(std::size_t pin)
{
    ++pending_serial_[pin];
    target_[pin] = current_[pin];
}

void Gate::commit(uint16_t out_index,
                  LogicValue value,
                  uint32_t serial,
                  uint64_t now,
                  EventQueue& eq)
{
    assert(out_index < outputs_.size() && "event for a nonexistent output");
    if (serial != pending_serial_[out_index]) return;

    current_[out_index] = value;
    if (Net* net = outputs_[out_index])
        net->driveFrom(driver_ids_[out_index], value, now, eq);

    onCommitted(out_index, value, now, eq);
}

void Gate::resetState()
{
    // Invalidate anything still in flight, even though the Simulator clears
    // its queue on reset as well.
    for (uint32_t& s : pending_serial_) ++s;
    std::fill(target_.begin(), target_.end(), LogicValue::HIGHZ);
    std::fill(current_.begin(), current_.end(), LogicValue::HIGHZ);
    onReset();
}

void Gate::connectInput(std::size_t pin, Net* net)
{
    if (pin >= inputs_.size())
        throw std::out_of_range("Gate::connectInput: no such input pin");
    if (inputs_[pin] == net) return;

    if (inputs_[pin]) inputs_[pin]->removeSink(this);
    inputs_[pin] = net;
    if (net) net->addSink(this);
}

void Gate::connectOutput(std::size_t pin, Net* net)
{
    if (pin >= outputs_.size())
        throw std::out_of_range("Gate::connectOutput: no such output pin");
    if (outputs_[pin] == net) return;

    if (outputs_[pin]) outputs_[pin]->removeDriver(driver_ids_[pin]);
    outputs_[pin] = net;
    driver_ids_[pin] = net ? net->addDriver(this, strength_, current_[pin]) : 0;
}

void Gate::disconnect(const Net* net)
{
    if (!net) return;
    for (std::size_t i = 0; i < inputs_.size(); ++i)
        if (inputs_[i] == net) connectInput(i, nullptr);
    for (std::size_t i = 0; i < outputs_.size(); ++i)
        if (outputs_[i] == net) connectOutput(i, nullptr);
}
