/**
 * @file Gate.h
 * @author Sebastian Paulus
 * @date 2026/09/13
 * @brief Abstract base class for logic gates, plus timing models.
 */

#ifndef GATE_H
#define GATE_H

#include <cstdint>
#include <string>
#include <utility>
#include <vector>
#include "Event.h"
#include "Types.h"
#include "Net.h"

class EventQueue;

/**
 * @class Gate
 * @brief Abstract base class representing a logic Gate.
 *
 * Subclasses implement pure combinational logic (computeOutputs) and declare
 * their intrinsic complexity (intrinsicStages).
 */
class Gate
{
protected:
    std::vector<Net*> inputs_; ///< Nets driving this component.
    std::vector<Net*> outputs_; ///< Nets driven by this component.
    std::vector<std::size_t> driver_ids_; ///< Index of output's driver list.
    std::string name_; ///< Descriptive name, diagnostics only.
 
    uint32_t rise_delay_ = 0; ///< Ticks to drive an output to HIGH.
    uint32_t fall_delay_ = 0; ///< Ticks to drive an output to LOW.
    uint32_t decay_delay_ = 0; ///< Ticks to fade to HIGHZ once undriven.
 
    /// Per-output counter, bumped on every schedule. A fired event whose serial
    /// no longer matches has been superseded and is dropped.
    std::vector<uint32_t> pending_serial_;
 
    /// Per-output value this component is currently heading toward.
    std::vector<LogicValue> target_;
 
public:
    /**
     * @brief Constructs a gate and wires it into the netlist.
     *
     * Registers itself as a sink on every input and as a driver on every
     * output, capturing the driver handles.
     */
    Gate(std::vector<Net*> inputs,
         std::vector<Net*> outputs,
         std::string name);
 
    virtual ~Gate() = default;
 
    Gate(const Gate&) = delete;
    Gate& operator=(const Gate&) = delete;
 
    // --- Subclass responsibilities ---------------------------------------
 
    /**
     * @brief Pure logic function. Reads inputs_, writes one value per output.
     */
    virtual void computeOutputs(std::vector<LogicValue>& out) const = 0;
 
    /**
     * @brief Intrinsic CMOS stage count, consumed by the TimingModel.
     */
    virtual uint32_t intrinsicStages() const = 0;
 
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
 
    uint32_t riseDelay()  const { return rise_delay_; }
    uint32_t fallDelay()  const { return fall_delay_; }
    uint32_t decayDelay() const { return decay_delay_; }
 
    /// Largest fanout across this component's outputs, for load-scaled delay.
    std::size_t maxFanout() const;
 
    // --- Accessors -------------------------------------------------------
 
    const std::string& getName() const { return name_; }
    const std::vector<Net*>& getInputs() const { return inputs_; }
    const std::vector<Net*>& getOutputs() const { return outputs_; }
 
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
        const uint32_t base =
            static_cast<uint32_t>(c.intrinsicStages() * ticks_per_stage
                                  + c.maxFanout() * ticks_per_load);
        const uint32_t fall = static_cast<uint32_t>(base * fall_ratio);
        c.setDelays(base ? base : 1, fall ? fall : 1);
    }
};

#endif

