/**
 * @file Gate.h
 * @author Sebastian Paulus
 * @date 2026/09/13
 * @brief Abstract base class for logic gates, plus timing models.
 */

#ifndef GATE_H
#define GATE_H

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>
#include "Event.h"
#include "EventQueue.h"
#include "Types.h"
#include "Net.h"

class Simulator;

/**
 * @class Gate
 * @brief Abstract base class for every component: gates, flip-flops,
 *        sources and the multi-bit primitives.
 *
 * Subclasses implement the logic function (computeOutputs()) and declare
 * their intrinsic complexity (intrinsicStages()). Everything else, from
 * scheduling output changes to driving nets, is handled here, the same way
 * for every component.
 *
 * Pins:
 *  - Any pin may be left unconnected by passing nullptr. An unconnected input
 *    reads as HIGHZ; an unconnected output simply drives nothing.
 *  - Pins can be rewired later with connectInput() / connectOutput().
 *
 * Timing:
 *  - Each gate has rise, fall and decay (to HIGHZ) delays, set by the
 *    Simulator's TimingModel unless pinned with setFixedDelays().
 *  - Output changes use inertial delay: an output that is rescheduled before
 *    its pending change fires drops that change, so pulses shorter than the
 *    gate's delay are swallowed.
 *
 * @code
 *   // A minimal custom component: drives the majority of three inputs.
 *   struct Majority : Gate
 *   {
 *       Majority(std::vector<Net*> in, std::vector<Net*> out, std::string n)
 *           : Gate(std::move(in), std::move(out), std::move(n)) {}
 *       void computeOutputs(std::vector<LogicValue>& out) override
 *       {
 *           const LogicValue a = asInput(readInput_(0));
 *           const LogicValue b = asInput(readInput_(1));
 *           const LogicValue c = asInput(readInput_(2));
 *           out[0] = (a & b) | (a & c) | (b & c);
 *       }
 *       uint32_t intrinsicStages() const override { return 2; }
 *       const char* typeName() const override { return "maj"; }
 *   };
 *   sim.emplace<Majority>(std::vector<Net*>{&a, &b, &c},
 *                         std::vector<Net*>{&y}, "maj0");
 * @endcode
 */
class Gate
{
    /// Assigns ids and names, and rewires pins during edits.
    friend class Simulator;

protected:
    /// Nets driving this component, one per input pin (may be null).
    std::vector<Net*> inputs_;

    /// Nets driven by this component, one per output pin (may be null).
    std::vector<Net*> outputs_;

    /// Per output, this gate's index in that net's driver list.
    std::vector<std::size_t> driver_ids_;

    std::string name_;  ///< Descriptive name, diagnostics only.
    Strength strength_; ///< How hard every output drives its net.

    uint32_t rise_delay_ = 0; ///< Ticks to drive an output to HIGH.
    uint32_t fall_delay_ = 0; ///< Ticks to drive an output to LOW.
    uint32_t decay_delay_ = 0; ///< Ticks to fade to HIGHZ once undriven.
    bool fixed_delays_ = false; ///< Set by hand; TimingModels leave it alone.

    /// Per-output counter, bumped on every schedule. A fired event whose serial
    /// no longer matches has been superseded and is dropped.
    std::vector<uint32_t> pending_serial_;

    /// Per-output value this component is currently heading toward.
    std::vector<LogicValue> target_;

    /// Per-output value this component last committed (is driving now).
    std::vector<LogicValue> current_;

    /**
     * @brief Schedules output @p pin to become @p v at @p time, superseding
     *        anything already pending for that output.
     *
     * @param pin  Output pin.
     * @param v    Value it becomes.
     * @param time Absolute time of the change.
     * @param eq   Queue to schedule into.
     */
    void scheduleOutput_(std::size_t pin,
                         LogicValue v,
                         uint64_t time,
                         EventQueue& eq);

    /// @brief Drops whatever is pending for an output; it stays at its
    ///        current value.
    /// @param pin Output pin.
    void cancelPending_(std::size_t pin);

