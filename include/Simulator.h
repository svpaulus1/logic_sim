/**
 * @file Simulator.h
 * @date 2026/09/24
 * @brief Top-level object: owns the netlist, the event queue and the clock of
 *        simulated time. This is the class a GUI front end talks to.
 */

#ifndef SIMULATOR_H
#define SIMULATOR_H

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>
#include "BuiltinGate.h"
#include "Clock.h"
#include "EventQueue.h"
#include "FlipFlop.h"
#include "Gate.h"
#include "Input.h"
#include "Latch.h"
#include "Module.h"
#include "Net.h"
#include "Types.h"

/**
 * @class Simulator
 * @brief Owns every Net and Gate of one circuit and runs it.
 *
 * Nets and gates are created through the Simulator, which keeps them alive
 * until they are removed or the Simulator is destroyed. References and
 * pointers to them stay valid until then. Each also gets a numeric id that is
 * never reused, which is the handle to hand across an API boundary.
 *
 * The netlist can be edited at any time, including mid-simulation. Edits only
 * mark the circuit dirty; before simulated time moves again the Simulator
 * re-applies the timing model, re-resolves every net and re-evaluates every
 * gate at the current time, so the circuit picks up from where it was.
 *
 * Nothing needs to be called to start: the first run()/step() initialises
 * everything at time 0.
 */
class Simulator
{
public:
    /// Outcome of run(), step() and runUntilIdle().
    enum class Result
    {
        Ok,          ///< Reached the requested point; more events are pending.
        Idle,        ///< Nothing left to do: no events, no scheduled stimulus.
        Oscillation  ///< A timestep never settled; see oscillatingGates().
    };

    /// Called once per net whose settled value changed during a timestep.
    using ChangeListener =
        std::function<void(const Net& net, LogicValue old_value, uint64_t time)>;
    using ListenerId = std::size_t;

    /// What instantiate() created, by id, so the instance can be removed.
    struct Instance
    {
        std::string name;
        std::vector<uint32_t> gate_ids;
        std::vector<uint32_t> net_ids;  ///< Nets private to the instance.
    };

    /// @param wheel_size Event wheel size, a power of two (see EventQueue).
    explicit Simulator(std::size_t wheel_size = 1024);
    ~Simulator();

    Simulator(const Simulator&) = delete;
    Simulator& operator=(const Simulator&) = delete;

    // --- Building the circuit --------------------------------------------
    // An empty name is replaced by a generated one ("n7", "and12", ...).
    // Every Net* passed in must belong to this Simulator (nullptr = unconnected).

    Net& addNet(std::string name = "");

    /// @throws std::invalid_argument on bad pin counts or a foreign net.
    BuiltinGate& addGate(BuiltinGate::Type type,
                         std::vector<Net*> inputs,
                         std::vector<Net*> outputs,
                         std::string name = "");

    /// See FlipFlop for the pin layout.
    FlipFlop& addFlipFlop(FlipFlop::Type type,
                          std::vector<Net*> inputs,
                          std::vector<Net*> outputs,
                          std::string name = "",
                          FlipFlop::Edge edge = FlipFlop::Edge::Rising);

    /// Gated D latch: inputs {D, EN}, outputs {Q, QN}.
    Latch& addLatch(std::vector<Net*> inputs,
                    std::vector<Net*> outputs,
                    std::string name = "",
                    bool active_high = true);

    Input& addInput(Net* output,
                    std::string name = "",
                    LogicValue initial = LogicValue::LOW);

    Clock& addClock(Net* output,
                    std::string name,
                    uint64_t high_ticks,
                    uint64_t low_ticks = 0,
                    LogicValue initial = LogicValue::LOW);

    /// Constructs any Gate subclass in place: emplace<MyGate>(args...).
    template <class T, class... Args>
    T& emplace(Args&&... args)
    {
        static_assert(std::is_base_of<Gate, T>::value, "T must derive from Gate");
        auto gate = std::make_unique<T>(std::forward<Args>(args)...);
        T& ref = *gate;
        adopt(std::move(gate));
        return ref;
    }

    /// Takes ownership of an already-built gate.
    /// @throws std::invalid_argument if it is null or wired to a foreign net.
    Gate& adopt(std::unique_ptr<Gate> gate);

    /**
     * @brief Flattens @p module into this circuit.
     * @param inputs  One net per input port, in order (nullptr = unconnected).
     * @param outputs One net per output port, in order (nullptr = unconnected).
     * @param instance_name Prefix for every created name; generated if empty.
     * @throws std::invalid_argument if the port counts do not match, or
     *         std::runtime_error if modules nest more than 64 deep.
     */
    Instance instantiate(const Module& module,
                         std::vector<Net*> inputs,
                         std::vector<Net*> outputs,
                         std::string instance_name = "");

    // --- Editing ---------------------------------------------------------

    /// Destroys @p gate. Its pending events and scheduled stimulus go too.
    void removeGate(Gate& gate);

    /// Disconnects every pin on @p net, then destroys it.
    void removeNet(Net& net);

    /// Removes whatever of @p instance still exists.
    void removeInstance(const Instance& instance);

    /// Re-points one input pin (nullptr disconnects).
    void connectInput(Gate& gate, std::size_t pin, Net* net);

    /// Re-points one output pin (nullptr disconnects).
    void connectOutput(Gate& gate, std::size_t pin, Net* net);

