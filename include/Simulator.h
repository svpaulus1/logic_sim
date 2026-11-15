/**
 * @file Simulator.h
 * @author Sebastian Paulus
 * @date 2026/09/24
 * @brief Top-level object: owns the netlist, the event queue and the clock of
 *        simulated time.
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
#include "Adder.h"
#include "BuiltinGate.h"
#include "Clock.h"
#include "EventQueue.h"
#include "FlipFlop.h"
#include "Gate.h"
#include "Input.h"
#include "Latch.h"
#include "Module.h"
#include "Mux.h"
#include "Net.h"
#include "Register.h"
#include "Shifter.h"
#include "Types.h"

/**
 * @class Simulator
 * @brief Owns every Net and Gate of one circuit and runs it.
 *
 * Ownership:
 *  - Nets and gates are created through the Simulator, which keeps them
 *    alive until they are removed or the Simulator is destroyed. References
 *    and pointers to them stay valid until then.
 *  - Each also gets a numeric id that is never reused, which is the handle
 *    to hand across an API boundary.
 *  - Every Net* passed to a Simulator function must belong to that
 *    Simulator; nullptr means unconnected.
 *
 * Editing:
 *  - The netlist can be edited at any time, including mid-simulation.
 *  - Edits only mark the circuit dirty. Before simulated time moves again
 *    the Simulator re-applies the timing model, re-resolves every net and
 *    re-evaluates every gate at the current time, so the circuit picks up
 *    from where it was.
 *
 * Running:
 *  - Nothing needs to be called to start: the first run()/step()
 *    initialises everything at time 0.
 *  - Change listeners hear about settled net values after each timestep,
 *    which is how a GUI or a VcdWriter follows the circuit.
 *
 * @code
 *   Simulator sim;
 *   Net& a = sim.addNet("a");
 *   Net& b = sim.addNet("b");
 *   Net& y = sim.addNet("y");
 *   Input& in_a = sim.addInput(&a, "in_a");
 *   Input& in_b = sim.addInput(&b, "in_b");
 *   sim.addGate(BuiltinGate::Type::AND, {&a, &b}, {&y});
 *
 *   sim.setInput(in_a, LogicValue::HIGH);
 *   sim.setInput(in_b, LogicValue::HIGH);
 *   sim.runUntilIdle();            // y.getValue() == LogicValue::HIGH
 * @endcode
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

    /// Called once per net whose settled value changed during a timestep,
    /// with the value listeners last saw and the current time.
    using ChangeListener =
        std::function<void(const Net& net, LogicValue old_value, uint64_t time)>;

    /// Handle returned by addChangeListener(); never 0.
    using ListenerId = std::size_t;

    /// What instantiate() created, by id, so the instance can be removed.
    struct Instance
    {
        std::string name;               ///< Prefix of every created name.
        std::vector<uint32_t> gate_ids; ///< Every gate, nested ones included.
        std::vector<uint32_t> net_ids;  ///< Nets private to the instance.
    };

    /**
     * @brief Creates an empty circuit at time 0, using UnitDelayModel.
     * @param wheel_size Event wheel size, a power of two (see EventQueue).
     */
    explicit Simulator(std::size_t wheel_size = 1024);

    /// Destroys every gate, then every net.
    ~Simulator();

    /// Not copyable: gates and nets point back into it.
    Simulator(const Simulator&) = delete;

    /// Not copyable: gates and nets point back into it.
    Simulator& operator=(const Simulator&) = delete;

    // --- Building the circuit --------------------------------------------
    // An empty name is replaced by a generated one ("n7", "and12", ...).

    /**
     * @brief Adds an unconnected net.
     * @param name Net name; generated ("n7") if empty.
     * @return The new net, owned by this Simulator.
     */
    Net& addNet(std::string name = "");

    /**
     * @brief Adds a built-in gate (see BuiltinGate for the pin counts).
     *
     * @param type    Which gate.
     * @param inputs  Input pins.
     * @param outputs Output pins.
     * @param name    Instance name; generated ("and7") if empty.
     * @return The new gate, owned by this Simulator.
     * @throws std::invalid_argument on bad pin counts or a foreign net.
     */
    BuiltinGate& addGate(BuiltinGate::Type type,
                         std::vector<Net*> inputs,
                         std::vector<Net*> outputs,
                         std::string name = "");

    /**
     * @brief Adds an edge-triggered flip-flop (see FlipFlop for the pin
     *        layout).
     *
     * @param type    Which flip-flop.
     * @param inputs  Input pins; trailing ones may be omitted.
     * @param outputs {Q, QN}; trailing ones may be omitted.
     * @param name    Instance name; generated ("dff7") if empty.
     * @param edge    Clock edge that loads the flip-flop.
     * @return The new flip-flop, owned by this Simulator.
     * @throws std::invalid_argument on too many pins or a foreign net.
     */
    FlipFlop& addFlipFlop(FlipFlop::Type type,
                          std::vector<Net*> inputs,
                          std::vector<Net*> outputs,
                          std::string name = "",
                          FlipFlop::Edge edge = FlipFlop::Edge::Rising);

    /**
     * @brief Adds a gated D latch: inputs {D, EN}, outputs {Q, QN}.
     *
     * @param inputs      {D, EN}; trailing ones may be omitted.
     * @param outputs     {Q, QN}; trailing ones may be omitted.
     * @param name        Instance name; generated ("dlatch7") if empty.
     * @param active_high true: transparent while EN is HIGH; false: while LOW.
     * @return The new latch, owned by this Simulator.
     * @throws std::invalid_argument on too many pins or a foreign net.
     */
    Latch& addLatch(std::vector<Net*> inputs,
                    std::vector<Net*> outputs,
                    std::string name = "",
                    bool active_high = true);

    /**
     * @brief Adds an N-bit adder.
     *
     * Pins, least significant bit first: inputs {A0..A(n-1), B0..B(n-1), CIN},
     * outputs {S0..S(n-1), COUT}. See Adder.
     *
     * @param inputs  Input pins; trailing ones may be omitted.
     * @param outputs Output pins; trailing ones may be omitted.
     * @param size    Bits per operand, at least 1.
     * @param name    Instance name; generated ("adder7") if empty.
     * @return The new adder, owned by this Simulator.
     * @throws std::invalid_argument on a bad size, too many pins or a foreign
     *         net.
     */
    Adder& addAdder(std::vector<Net*> inputs,
                    std::vector<Net*> outputs,
                    std::size_t size,
                    std::string name = "");

    /**
     * @brief Adds a multiplexer choosing between @p width inputs of
     *        @p bit_width bits each.
     *
     * Pins, least significant bit first: inputs {D0[..], D1[..], ...,
     * S0..S(k-1)}, outputs {Y0..Y(b-1)}. See Mux.
     *
     * @param inputs    Data pins, then select pins; trailing ones may be
     *                  omitted.
     * @param outputs   Output pins; trailing ones may be omitted.
     * @param width     Number of data inputs, at least 2.
     * @param bit_width Bits per data input and per output, at least 1.
     * @param name      Instance name; generated ("mux7") if empty.
     * @return The new mux, owned by this Simulator.
     * @throws std::invalid_argument on bad sizes, too many pins or a foreign
     *         net.
     */
    Mux& addMux(std::vector<Net*> inputs,
                std::vector<Net*> outputs,
                std::size_t width,
                std::size_t bit_width,
                std::string name = "");

    /**
     * @brief Adds a barrel shifter.
     *
     * Pins, least significant bit first: inputs {D0..D(n-1), S0..S(k-1)},
     * outputs {Y0..Y(n-1)}. See Shifter.
     *
     * @param type    Which shift or rotate to perform.
     * @param inputs  Data pins, then shift-amount pins; trailing ones may be
     *                omitted.
     * @param outputs Output pins; trailing ones may be omitted.
     * @param size    Bits in the value being shifted, at least 2.
     * @param name    Instance name; generated ("sra7") if empty.
     * @return The new shifter, owned by this Simulator.
     * @throws std::invalid_argument on a bad size, too many pins or a foreign
     *         net.
     */
    Shifter& addShifter(Shifter::Type type,
                        std::vector<Net*> inputs,
                        std::vector<Net*> outputs,
                        std::size_t size,
                        std::string name = "");

    /**
     * @brief Adds an N-bit register.
     *
     * Pins, least significant bit first: inputs {D0..D(n-1), CLK, EN, RESET,
     * OE}, outputs {Q0..Q(n-1)}. See Register.
     *
     * @param inputs  Data pins, then CLK, EN, RESET, OE; trailing ones may be
     *                omitted.
     * @param outputs Output pins; trailing ones may be omitted.
     * @param size    Bits stored, at least 1.
     * @param name    Instance name; generated ("register7") if empty.
     * @param edge    Clock edge that loads the register.
     * @return The new register, owned by this Simulator.
     * @throws std::invalid_argument on a bad size, too many pins or a foreign
     *         net.
     */
    Register& addRegister(std::vector<Net*> inputs,
                          std::vector<Net*> outputs,
                          std::size_t size,
                          std::string name = "",
                          Register::Edge edge = Register::Edge::Rising);

    /**
     * @brief Adds an Input: a source whose value is changed with setInput()
     *        or scheduleInput().
     *
     * @param output  Net to drive; nullptr leaves it unconnected.
     * @param name    Instance name; generated ("input7") if empty.
     * @param initial Value driven from the start.
     * @return The new input, owned by this Simulator.
     * @throws std::invalid_argument if @p output is a foreign net.
     */
    Input& addInput(Net* output,
                    std::string name = "",
                    LogicValue initial = LogicValue::LOW);

    /**
     * @brief Adds a free-running Clock.
     *
     * @param output     Net to drive; nullptr leaves it unconnected.
     * @param name       Instance name; generated ("clock7") if empty.
     * @param high_ticks Ticks spent HIGH per period (>= 1).
     * @param low_ticks  Ticks spent LOW per period; 0 = same as high.
     * @param initial    Level at time 0, LOW or HIGH.
     * @return The new clock, owned by this Simulator.
     * @throws std::invalid_argument if @p high_ticks is 0, @p initial is not
     *         LOW or HIGH, or @p output is a foreign net.
     */
    Clock& addClock(Net* output,
                    std::string name,
                    uint64_t high_ticks,
                    uint64_t low_ticks = 0,
                    LogicValue initial = LogicValue::LOW);

    /**
     * @brief Constructs any Gate subclass in place and adopts it.
     *
     * @tparam T    The gate class; must derive from Gate.
     * @tparam Args Constructor argument types.
     * @param args  Forwarded to the constructor of @p T.
     * @return The new gate, owned by this Simulator.
     * @throws std::invalid_argument if it is wired to a foreign net, plus
     *         whatever the constructor of @p T throws.
     *
     * @code
     *   // Majority as defined in the Gate example.
     *   Majority& maj = sim.emplace<Majority>(ins, outs, "maj0");
     * @endcode
     */
    template <class T, class... Args>
    T& emplace(Args&&... args)
    {
        static_assert(std::is_base_of<Gate, T>::value, "T must derive from Gate");
        auto gate = std::make_unique<T>(std::forward<Args>(args)...);
        T& ref = *gate;
        adopt(std::move(gate));
        return ref;
    }

    /**
     * @brief Takes ownership of an already-built gate.
     *
     * An empty name is replaced by "<type name><id>", and the timing model
     * is applied straight away.
     *
     * @param gate The gate; every pin must be a net of this Simulator.
     * @return The gate, now owned by this Simulator.
     * @throws std::invalid_argument if it is null, already belongs to a
     *         Simulator or is wired to a foreign net.
     */
    Gate& adopt(std::unique_ptr<Gate> gate);

    /**
     * @brief Flattens @p module into this circuit.
     *
     * Every gate and private net created is named "<instance_name>.<item>",
     * each nested instance adding a level ("fa0.ha1.sum"). An unconnected
     * port still gets a private net, so the gates inside stay joined.
     *
     * @param module        The blueprint.
     * @param inputs        One net per input port, in order (nullptr =
     *                      unconnected).
     * @param outputs       One net per output port, in order (nullptr =
     *                      unconnected).
     * @param instance_name Prefix for every created name; generated
     *                      ("<module name><n>") if empty.
     * @return What was created, for removeInstance().
     * @throws std::invalid_argument if the port counts do not match or a net
     *         is foreign; std::runtime_error if modules nest more than 64
     *         deep.
     */
    Instance instantiate(const Module& module,
                         std::vector<Net*> inputs,
                         std::vector<Net*> outputs,
                         std::string instance_name = "");

    // --- Editing ---------------------------------------------------------

    /**
     * @brief Destroys @p gate. Its pending events and scheduled stimulus go
     *        too.
     * @param gate A gate of this Simulator; invalid afterwards.
     * @throws std::invalid_argument if it belongs to another Simulator.
     */
    void removeGate(Gate& gate);

    /**
     * @brief Disconnects every pin on @p net, then destroys it.
     * @param net A net of this Simulator; invalid afterwards.
     * @throws std::invalid_argument if it belongs to another Simulator.
     */
    void removeNet(Net& net);

    /**
     * @brief Removes whatever of @p instance still exists.
     * @param instance What instantiate() returned; parts already removed are
     *                 skipped.
     */
    void removeInstance(const Instance& instance);

    /**
     * @brief Re-points one input pin.
     *
     * @param gate A gate of this Simulator.
     * @param pin  Input pin index.
     * @param net  The new net; nullptr disconnects the pin.
     * @throws std::invalid_argument if @p gate or @p net is foreign.
     * @throws std::out_of_range if @p pin does not exist.
     */
    void connectInput(Gate& gate, std::size_t pin, Net* net);

    /**
     * @brief Re-points one output pin.
     *
     * @param gate A gate of this Simulator.
     * @param pin  Output pin index.
     * @param net  The new net; nullptr disconnects the pin.
     * @throws std::invalid_argument if @p gate or @p net is foreign.
     * @throws std::out_of_range if @p pin does not exist.
     */
    void connectOutput(Gate& gate, std::size_t pin, Net* net);

    /**
     * @brief Moves every connection of @p absorb onto @p keep, then removes
     *        @p absorb (e.g. when a GUI wire joins two existing wires).
     *
     * @param keep   The net that survives.
     * @param absorb The net that goes; invalid afterwards. Merging a net with
     *               itself does nothing.
     * @return @p keep.
     * @throws std::invalid_argument if either net is foreign.
     */
    Net& mergeNets(Net& keep, Net& absorb);

    // --- Lookup ----------------------------------------------------------

    /// @brief Looks up a net by id.
    /// @param id The net's id.
    /// @return The net, or nullptr if there is none (or it was removed).
    Net* net(uint32_t id) const;

    /// @brief Looks up a gate by id.
    /// @param id The gate's id.
    /// @return The gate, or nullptr if there is none (or it was removed).
    Gate* gate(uint32_t id) const;

    /// @brief Finds a net by name. Linear time.
    /// @param name The name to look for.
    /// @return The first net with that name, or nullptr.
    Net* findNet(std::string_view name) const;

    /// @brief Finds a gate by name. Linear time.
    /// @param name The name to look for.
    /// @return The first gate with that name, or nullptr.
    Gate* findGate(std::string_view name) const;

    /// @brief Every live net.
    /// @return The nets, in id order.
    std::vector<Net*> nets() const;

    /// @brief Every live gate.
    /// @return The gates, in id order.
    std::vector<Gate*> gates() const;

    /// @brief Number of live nets.
    /// @return Nets added and not yet removed.
    std::size_t netCount() const { return live_nets_; }

    /// @brief Number of live gates.
    /// @return Gates added and not yet removed.
    std::size_t gateCount() const { return live_gates_; }

    // --- Timing ----------------------------------------------------------

    /**
     * @brief Replaces the model that sets each gate's delays.
     *
     * The model is applied before time next moves, and again after every
     * edit (fanout-based models depend on the wiring). The default is
     * UnitDelayModel. Gates with Gate::setFixedDelays() and sources (Input,
     * Clock) are never touched.
     *
     * @param model The new model; nullptr leaves every gate's delay as it is.
     */
    void setTimingModel(std::unique_ptr<TimingModel> model);

    /// @brief The current timing model.
    /// @return The model, or nullptr if none is set.
    const TimingModel* timingModel() const { return timing_.get(); }

    // --- Stimulus --------------------------------------------------------

    /**
     * @brief Changes an Input at the current time.
     *
     * The new value reaches the net at the next run()/step().
     *
     * @param input An Input of this Simulator.
     * @param value The new value.
     * @throws std::invalid_argument if @p input is foreign.
     */
    void setInput(Input& input, LogicValue value);

    /**
     * @brief Changes an Input at a future time.
     *
     * @param input An Input of this Simulator.
     * @param value The new value.
     * @param time  When to apply it; now() is allowed.
     * @throws std::invalid_argument if @p input is foreign or @p time is
     *         before now().
     */
    void scheduleInput(Input& input, LogicValue value, uint64_t time);

    /**
     * @brief Pauses (holding the current level) or resumes a Clock.
     * @param clock   A Clock of this Simulator.
     * @param running true to resume, false to pause.
     * @throws std::invalid_argument if @p clock is foreign.
     */
    void setClockRunning(Clock& clock, bool running);

    /**
     * @brief Holds @p net at @p value regardless of its drivers, until
     *        releaseNet().
     * @param net   A net of this Simulator.
     * @param value The value to hold.
     * @throws std::invalid_argument if @p net is foreign.
     */
    void forceNet(Net& net, LogicValue value);

    /**
     * @brief Ends forceNet(): the net goes back to the value its drivers
     *        resolve to.
     * @param net A net of this Simulator.
     * @throws std::invalid_argument if @p net is foreign.
     */
    void releaseNet(Net& net);

    // --- Running ---------------------------------------------------------

    /// @brief Current simulation time.
    /// @return Ticks since time 0.
    uint64_t now() const { return queue_.now(); }

    /**
     * @brief Processes everything up to and including @p until_time.
     *
     * Afterwards now() == until_time, unless an oscillation stopped it early.
     * A time already in the past does nothing.
     *
     * @param until_time Last tick to process.
     * @return Idle if nothing is left pending, Ok if something is, or
     *         Oscillation.
     */
    Result run(uint64_t until_time);

    /**
     * @brief Runs @p ticks further: run(now() + ticks).
     * @param ticks How far to advance; clamped so time cannot overflow.
     * @return As run().
     */
    Result runFor(uint64_t ticks);

    /**
     * @brief Jumps to the next time anything happens and processes it.
     * @return Idle if there was nothing to do (time does not move),
     *         otherwise as run().
     */
    Result step();

    /**
     * @brief Runs until nothing is pending, for at most @p max_ticks.
     * @param max_ticks Most ticks to advance before giving up.
     * @return Idle once quiet, Ok if still busy at the limit (a running
     *         Clock never goes quiet), or Oscillation.
     */
    Result runUntilIdle(uint64_t max_ticks = 1000000);

    /// @brief Initialises (or re-syncs after edits) without moving time.
    ///        Optional: run() and friends do this on their own.
    void initialize() { resync_(); }

    /**
     * @brief Back to time 0 with every gate and net at power-on state.
     *
     * Keeps the netlist and each Input's value; drops pending events,
     * scheduled stimulus and forces. Listeners are not called for the reset
     * itself (a GUI should simply redraw).
     */
    void reset();

    /// @brief Tells whether there is anything left to simulate.
    /// @return true if no events are pending and no stimulus is scheduled.
    bool idle() const { return queue_.empty() && stimuli_.empty(); }

    /// @brief Tells whether the last run stopped on an oscillation.
    /// @return true if a timestep failed to settle.
    bool oscillationDetected() const { return queue_.oscillationDetected(); }

    /// @brief Gates still switching in the timestep that failed to settle,
    ///        which is what a GUI wants to highlight.
    /// @return Each gate once.
    std::vector<Gate*> oscillatingGates() const { return queue_.pendingGatesNow(); }

    /// @brief Delta cycles allowed per timestep before declaring an
    ///        oscillation.
    /// @param n The limit (10000 by default); 0 is treated as 1.
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
     *
     * @param fn The callback.
     * @return Handle for removeChangeListener().
     */
    ListenerId addChangeListener(ChangeListener fn);

    /// @brief Unregisters a listener. Unknown ids are ignored.
    /// @param id What addChangeListener() returned.
    void removeChangeListener(ListenerId id);

