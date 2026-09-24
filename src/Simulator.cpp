// File: Simulator.cpp
#include "Simulator.h"
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <unordered_map>

namespace
{
    /// Deeper than any real design; catches modules that contain themselves.
    constexpr unsigned kMaxModuleDepth = 64;
} // namespace

Simulator::Simulator(std::size_t wheel_size)
    : queue_(wheel_size),
      timing_(std::make_unique<UnitDelayModel>())
{
}

Simulator::~Simulator()
{
    // Gates unregister from their nets on destruction, so they go first.
    gates_.clear();
    nets_.clear();
}

// --- Ownership checks -----------------------------------------------------

void Simulator::requireOwned_(const Net* net) const
{
    if (net && net->change_log_ != &change_log_)
        throw std::invalid_argument("net '" + net->getName() + "' does not belong to this Simulator");
}

void Simulator::requireOwned_(const Gate& gate) const
{
    if (gate.id_ >= gates_.size() || gates_[gate.id_].get() != &gate)
        throw std::invalid_argument("gate '" + gate.getName() + "' does not belong to this Simulator");
}

// --- Building -------------------------------------------------------------

Net& Simulator::addNet(std::string name)
{
    const auto id = static_cast<uint32_t>(nets_.size());
    if (name.empty()) name = "n" + std::to_string(id);

    auto net = std::make_unique<Net>(std::move(name));
    net->id_ = id;
    net->change_log_ = &change_log_;
    Net& ref = *net;
    nets_.push_back(std::move(net));
    ++live_nets_;
    dirty_ = true;
    return ref;
}

Gate& Simulator::adopt(std::unique_ptr<Gate> gate)
{
    if (!gate)
        throw std::invalid_argument("Simulator::adopt: null gate");
    if (gate->id_ != Gate::kNoId)
        throw std::invalid_argument("Simulator::adopt: gate already belongs to a Simulator");
    for (const Net* n : gate->inputs_)  requireOwned_(n);
    for (const Net* n : gate->outputs_) requireOwned_(n);

    const auto id = static_cast<uint32_t>(gates_.size());
    gate->id_ = id;
    if (gate->name_.empty()) gate->name_ = gate->typeName() + std::to_string(id);
    if (timing_ && gate->usesTimingModel() && !gate->hasFixedDelays())
        timing_->apply(*gate);

    Gate& ref = *gate;
    gates_.push_back(std::move(gate));
    ++live_gates_;
    dirty_ = true;
    return ref;
}

BuiltinGate& Simulator::addGate(BuiltinGate::Type type,
                                std::vector<Net*> inputs,
                                std::vector<Net*> outputs,
                                std::string name)
{
    return emplace<BuiltinGate>(type, std::move(inputs), std::move(outputs), std::move(name));
}

FlipFlop& Simulator::addFlipFlop(FlipFlop::Type type,
                                 std::vector<Net*> inputs,
                                 std::vector<Net*> outputs,
                                 std::string name,
                                 FlipFlop::Edge edge)
{
    return emplace<FlipFlop>(type, std::move(inputs), std::move(outputs), std::move(name), edge);
}

Latch& Simulator::addLatch(std::vector<Net*> inputs,
                           std::vector<Net*> outputs,
                           std::string name,
                           bool active_high)
{
    return emplace<Latch>(std::move(inputs), std::move(outputs), std::move(name), active_high);
}

Input& Simulator::addInput(Net* output, std::string name, LogicValue initial)
{
    return emplace<Input>(output, std::move(name), initial);
}

Clock& Simulator::addClock(Net* output,
                           std::string name,
                           uint64_t high_ticks,
                           uint64_t low_ticks,
                           LogicValue initial)
{
    return emplace<Clock>(output, std::move(name), high_ticks, low_ticks, initial);
}