    /**
     * @brief Called after an output event is committed (e.g. a Clock schedules
     *        its next edge here). Does nothing by default.
     *
     * @param pin Output pin that changed.
     * @param v   Value it now drives.
     * @param now Current simulation time.
     * @param eq  Queue to schedule follow-up events into.
     */
    virtual void onCommitted(std::size_t pin,
                             LogicValue v,
                             uint64_t now,
                             EventQueue& eq)
    {
        (void)pin;
        (void)v;
        (void)now;
        (void)eq;
    }

    /// Called by resetState(); subclasses restore their power-on state here.
    virtual void onReset() {}

    /**
     * @brief Unchecked inputValue() for use inside computeOutputs().
     * @param pin Input pin; must exist.
     * @return The value on that pin, HIGHZ if unconnected.
     */
    LogicValue readInput_(std::size_t pin) const
    {
        const Net* n = inputs_[pin];
        return n ? n->getValue() : LogicValue::HIGHZ;
    }

private:
    uint32_t id_ = kNoId; ///< Assigned by the owning Simulator.
    std::vector<LogicValue> next_; ///< Scratch for evaluate().

public:
    /// id() of a gate that no Simulator owns.
    static constexpr uint32_t kNoId = UINT32_MAX;

    /**
     * @brief Constructs a gate and wires it into the netlist.
     *
     * Registers itself as a sink on every connected input and as a driver on
     * every connected output, capturing the driver handles.
     *
     * @param inputs   One net per input pin; nullptr leaves a pin unconnected.
     * @param outputs  One net per output pin; nullptr leaves a pin unconnected.
     * @param name     Descriptive name (diagnostics, GUI, VCD output).
     * @param strength How hard every output drives its net.
     *
     * @throws std::invalid_argument if there are more than 65535 outputs.
     */
    Gate(std::vector<Net*> inputs,
         std::vector<Net*> outputs,
         std::string name,
         Strength strength = Strength::Strong);

    /// Unregisters from every net, so the nets must still be alive.
    virtual ~Gate();

    /// Not copyable: nets hold pointers to their gates.
    Gate(const Gate&) = delete;

    /// Not copyable: nets hold pointers to their gates.
    Gate& operator=(const Gate&) = delete;

    // --- Subclass responsibilities ---------------------------------------

    /**
     * @brief Logic function. Reads the inputs, writes one value per output.
     *
     * Combinational gates must be pure. Sequential gates may update their
     * internal state here (edge detection); evaluate() may call this more
     * than once per input change, so that update must be idempotent.
     *
     * @param out One value per output, written in pin order.
     */
    virtual void computeOutputs(std::vector<LogicValue>& out) = 0;

    /**
     * @brief Intrinsic CMOS stage count, consumed by the TimingModel.
     * @return Stages on the slowest path (NAND = 1, AND = 2, XOR = 4).
     */
    virtual uint32_t intrinsicStages() const = 0;

    /// @brief Type name used in diagnostics, generated gate names and the GUI.
    /// @return A short lowercase name ("nand", "dff", "input", ...).
    virtual const char* typeName() const = 0;

    /// @brief Region this gate's output events go into. Sequential elements
    ///        use Update so every reader of the old state samples before it
    ///        changes.
    /// @return Region::Active unless a subclass overrides it.
    virtual Region outputRegion() const { return Region::Active; }

    /// @brief Tells whether the TimingModel sets this gate's delays.
    /// @return false for stimulus sources (inputs, clocks, pullups), which
    ///         have no propagation delay of their own; true otherwise.
    virtual bool usesTimingModel() const { return true; }

    // --- Kernel-facing behaviour (non-virtual: one policy for all gates) ---

    /**
     * @brief Called when any input net changes. Recomputes outputs and
     *        schedules those heading somewhere new.
     *
     * @param now Current simulation time.
     * @param eq  Queue to schedule output changes into.
     */
    void evaluate(uint64_t now, EventQueue& eq);

    /**
     * @brief Called when a scheduled output event fires. Drops the event if a
     *        newer one superseded it, otherwise drives the net.
     *
     * @param out_index Output pin the event is for.
     * @param value     Value the output takes.
     * @param serial    Serial the event was scheduled with.
     * @param now       Current simulation time.
     * @param eq        Queue the woken gates schedule into.
     */
    void commit(uint16_t out_index,
                LogicValue value,
                uint32_t serial,
                uint64_t now,
                EventQueue& eq);

