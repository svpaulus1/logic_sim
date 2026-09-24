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
 * @brief Abstract base class representing a logic Gate.
 *
 * Subclasses implement the logic function (computeOutputs) and declare
 * their intrinsic complexity (intrinsicStages).
 *
 * Any pin may be left unconnected by passing nullptr. An unconnected input
 * reads as HIGHZ; an unconnected output simply drives nothing.
 *
 * Output changes use inertial delay: an output that is rescheduled before
 * its pending change fires drops that change, so pulses shorter than the
 * gate's delay are swallowed.
 */
class Gate
{
    friend class Simulator;

protected:
    std::vector<Net*> inputs_; ///< Nets driving this component (may be null).
    std::vector<Net*> outputs_; ///< Nets driven by this component (may be null).
    std::vector<std::size_t> driver_ids_; ///< Index of output's driver list.
    std::string name_; ///< Descriptive name, diagnostics only.
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
     */
    void scheduleOutput_(std::size_t pin, LogicValue v, uint64_t time, EventQueue& eq);

    /// Drops whatever is pending for output @p pin; it stays at its current value.
    void cancelPending_(std::size_t pin);

    /// Called after an output event is committed (e.g. a Clock schedules its
    /// next edge here).
    virtual void onCommitted(std::size_t pin, LogicValue v, uint64_t now, EventQueue& eq)
    {
        (void)pin; (void)v; (void)now; (void)eq;
    }

    /// Called by resetState(); subclasses restore their power-on state here.
    virtual void onReset() {}

    /// Unchecked inputValue() for use inside computeOutputs().
    LogicValue readInput_(std::size_t pin) const
    {
        const Net* n = inputs_[pin];
        return n ? n->getValue() : LogicValue::HIGHZ;
    }

private:
    uint32_t id_ = kNoId; ///< Assigned by the owning Simulator.
    std::vector<LogicValue> next_; ///< Scratch for evaluate(), no per-call allocation.

public:
    /// id() of a gate that no Simulator owns.
    static constexpr uint32_t kNoId = UINT32_MAX;

    /**
     * @brief Constructs a gate and wires it into the netlist.
     *
     * Registers itself as a sink on every connected input and as a driver on
     * every connected output, capturing the driver handles.
     *
     * @throws std::invalid_argument if there are more than 65535 outputs.
     */
    Gate(std::vector<Net*> inputs,
         std::vector<Net*> outputs,
         std::string name,
         Strength strength = Strength::Strong);

    /// Unregisters from every net, so the nets must still be alive.
    virtual ~Gate();

    Gate(const Gate&) = delete;
    Gate& operator=(const Gate&) = delete;

    // --- Subclass responsibilities ---------------------------------------

    /**
     * @brief Logic function. Reads the inputs, writes one value per output.
     *
     * Combinational gates must be pure. Sequential gates may update their
     * internal state here (edge detection); evaluate() may call this more
     * than once per input change, so that update must be idempotent.
     */
    virtual void computeOutputs(std::vector<LogicValue>& out) = 0;

    /**
     * @brief Intrinsic CMOS stage count, consumed by the TimingModel.
     */
    virtual uint32_t intrinsicStages() const = 0;

    /// Short lowercase type name ("nand", "dff", "input", ...).
    virtual const char* typeName() const = 0;

    /// Region this gate's output events go into. Sequential elements use
    /// Update so every reader of the old state samples before it changes.
    virtual Region outputRegion() const { return Region::Active; }

    /// False for stimulus sources (inputs, clocks, pullups), which have no
    /// propagation delay of their own.
    virtual bool usesTimingModel() const { return true; }

    // --- Kernel-facing behaviour (non-virtual: one policy for all gates) ---

    /**
     * @brief Called when any input net changes. Recomputes outputs and
     *        schedules those heading somewhere new.
     */
    void evaluate(uint64_t now, EventQueue& eq);

    /**
     * @brief Called when a scheduled output event fires. Drops the event if a
     *        newer one superseded it, otherwise drives the net.
     */
    void commit(uint16_t out_index,
                LogicValue value,
                uint32_t serial,
                uint64_t now,
                EventQueue& eq);