Simulator::Instance Simulator::instantiate(const Module& module,
                                           std::vector<Net*> inputs,
                                           std::vector<Net*> outputs,
                                           std::string instance_name)
{
    if (inputs.size() != module.inputPorts().size() ||
        outputs.size() != module.outputPorts().size())
        throw std::invalid_argument("instantiate: pin count does not match ports of " + module.getName());
    for (const Net* n : inputs)  requireOwned_(n);
    for (const Net* n : outputs) requireOwned_(n);

    if (instance_name.empty())
        instance_name = module.getName() + std::to_string(instance_counter_++);

    Instance created;
    created.name = instance_name;
    instantiate_(module, inputs, outputs, instance_name, created, 0);
    return created;
}

void Simulator::instantiate_(const Module& module,
                             const std::vector<Net*>& inputs,
                             const std::vector<Net*>& outputs,
                             const std::string& path,
                             Instance& created,
                             unsigned depth)
{
    if (depth > kMaxModuleDepth)
        throw std::runtime_error("instantiate: modules nested too deep (does one contain itself?)");

    std::unordered_map<std::string, Net*> local;

    auto freshNet = [&](const std::string& local_name) -> Net*
    {
        Net& n = addNet(path + "." + local_name);
        created.net_ids.push_back(n.id());
        return &n;
    };

    // Ports map to the caller's nets; an unconnected port still gets a net
    // of its own so the gates inside that share it stay connected.
    for (std::size_t i = 0; i < inputs.size(); ++i)
        local[module.input_ports_[i]] = inputs[i] ? inputs[i] : freshNet(module.input_ports_[i]);
    for (std::size_t i = 0; i < outputs.size(); ++i)
        local[module.output_ports_[i]] = outputs[i] ? outputs[i] : freshNet(module.output_ports_[i]);

    auto resolve = [&](const std::vector<std::string>& names)
    {
        std::vector<Net*> pins;
        pins.reserve(names.size());
        for (const std::string& n : names)
        {
            if (n.empty())
            {
                pins.push_back(nullptr);
                continue;
            }
            auto it = local.find(n);
            if (it == local.end()) it = local.emplace(n, freshNet(n)).first;
            pins.push_back(it->second);
        }
        return pins;
    };

    for (const Module::Item& item : module.items_)
    {
        std::vector<Net*> ins = resolve(item.inputs);
        std::vector<Net*> outs = resolve(item.outputs);
        const std::string child = path + "." + item.name;

        if (item.sub)
        {
            instantiate_(*item.sub, ins, outs, child, created, depth + 1);
        }
        else
        {
            std::unique_ptr<Gate> g = item.make(std::move(ins), std::move(outs), child);
            created.gate_ids.push_back(adopt(std::move(g)).id());
        }
    }
}

// --- Editing ---------------------------------------------------------------

void Simulator::removeGate(Gate& gate)
{
    requireOwned_(gate);

    queue_.cancel(&gate);
    for (auto it = stimuli_.begin(); it != stimuli_.end();)
        it = (it->second.input == &gate) ? stimuli_.erase(it) : std::next(it);

    const uint32_t id = gate.id_;
    gates_[id].reset();   // ~Gate unregisters from its nets
    --live_gates_;
    dirty_ = true;
}

void Simulator::removeNet(Net& net)
{
    requireOwned_(&net);

    // Copy first: disconnect() edits the very lists being walked.
    std::vector<Gate*> touching(net.sinks_);
    for (const Net::Driver& d : net.drivers_)
        if (d.gate) touching.push_back(d.gate);
    for (Gate* g : touching)
        g->disconnect(&net);

    change_log_.erase(std::remove(change_log_.begin(), change_log_.end(), &net),
                      change_log_.end());

    const uint32_t id = net.id_;
    nets_[id].reset();
    --live_nets_;
    dirty_ = true;
}

void Simulator::removeInstance(const Instance& instance)
{
    for (uint32_t id : instance.gate_ids)
        if (Gate* g = gate(id)) removeGate(*g);
    for (uint32_t id : instance.net_ids)
        if (Net* n = net(id)) removeNet(*n);
}

