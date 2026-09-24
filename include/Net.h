/**
 * @file Net.h
 * @author Sebastian Paulus
 * @date 2026/06/16
 * @brief The Net class.
 *
 * @details
 * The Net object allows for logic signals to transmit from one gate to
 * another. It handles multiple drivers, which is what makes enable buffers,
 * tri-state buses and pullups work. Nets have no delay of their own: all timing
 * lives in the driving Gate, so a resolved change reaches the sinks in zero
 * simulated time.
 */

#ifndef NET_H
#define NET_H

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>
#include "Types.h"

class Gate;
class EventQueue;
class Simulator;

/**
 * @class Net
 * @brief Represents a physical wire connecting hardware components.
 *
 * Resolution rules, applied whenever a driver changes:
 *  - drivers that are released (HIGHZ) are ignored;
 *  - of the rest, only those at the highest Strength count;
 *  - if they all agree the net takes that value, otherwise UNKNOWN;
 *  - with no active driver at all the net floats at HIGHZ.
 */
class Net
{
    friend class Simulator;

public:
    /// One driver slot. A slot whose gate is null is free for reuse.
    struct Driver
    {
        Gate* gate = nullptr;
        LogicValue value = LogicValue::HIGHZ;
        Strength strength = Strength::Strong;
    };

    /// id() of a net that no Simulator owns.
    static constexpr uint32_t kNoId = UINT32_MAX;

private:
    std::string name_;
    LogicValue value_;
    std::vector<Driver> drivers_;
    std::vector<std::size_t> free_slots_;
    std::size_t active_drivers_ = 0;
    std::vector<Gate*> sinks_;

    bool forced_ = false;
    LogicValue forced_value_ = LogicValue::UNKNOWN;

    // --- Simulator bookkeeping ---
    uint32_t id_ = kNoId;
    std::vector<Net*>* change_log_ = nullptr; ///< Where to report changes.
    bool logged_ = false;                      ///< Already in change_log_.
    LogicValue reported_ = LogicValue::UNKNOWN; ///< Last value listeners saw.

    /// Recompute value_ from drivers_ (or the forced value). Returns true if it changed.
    bool resolve_();

    void wakeSinks_(uint64_t now, EventQueue& eq);

public:
    /**
     * @brief Constructs a new Net object.
     * @param name The descriptive name of the net.
     * @param value The value before the first resolve. A Simulator resolves
     *              every net when it initialises, so this is only visible to
     *              code that inspects a net before simulation starts.
     */
    Net(std::string name = "Wire",
        LogicValue value = LogicValue::UNKNOWN)
        : name_(std::move(name)), value_(value)
    {}

    Net(const Net&) = delete;
    Net& operator=(const Net&) = delete;

    // --- Getters ---

    /// Stable id assigned by the owning Simulator, or kNoId.
    uint32_t id() const { return id_; }
    const std::string& getName() const { return name_; }
    LogicValue getValue() const { return value_; }

    /// Gates that read this net, once per connected input pin.
    const std::vector<Gate*>& getSinks() const { return sinks_; }

    /// Fanout: how many gate inputs this net feeds. Read by the
    /// TimingModel to scale the driver's delay by load.
    std::size_t sinkCount() const { return sinks_.size(); }

    /// Number of connected drivers. One driver means contention is impossible.
    std::size_t driverCount() const { return active_drivers_; }

    /// Every driver slot, including free ones (gate == nullptr).
    const std::vector<Driver>& getDrivers() const { return drivers_; }

    /// Contribution of one driver slot.
    LogicValue driverValue(std::size_t driver_index) const
    {
        assert(driver_index < drivers_.size() && "unregistered driver index");
        return drivers_[driver_index].value;
    }

    /// True if two or more of the winning drivers disagree (bus fight).
    bool hasConflict() const;

    bool isForced() const { return forced_; }

    // --- Construction ---

    void setName(std::string name) { name_ = std::move(name); }

    /// Registers a gate input pin that reads this net.
    void addSink(Gate* gate)
    {
        assert(gate && "null sink");
        sinks_.push_back(gate);
    }

    /// Unregisters one input pin of @p gate (a gate that reads this net on
    /// two pins is registered twice).
    void removeSink(Gate* gate);

    /**
     * @brief Registers a driver and returns the index it must use later.
     *
     * Indices stay valid until removeDriver(); freed slots are reused.
     * The net is not re-resolved: call refresh() (the Simulator does).
     *
     * @return Index into the driver list.
     */
    std::size_t addDriver(Gate* gate,
                          Strength strength = Strength::Strong,
                          LogicValue initial = LogicValue::HIGHZ);

    /// Frees a driver slot. The net is not re-resolved: call refresh().
    void removeDriver(std::size_t driver_index);

    // --- Simulation ---

    /**
     * @brief Applies a driver's new contribution and, if the resolved value
     *        changed, wakes every sink.
     */
    void driveFrom(std::size_t driver_index,
                   LogicValue v,
                   uint64_t now,
                   EventQueue& eq);

    /// Re-resolves from the current drivers and wakes the sinks if the value
    /// changed. Used after wiring changes and at initialisation.
    void refresh(uint64_t now, EventQueue& eq);

    /**
     * @brief Testbench / debug override (Verilog's force). The net holds @p v
     *        regardless of its drivers until release() is called. Sinks are
     *        woken immediately.
     */
    void forceValue(LogicValue v, uint64_t now, EventQueue& eq);

    /// Ends a forceValue(): the net goes back to its resolved driver value.
    void release(uint64_t now, EventQueue& eq);

    /// Back to power-on: every driver released, no force, value HIGHZ.
    /// Does not wake anything; the Simulator re-initialises afterwards.
    void resetState();
};

#endif