    /// Back to power-on: nothing driven, nothing pending, subclass state reset.
    void resetState();

    // --- Timing ----------------------------------------------------------

    /**
     * @brief Delay for a transition ending at @p to.
     * @param to The value the output is changing to.
     * @return Rise delay for HIGH, fall for LOW, decay for HIGHZ, and the
     *         smallest of the three for UNKNOWN.
     */
    uint32_t delayFor(LogicValue to) const;

    /**
     * @brief Sets all three delays.
     * @param rise  Ticks to drive an output to HIGH.
     * @param fall  Ticks to drive an output to LOW.
     * @param decay Ticks to fade to HIGHZ.
     */
    void setDelays(uint32_t rise, uint32_t fall, uint32_t decay)
    {
        rise_delay_ = rise;
        fall_delay_ = fall;
        decay_delay_ = decay;
    }

    /**
     * @brief Sets rise and fall delays; decay is the smaller of the two
     *        (Verilog's convention).
     * @param rise Ticks to drive an output to HIGH.
     * @param fall Ticks to drive an output to LOW.
     */
    void setDelays(uint32_t rise, uint32_t fall)
    {
        setDelays(rise, fall, (rise < fall ? rise : fall));
    }

    /// @brief Sets every delay to the same value.
    /// @param d Ticks for any transition.
    void setDelay(uint32_t d) { setDelays(d, d, d); }

    /**
     * @brief Sets delays by hand and pins them: TimingModels skip this gate.
     * @param rise  Ticks to drive an output to HIGH.
     * @param fall  Ticks to drive an output to LOW.
     * @param decay Ticks to fade to HIGHZ.
     */
    void setFixedDelays(uint32_t rise, uint32_t fall, uint32_t decay)
    {
        setDelays(rise, fall, decay);
        fixed_delays_ = true;
    }

    /// Lets the TimingModel manage this gate's delays again.
    void clearFixedDelays() { fixed_delays_ = false; }

    /// @brief Tells whether setFixedDelays() pinned the delays.
    /// @return true if TimingModels skip this gate.
    bool hasFixedDelays() const { return fixed_delays_; }

    /// @brief Ticks to drive an output to HIGH.
    /// @return The rise delay.
    uint32_t riseDelay()  const { return rise_delay_; }

    /// @brief Ticks to drive an output to LOW.
    /// @return The fall delay.
    uint32_t fallDelay()  const { return fall_delay_; }

    /// @brief Ticks to fade to HIGHZ once undriven.
    /// @return The decay delay.
    uint32_t decayDelay() const { return decay_delay_; }

    /// @brief Largest fanout across this component's outputs, for load-scaled
    ///        delay.
    /// @return The most input pins any one output net feeds.
    std::size_t maxFanout() const;

    // --- Wiring ----------------------------------------------------------

    /**
     * @brief Re-points input @p pin at @p net (nullptr disconnects it).
     *
     * Only updates the wiring; the Simulator re-evaluates afterwards.
     *
     * @param pin Input pin.
     * @param net New net, or nullptr to disconnect.
     * @throws std::out_of_range if @p pin does not exist.
     */
    void connectInput(std::size_t pin, Net* net);

    /**
     * @brief Re-points output @p pin at @p net (nullptr disconnects it).
     *
     * The new net immediately sees whatever this output is driving.
     *
     * @param pin Output pin.
     * @param net New net, or nullptr to disconnect.
     * @throws std::out_of_range if @p pin does not exist.
     */
    void connectOutput(std::size_t pin, Net* net);

    /// @brief Disconnects every pin attached to @p net.
    /// @param net The net to detach from.
    void disconnect(const Net* net);

    // --- Accessors -------------------------------------------------------

    /// @brief Stable id assigned by the owning Simulator.
    /// @return The id, or kNoId if no Simulator owns this gate.
    uint32_t id() const { return id_; }

    /// @brief Descriptive name (diagnostics, GUI, VCD output).
    /// @return The name.
    const std::string& getName() const { return name_; }

