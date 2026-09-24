// File: Module.cpp
#include "Module.h"
#include <algorithm>
#include <stdexcept>
#include "Latch.h"

Module::Module(std::string name,
               std::vector<std::string> input_ports,
               std::vector<std::string> output_ports)
    : name_(std::move(name)),
      input_ports_(std::move(input_ports)),
      output_ports_(std::move(output_ports))
{
    std::vector<std::string> all(input_ports_);
    all.insert(all.end(), output_ports_.begin(), output_ports_.end());
    for (std::size_t i = 0; i < all.size(); ++i)
    {
        if (all[i].empty())
            throw std::invalid_argument("Module " + name_ + ": empty port name");
        if (std::find(all.begin() + static_cast<std::ptrdiff_t>(i) + 1, all.end(), all[i]) != all.end())
            throw std::invalid_argument("Module " + name_ + ": duplicate port " + all[i]);
    }
}

void Module::addItem_(Item item, const char* default_prefix)
{
    if (item.name.empty())
        item.name = default_prefix + std::to_string(items_.size());

    for (const Item& other : items_)
        if (other.name == item.name)
            throw std::invalid_argument("Module " + name_ + ": duplicate item name " + item.name);

    items_.push_back(std::move(item));
}

Module& Module::addGate(BuiltinGate::Type type,
                        std::vector<std::string> inputs,
                        std::vector<std::string> outputs,
                        std::string name)
{
    BuiltinGate::checkPins(type, inputs.size(), outputs.size());

    Item item;
    item.make = [type](std::vector<Net*> in, std::vector<Net*> out, std::string n)
    {
        return std::make_unique<BuiltinGate>(type, std::move(in), std::move(out), std::move(n));
    };
    item.inputs = std::move(inputs);
    item.outputs = std::move(outputs);
    item.name = std::move(name);
    addItem_(std::move(item), BuiltinGate::gateName(type));
    return *this;
}

Module& Module::addFlipFlop(FlipFlop::Type type,
                            std::vector<std::string> inputs,
                            std::vector<std::string> outputs,
                            std::string name,
                            FlipFlop::Edge edge)
{
    if (inputs.size() > FlipFlop::inputCount(type) || outputs.size() > 2)
        throw std::invalid_argument("Module " + name_ + ": too many flip-flop pins");

    Item item;
    item.make = [type, edge](std::vector<Net*> in, std::vector<Net*> out, std::string n)
    {
        return std::make_unique<FlipFlop>(type, std::move(in), std::move(out), std::move(n), edge);
    };
    item.inputs = std::move(inputs);
    item.outputs = std::move(outputs);
    item.name = std::move(name);
    addItem_(std::move(item), "ff");
    return *this;
}

Module& Module::addLatch(std::vector<std::string> inputs,
                         std::vector<std::string> outputs,
                         std::string name,
                         bool active_high)
{
    if (inputs.size() > 2 || outputs.size() > 2)
        throw std::invalid_argument("Module " + name_ + ": too many latch pins");

    Item item;
    item.make = [active_high](std::vector<Net*> in, std::vector<Net*> out, std::string n)
    {
        return std::make_unique<Latch>(std::move(in), std::move(out), std::move(n), active_high);
    };
    item.inputs = std::move(inputs);
    item.outputs = std::move(outputs);
    item.name = std::move(name);
    addItem_(std::move(item), "latch");
    return *this;
}

Module& Module::addPrimitive(Factory make,
                             std::vector<std::string> inputs,
                             std::vector<std::string> outputs,
                             std::string name)
{
    if (!make)
        throw std::invalid_argument("Module " + name_ + ": empty factory");

    Item item;
    item.make = std::move(make);
    item.inputs = std::move(inputs);
    item.outputs = std::move(outputs);
    item.name = std::move(name);
    addItem_(std::move(item), "u");
    return *this;
}

Module& Module::addInstance(std::shared_ptr<const Module> sub,
                            std::vector<std::string> inputs,
                            std::vector<std::string> outputs,
                            std::string name)
{
    if (!sub)
        throw std::invalid_argument("Module " + name_ + ": null sub-module");
    if (sub.get() == this)
        throw std::invalid_argument("Module " + name_ + ": cannot contain itself");
    if (inputs.size() != sub->input_ports_.size() ||
        outputs.size() != sub->output_ports_.size())
        throw std::invalid_argument("Module " + name_ + ": pin count does not match ports of " + sub->name_);

    const std::string prefix = sub->name_;
    Item item;
    item.sub = std::move(sub);
    item.inputs = std::move(inputs);
    item.outputs = std::move(outputs);
    item.name = std::move(name);
    addItem_(std::move(item), prefix.c_str());
    return *this;
}