    /// Back to power-on: nothing driven, nothing pending, subclass state reset.
    void resetState();

    // --- Timing ----------------------------------------------------------

    /// Delay for a transition ending at @p to.
    uint32_t delayFor(LogicValue to) const;

    void setDelays(uint32_t rise, uint32_t fall, uint32_t decay)
    {
        rise_delay_ = rise;
        fall_delay_ = fall;
        decay_delay_ = decay;
    }

    /// Verilog's convention: given two values, decay is the smaller.
    void setDelays(uint32_t rise, uint32_t fall)
    {
        setDelays(rise, fall, (rise < fall ? rise : fall));
    }

    void setDelay(uint32_t d) { setDelays(d, d, d); }

    /// Sets delays by hand and pins them: TimingModels skip this gate.
    void setFixedDelays(uint32_t rise, uint32_t fall, uint32_t decay)
    {
        setDelays(rise, fall, decay);
        fixed_delays_ = true;
    }

    /// Lets the TimingModel manage this gate's delays again.
    void clearFixedDelays() { fixed_delays_ = false; }
    bool hasFixedDelays() const { return fixed_delays_; }

    uint32_t riseDelay()  const { return rise_delay_; }
    uint32_t fallDelay()  const { return fall_delay_; }
    uint32_t decayDelay() const { return decay_delay_; }

    /// Largest fanout across this component's outputs, for load-scaled delay.
    std::size_t maxFanout() const;

    // --- Wiring ----------------------------------------------------------

    /**
     * @brief Re-points input @p pin at @p net (nullptr disconnects it).
     *
     * Only updates the wiring; the Simulator re-evaluates afterwards.
     * @throws std::out_of_range if @p pin does not exist.
     */
    void connectInput(std::size_t pin, Net* net);

    /**
     * @brief Re-points output @p pin at @p net (nullptr disconnects it).
     *
     * The new net immediately sees whatever this output is driving.
     * @throws std::out_of_range if @p pin does not exist.
     */
    void connectOutput(std::size_t pin, Net* net);

    /// Disconnects every pin attached to @p net.
    void disconnect(const Net* net);

    // --- Accessors -------------------------------------------------------

    /// Stable id assigned by the owning Simulator, or kNoId.
    uint32_t id() const { return id_; }
    const std::string& getName() const { return name_; }
    const std::vector<Net*>& getInputs() const { return inputs_; }
    const std::vector<Net*>& getOutputs() const { return outputs_; }
    std::size_t numInputs() const { return inputs_.size(); }
    std::size_t numOutputs() const { return outputs_.size(); }
    Strength strength() const { return strength_; }

    /// Value on input @p pin, HIGHZ if unconnected.
    /// @throws std::out_of_range if @p pin does not exist.
    LogicValue inputValue(std::size_t pin) const
    {
        const Net* n = inputs_.at(pin);
        return n ? n->getValue() : LogicValue::HIGHZ;
    }

    /// Value output @p pin is driving right now (HIGHZ before the first commit).
    /// @throws std::out_of_range if @p pin does not exist.
    LogicValue outputValue(std::size_t pin) const { return current_.at(pin); }

    void setName(std::string name) { name_ = std::move(name); }
};


struct TimingModel
{
    virtual ~TimingModel() = default;
    virtual void apply(Gate& c) const = 0;
};

/// Everything settles in zero time; ordering falls to delta cycles.
struct ZeroDelayModel : TimingModel
{
    void apply(Gate& c) const override { c.setDelay(0); }
};

/// Every gate costs one tick.
struct UnitDelayModel : TimingModel
{
    void apply(Gate& c) const override { c.setDelay(1); }
};

/// Stage count scaled, plus a load term proportional to fanout.
struct StageDelayModel : TimingModel
{
    uint32_t ticks_per_stage = 1;
    uint32_t ticks_per_load = 1;
    double fall_ratio = 1.0;

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