void Simulator::connectInput(Gate& gate, std::size_t pin, Net* net)
{
    requireOwned_(gate);
    requireOwned_(net);
    gate.connectInput(pin, net);
    dirty_ = true;
}

void Simulator::connectOutput(Gate& gate, std::size_t pin, Net* net)
{
    requireOwned_(gate);
    requireOwned_(net);
    gate.connectOutput(pin, net);
    dirty_ = true;
}

Net& Simulator::mergeNets(Net& keep, Net& absorb)
{
    requireOwned_(&keep);
    requireOwned_(&absorb);
    if (&keep == &absorb) return keep;

    std::vector<Gate*> touching(absorb.sinks_);
    for (const Net::Driver& d : absorb.drivers_)
        if (d.gate) touching.push_back(d.gate);

    for (Gate* g : touching)
    {
        for (std::size_t i = 0; i < g->numInputs(); ++i)
            if (g->inputs_[i] == &absorb) g->connectInput(i, &keep);
        for (std::size_t i = 0; i < g->numOutputs(); ++i)
            if (g->outputs_[i] == &absorb) g->connectOutput(i, &keep);
    }

    removeNet(absorb);
    return keep;
}

// --- Lookup ---------------------------------------------------------------

Net* Simulator::net(uint32_t id) const
{
    return id < nets_.size() ? nets_[id].get() : nullptr;
}

Gate* Simulator::gate(uint32_t id) const
{
    return id < gates_.size() ? gates_[id].get() : nullptr;
}

Net* Simulator::findNet(std::string_view name) const
{
    for (const auto& n : nets_)
        if (n && n->getName() == name) return n.get();
    return nullptr;
}

Gate* Simulator::findGate(std::string_view name) const
{
    for (const auto& g : gates_)
        if (g && g->getName() == name) return g.get();
    return nullptr;
}

std::vector<Net*> Simulator::nets() const
{
    std::vector<Net*> out;
    out.reserve(live_nets_);
    for (const auto& n : nets_)
        if (n) out.push_back(n.get());
    return out;
}

std::vector<Gate*> Simulator::gates() const
{
    std::vector<Gate*> out;
    out.reserve(live_gates_);
    for (const auto& g : gates_)
        if (g) out.push_back(g.get());
    return out;
}

// --- Timing ---------------------------------------------------------------

void Simulator::setTimingModel(std::unique_ptr<TimingModel> model)
{
    timing_ = std::move(model);
    dirty_ = true;
}

// --- Stimulus -------------------------------------------------------------

void Simulator::setInput(Input& input, LogicValue value)
{
    requireOwned_(input);
    resync_();
    input.setValue(value, queue_.now(), queue_);
}

void Simulator::scheduleInput(Input& input, LogicValue value, uint64_t time)
{
    requireOwned_(input);
    if (time < queue_.now())
        throw std::invalid_argument("scheduleInput: time is in the past");
    stimuli_.emplace(time, Stimulus{&input, value});
}

void Simulator::setClockRunning(Clock& clock, bool running)
{
    requireOwned_(clock);
    resync_();
    clock.setRunning(running, queue_.now(), queue_);
}

void Simulator::forceNet(Net& net, LogicValue value)
{
    requireOwned_(&net);
    resync_();
    net.forceValue(value, queue_.now(), queue_);
}

void Simulator::releaseNet(Net& net)
{
    requireOwned_(&net);
    resync_();
    net.release(queue_.now(), queue_);
}

// --- Running --------------------------------------------------------------

void Simulator::resync_()
{
    if (!dirty_) return;
    dirty_ = false;

    // Fanout-based models depend on the wiring, so re-apply after every edit.
    if (timing_)
        for (const auto& g : gates_)
            if (g && g->usesTimingModel() && !g->hasFixedDelays())
                timing_->apply(*g);

    // Resolve every net before any gate looks at one: waking sinks net by net
    // would let a gate read a neighbour still at its stale (e.g. constructor
    // default X) value, which a flip-flop can mistake for a clock edge.
    // No wake-ups are needed since every gate is evaluated right after.
    for (const auto& n : nets_)
        if (n) n->resolve_();

    const uint64_t t = queue_.now();
    for (const auto& g : gates_)
        if (g) g->evaluate(t, queue_);
}