private:
    /// An Input change waiting in stimuli_ for its time.
    struct Stimulus
    {
        Input* input;     ///< Input to change.
        LogicValue value; ///< Value to set.
    };

    /// @brief Throws unless @p net belongs to this Simulator.
    /// @param net The net; nullptr (unconnected) always passes.
    /// @throws std::invalid_argument if it is foreign.
    void requireOwned_(const Net* net) const;

    /// @brief Throws unless @p gate belongs to this Simulator.
    /// @param gate The gate.
    /// @throws std::invalid_argument if it is foreign or was removed.
    void requireOwned_(const Gate& gate) const;

    /// @brief Brings a dirty netlist back in step at the current time:
    ///        re-applies the timing model, resolves every net, then
    ///        evaluates every gate.
    void resync_();

    /// @brief Applies every stimulus due at now().
    /// @return true if any was applied.
    bool applyDueStimuli_();

    /// @brief Reports settled changes to the listeners.
    void dispatchChanges_();

    /// @brief Earliest time with an event or a stimulus.
    /// @return That time, or nothing if there is neither.
    std::optional<uint64_t> nextActivity_() const;

    /**
     * @brief Recursive part of instantiate(): flattens one level.
     *
     * @param module  The blueprint.
     * @param inputs  Nets bound to its input ports.
     * @param outputs Nets bound to its output ports.
     * @param path    Hierarchical name of this level.
     * @param created Collects the ids of everything created.
     * @param depth   Nesting depth, to stop runaway recursion.
     * @throws std::runtime_error if @p depth passes 64.
     */
    void instantiate_(const Module& module,
                      const std::vector<Net*>& inputs,
                      const std::vector<Net*>& outputs,
                      const std::string& path,
                      Instance& created,
                      unsigned depth);

    // Destroyed in reverse order: gates (which unregister from their nets)
    // must go before the nets.
    std::vector<std::unique_ptr<Net>> nets_;   ///< By id; null if removed.
    std::vector<std::unique_ptr<Gate>> gates_; ///< By id; null if removed.
    std::size_t live_nets_ = 0;  ///< Non-null entries of nets_.
    std::size_t live_gates_ = 0; ///< Non-null entries of gates_.

    EventQueue queue_;                    ///< Pending events and the time.
    std::unique_ptr<TimingModel> timing_; ///< Current model; may be null.
    std::multimap<uint64_t, Stimulus> stimuli_; ///< Scheduled, by time.

    std::vector<Net*> change_log_;  ///< Nets changed since the last dispatch.
    /// Reused by dispatchChanges_() so it does not allocate every timestep.
    std::vector<std::pair<Net*, LogicValue>> dispatch_scratch_;
    /// Registered listeners with their ids, in registration order.
    std::vector<std::pair<ListenerId, ChangeListener>> listeners_;
    ListenerId next_listener_ = 1; ///< Id for the next listener.

    std::size_t instance_counter_ = 0; ///< Numbers generated instance names.
    bool dirty_ = true; ///< Netlist edited since the last resync_().
};

#endif