    /// Moves every connection of @p absorb onto @p keep and removes @p absorb
    /// (e.g. when a GUI wire joins two existing wires).
    Net& mergeNets(Net& keep, Net& absorb);

    // --- Lookup ----------------------------------------------------------

    /// nullptr if no such id (or it was removed).
    Net* net(uint32_t id) const;
    Gate* gate(uint32_t id) const;

    /// First net / gate with this name, or nullptr. Linear time.
    Net* findNet(std::string_view name) const;
    Gate* findGate(std::string_view name) const;

    /// Every live net / gate, in id order.
    std::vector<Net*> nets() const;
    std::vector<Gate*> gates() const;

    std::size_t netCount() const { return live_nets_; }
    std::size_t gateCount() const { return live_gates_; }

    // --- Timing ----------------------------------------------------------

    /// Default: UnitDelayModel. nullptr leaves every gate's delay as it is.
    /// Gates with setFixedDelays() are never touched.
    void setTimingModel(std::unique_ptr<TimingModel> model);
    const TimingModel* timingModel() const { return timing_.get(); }

    // --- Stimulus --------------------------------------------------------

    /// Changes an Input at the current time (takes effect at the next run).
    void setInput(Input& input, LogicValue value);

    /// Changes an Input at a future time.
    /// @throws std::invalid_argument if @p time is before now().
    void scheduleInput(Input& input, LogicValue value, uint64_t time);

    void setClockRunning(Clock& clock, bool running);

    /// Holds @p net at @p value regardless of its drivers, until releaseNet().
    void forceNet(Net& net, LogicValue value);
    void releaseNet(Net& net);

    // --- Running ---------------------------------------------------------

    uint64_t now() const { return queue_.now(); }

    /// Processes everything up to and including @p until_time; afterwards
    /// now() == until_time unless an oscillation stopped it early.
    Result run(uint64_t until_time);

    /// run(now() + ticks).
    Result runFor(uint64_t ticks);

    /// Jumps to the next time anything happens and processes it.
    Result step();

    /**
     * @brief Runs until nothing is pending, for at most @p max_ticks.
     * @return Idle once quiet, Ok if still busy at the limit (a running
     *         Clock never goes quiet), or Oscillation.
     */
    Result runUntilIdle(uint64_t max_ticks = 1000000);

    /// Initialises (or re-syncs after edits) without moving time. Optional:
    /// run() and friends do this on their own.
    void initialize() { resync_(); }

    /**
     * @brief Back to time 0 with every gate and net at power-on state.
     *
     * Keeps the netlist and each Input's value; drops pending events,
     * scheduled stimulus and forces. Listeners are not called for the reset
     * itself (a GUI should simply redraw).
     */
    void reset();

    /// True if nothing is pending and no stimulus is scheduled.
    bool idle() const { return queue_.empty() && stimuli_.empty(); }

    bool oscillationDetected() const { return queue_.oscillationDetected(); }

    /// Gates still switching in the timestep that failed to settle.
    std::vector<Gate*> oscillatingGates() const { return queue_.pendingGatesNow(); }

    /// Delta cycles allowed per timestep before declaring an oscillation.
    void setMaxDeltaCycles(uint32_t n) { queue_.setMaxDeltaCycles(n); }

    // --- Observation -----------------------------------------------------

    /**
     * @brief Registers @p fn to hear about settled value changes.
     *
     * Called after each timestep for every net whose value differs from what
     * listeners last saw (glitches inside a timestep are filtered out).
     * Listeners may read anything and may call setInput()/scheduleInput(),
     * but must not add, remove or rewire nets or gates, reset(), or run the
     * simulation (run/step/...) from inside the callback.
     */
    ListenerId addChangeListener(ChangeListener fn);
    void removeChangeListener(ListenerId id);

private:
    struct Stimulus
    {
        Input* input;
        LogicValue value;
    };

    /// Throws unless @p net / @p gate belongs to this Simulator.
    void requireOwned_(const Net* net) const;
    void requireOwned_(const Gate& gate) const;

    /// Brings a dirty netlist back in step at the current time.
    void resync_();

    /// Applies every stimulus due at now(). Returns true if any was applied.
    bool applyDueStimuli_();

    /// Reports settled changes to the listeners.
    void dispatchChanges_();

    /// Earliest time with an event or a stimulus.
    std::optional<uint64_t> nextActivity_() const;

    void instantiate_(const Module& module,
                      const std::vector<Net*>& inputs,
                      const std::vector<Net*>& outputs,
                      const std::string& path,
                      Instance& created,
                      unsigned depth);

    // Destroyed in reverse order: gates (which unregister from their nets)
    // must go before the nets.
    std::vector<std::unique_ptr<Net>> nets_;   ///< Indexed by id; null = removed.
    std::vector<std::unique_ptr<Gate>> gates_; ///< Indexed by id; null = removed.
    std::size_t live_nets_ = 0;
    std::size_t live_gates_ = 0;

    EventQueue queue_;
    std::unique_ptr<TimingModel> timing_;
    std::multimap<uint64_t, Stimulus> stimuli_;

    std::vector<Net*> change_log_;  ///< Nets changed since the last dispatch.
    std::vector<std::pair<Net*, LogicValue>> dispatch_scratch_;
    std::vector<std::pair<ListenerId, ChangeListener>> listeners_;
    ListenerId next_listener_ = 1;

    std::size_t instance_counter_ = 0;
    bool dirty_ = true;
};

#endif