bool Simulator::applyDueStimuli_()
{
    bool any = false;
    while (!stimuli_.empty() && stimuli_.begin()->first <= queue_.now())
    {
        const Stimulus s = stimuli_.begin()->second;
        stimuli_.erase(stimuli_.begin());
        s.input->setValue(s.value, queue_.now(), queue_);
        any = true;
    }
    return any;
}

void Simulator::dispatchChanges_()
{
    if (change_log_.empty()) return;

    // Settle the bookkeeping for every net before calling anyone, so a
    // throwing listener cannot leave a net stuck as "already logged".
    dispatch_scratch_.clear();
    for (Net* n : change_log_)
    {
        n->logged_ = false;
        if (n->value_ == n->reported_) continue;   // glitched and came back
        dispatch_scratch_.emplace_back(n, n->reported_);
        n->reported_ = n->value_;
    }
    change_log_.clear();

    const uint64_t t = queue_.now();
    for (const auto& change : dispatch_scratch_)
        for (const auto& listener : listeners_)
            listener.second(*change.first, change.second, t);
}

std::optional<uint64_t> Simulator::nextActivity_() const
{
    std::optional<uint64_t> next = queue_.nextEventTime();
    if (!stimuli_.empty() && (!next || stimuli_.begin()->first < *next))
        next = stimuli_.begin()->first;
    return next;
}

Simulator::Result Simulator::run(uint64_t until_time)
{
    resync_();
    if (until_time < queue_.now())
        return idle() ? Result::Idle : Result::Ok;

    for (;;)
    {
        // Stop at the next stimulus too, so it is applied at its exact time.
        uint64_t limit = until_time;
        if (!stimuli_.empty() && stimuli_.begin()->first < limit)
            limit = stimuli_.begin()->first;

        queue_.advance(limit);
        applyDueStimuli_();

        const bool settled = queue_.settle();
        dispatchChanges_();
        if (!settled) return Result::Oscillation;

        if (queue_.now() >= until_time) break;
    }
    return idle() ? Result::Idle : Result::Ok;
}

Simulator::Result Simulator::runFor(uint64_t ticks)
{
    const uint64_t t = queue_.now();
    const uint64_t max = std::numeric_limits<uint64_t>::max();
    return run(ticks > max - t ? max : t + ticks);
}

Simulator::Result Simulator::step()
{
    resync_();
    const std::optional<uint64_t> next = nextActivity_();
    if (!next) return Result::Idle;
    return run(*next);
}

Simulator::Result Simulator::runUntilIdle(uint64_t max_ticks)
{
    resync_();
    const uint64_t t = queue_.now();
    const uint64_t max = std::numeric_limits<uint64_t>::max();
    const uint64_t deadline = max_ticks > max - t ? max : t + max_ticks;

    for (;;)
    {
        const std::optional<uint64_t> next = nextActivity_();
        if (!next) return Result::Idle;
        if (*next > deadline) return Result::Ok;
        if (run(*next) == Result::Oscillation) return Result::Oscillation;
    }
}

void Simulator::reset()
{
    queue_.reset();
    stimuli_.clear();
    change_log_.clear();

    for (const auto& n : nets_)
        if (n) n->resetState();
    for (const auto& g : gates_)
        if (g) g->resetState();

    dirty_ = true;
}

// --- Observation ------------------------------------------------------------

Simulator::ListenerId Simulator::addChangeListener(ChangeListener fn)
{
    const ListenerId id = next_listener_++;
    listeners_.emplace_back(id, std::move(fn));
    return id;
}

void Simulator::removeChangeListener(ListenerId id)
{
    listeners_.erase(std::remove_if(listeners_.begin(), listeners_.end(),
                                    [id](const auto& l) { return l.first == id; }),
                     listeners_.end());
}