    /// @brief Nets on the input pins.
    /// @return One entry per input pin; nullptr where unconnected.
    const std::vector<Net*>& getInputs() const { return inputs_; }

    /// @brief Nets on the output pins.
    /// @return One entry per output pin; nullptr where unconnected.
    const std::vector<Net*>& getOutputs() const { return outputs_; }

    /// @brief Number of input pins, connected or not.
    /// @return The input pin count.
    std::size_t numInputs() const { return inputs_.size(); }

    /// @brief Number of output pins, connected or not.
    /// @return The output pin count.
    std::size_t numOutputs() const { return outputs_.size(); }

    /// @brief How hard every output drives its net.
    /// @return Strength::Strong, or Strength::Pull for pullup / pulldown.
    Strength strength() const { return strength_; }

    /**
     * @brief Value on an input pin.
     * @param pin Input pin.
     * @return The net's value, or HIGHZ if the pin is unconnected.
     * @throws std::out_of_range if @p pin does not exist.
     */
    LogicValue inputValue(std::size_t pin) const
    {
        const Net* n = inputs_.at(pin);
        return n ? n->getValue() : LogicValue::HIGHZ;
    }

    /**
     * @brief Value an output pin is driving right now.
     * @param pin Output pin.
     * @return The committed value, HIGHZ before the first commit.
     * @throws std::out_of_range if @p pin does not exist.
     */
    LogicValue outputValue(std::size_t pin) const { return current_.at(pin); }

    /// @brief Renames the gate.
    /// @param name The new name.
    void setName(std::string name) { name_ = std::move(name); }
};


/**
 * @struct TimingModel
 * @brief Sets a gate's delays; the Simulator applies it to every gate that
 *        uses one (see Gate::usesTimingModel()).
 *
 * It is re-applied after every edit, so models that depend on the wiring
 * (like StageDelayModel's fanout term) stay correct.
 */
struct TimingModel
{
    virtual ~TimingModel() = default;

    /// @brief Sets the delays of one gate.
    /// @param c The gate.
    virtual void apply(Gate& c) const = 0;
};

/**
 * @struct ZeroDelayModel
 * @brief Everything settles in zero time; ordering falls to delta cycles.
 */
struct ZeroDelayModel : TimingModel
{
    /// @brief Sets every delay of @p c to 0.
    /// @param c The gate.
    void apply(Gate& c) const override { c.setDelay(0); }
};

/**
 * @struct UnitDelayModel
 * @brief Every gate costs one tick. The Simulator's default.
 */
struct UnitDelayModel : TimingModel
{
    /// @brief Sets every delay of @p c to 1.
    /// @param c The gate.
    void apply(Gate& c) const override { c.setDelay(1); }
};

/**
 * @struct StageDelayModel
 * @brief Stage count scaled, plus a load term proportional to fanout.
 *
 * Rise delay = intrinsicStages() * ticks_per_stage + maxFanout() *
 * ticks_per_load; fall delay = rise * fall_ratio. Both are at least 1.
 */
struct StageDelayModel : TimingModel
{
    uint32_t ticks_per_stage = 1; ///< Ticks per CMOS stage.
    uint32_t ticks_per_load = 1;  ///< Ticks per input pin driven.
    double fall_ratio = 1.0;      ///< Fall delay relative to rise delay.

    /// @brief Sets the delays of @p c from its stages and fanout.
    /// @param c The gate.
    void apply(Gate& c) const override
    {
        const uint64_t wide = uint64_t{c.intrinsicStages()} * ticks_per_stage
                            + uint64_t{c.maxFanout()} * ticks_per_load;
        const uint32_t base =
            static_cast<uint32_t>(wide > UINT32_MAX ? UINT32_MAX : wide);
        const double ratio = fall_ratio > 0.0 ? fall_ratio : 0.0;
        const double scaled = std::round(base * ratio);
        const uint32_t fall = static_cast<uint32_t>(
            scaled > double(UINT32_MAX) ? double(UINT32_MAX) : scaled);
        c.setDelays(base ? base : 1, fall ? fall : 1);
    }
};

#endif
