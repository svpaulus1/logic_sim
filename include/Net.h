/**
 * @file Net.h
 * @author Sebastian Paulus
 * @date 2026/06/16
 * @brief A wire: resolves its drivers and wakes the gates that read it.
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
 * A Net carries one logic value from its drivers (gate outputs) to its sinks
 * (gate inputs). It handles multiple drivers, which is what makes enable
 * buffers, tri-state buses and pullups work. Nets have no delay of their own:
 * all timing lives in the driving Gate, so a resolved change reaches the
 * sinks in zero simulated time.
 *
 * Resolution rules, applied whenever a driver changes:
 *  - drivers that are released (HIGHZ) are ignored;
 *  - of the rest, only those at the highest Strength count;
 *  - if they all agree the net takes that value, otherwise UNKNOWN;
 *  - with no active driver at all the net floats at HIGHZ.
 *
 * Nets are normally created with Simulator::addNet(), which assigns an id and
 * keeps the net alive; gates register themselves as drivers and sinks when
 * they are built or rewired.
 *
 * @code
 *   Net& bus = sim.addNet("bus");
 *   sim.addGate(BuiltinGate::Type::BUFIF1, {&a, &en_a}, {&bus});
 *   sim.addGate(BuiltinGate::Type::PULLUP, {}, {&bus});
 *   // bus is HIGH while en_a is LOW, and follows a while en_a is HIGH.
 * @endcode
 */
class Net
{
    /// Assigns ids, hooks up change tracking and resolves nets directly.
    friend class Simulator;

public:
    /// One driver slot. A slot whose gate is null is free for reuse.
    struct Driver
    {
        Gate* gate = nullptr;                 ///< Driving gate; null if free.
        LogicValue value = LogicValue::HIGHZ; ///< What this driver puts out.
        Strength strength = Strength::Strong; ///< How hard it drives.
    };

    /// id() of a net that no Simulator owns.
    static constexpr uint32_t kNoId = UINT32_MAX;

private:
    std::string name_;                    ///< Descriptive name.
    LogicValue value_;                    ///< Current resolved value.
    std::vector<Driver> drivers_;         ///< Driver slots, some maybe free.
    std::vector<std::size_t> free_slots_; ///< Indices of free driver slots.
    std::size_t active_drivers_ = 0;      ///< Slots in use.
    std::vector<Gate*> sinks_;            ///< Readers, once per input pin.

    bool forced_ = false;                 ///< Held by forceValue().
    LogicValue forced_value_ = LogicValue::UNKNOWN; ///< Value while forced.

    // --- Simulator bookkeeping ---
    uint32_t id_ = kNoId;                     ///< See id().
    std::vector<Net*>* change_log_ = nullptr; ///< Where to report changes.
    bool logged_ = false;                      ///< Already in change_log_.
    LogicValue reported_ = LogicValue::UNKNOWN; ///< Last value listeners saw.

    /**
     * @brief Recomputes value_ from drivers_ (or the forced value).
     * @return true if the value changed.
     */
    bool resolve_();

    /**
     * @brief Re-evaluates every gate that reads this net.
     * @param now Current simulation time.
     * @param eq  Queue the gates schedule their output changes into.
     */
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

    /// Not copyable: gates hold pointers to their nets.
    Net(const Net&) = delete;

    /// Not copyable: gates hold pointers to their nets.
    Net& operator=(const Net&) = delete;

    // --- Getters ---

    /// @brief Stable id assigned by the owning Simulator.
    /// @return The id, or kNoId if no Simulator owns this net.
    uint32_t id() const { return id_; }

    /// @brief Descriptive name (diagnostics, GUI, VCD output).
    /// @return The name.
    const std::string& getName() const { return name_; }

    /// @brief The current resolved value.
    /// @return LOW, HIGH, UNKNOWN or HIGHZ.
    LogicValue getValue() const { return value_; }

    /// @brief Gates that read this net.
    /// @return One entry per connected input pin, so a gate reading the net
    ///         on two pins appears twice.
    const std::vector<Gate*>& getSinks() const { return sinks_; }

    /// @brief Fanout: how many gate inputs this net feeds. Read by the
    ///        TimingModel to scale the driver's delay by load.
    /// @return Number of connected input pins.
    std::size_t sinkCount() const { return sinks_.size(); }

    /// @brief Number of connected drivers. One driver means contention is
    ///        impossible.
    /// @return Driver slots in use.
    std::size_t driverCount() const { return active_drivers_; }

    /// @brief Every driver slot, including free ones (gate == nullptr).
    /// @return The driver slots, indexed by driver index.
    const std::vector<Driver>& getDrivers() const { return drivers_; }

    /**
     * @brief Contribution of one driver slot.
     * @param driver_index Index returned by addDriver().
     * @return What that driver currently puts on the net.
     */
    LogicValue driverValue(std::size_t driver_index) const
    {
        assert(driver_index < drivers_.size() && "unregistered driver index");
        return drivers_[driver_index].value;
    }

    /// @brief Tells whether two or more of the winning drivers disagree.
    /// @return true on a bus fight (the net is then UNKNOWN).
    bool hasConflict() const;

    /// @brief Tells whether forceValue() is holding the net.
    /// @return true until release() is called.
    bool isForced() const { return forced_; }

    // --- Construction ---

    /// @brief Renames the net.
    /// @param name The new name.
    void setName(std::string name) { name_ = std::move(name); }

    /// @brief Registers a gate input pin that reads this net.
    /// @param gate The reading gate; must not be null.
    void addSink(Gate* gate)
    {
        assert(gate && "null sink");
        sinks_.push_back(gate);
    }

    /// @brief Unregisters one input pin of @p gate (a gate that reads this net
    ///        on two pins is registered twice).
    /// @param gate The reading gate.
    void removeSink(Gate* gate);

    /**
     * @brief Registers a driver and returns the index it must use later.
     *
     * Indices stay valid until removeDriver(); freed slots are reused.
     * The net is not re-resolved: call refresh() (the Simulator does).
     *
     * @param gate     The driving gate; must not be null.
     * @param strength How hard it drives.
     * @param initial  Its contribution until it first drives something.
     * @return Index into the driver list.
     */
    std::size_t addDriver(Gate* gate,
                          Strength strength = Strength::Strong,
                          LogicValue initial = LogicValue::HIGHZ);

    /// @brief Frees a driver slot. The net is not re-resolved: call refresh().
    /// @param driver_index Index returned by addDriver().
    void removeDriver(std::size_t driver_index);

    // --- Simulation ---

    /**
     * @brief Applies a driver's new contribution and, if the resolved value
     *        changed, wakes every sink.
     *
     * @param driver_index Index returned by addDriver().
     * @param v            The driver's new value.
     * @param now          Current simulation time.
     * @param eq           Queue the woken gates schedule into.
     */
    void driveFrom(std::size_t driver_index,
                   LogicValue v,
                   uint64_t now,
                   EventQueue& eq);

    /**
     * @brief Re-resolves from the current drivers and wakes the sinks if the
     *        value changed. Used after wiring changes and at initialisation.
     *
     * @param now Current simulation time.
     * @param eq  Queue the woken gates schedule into.
     */
    void refresh(uint64_t now, EventQueue& eq);

    /**
     * @brief Testbench / debug override (Verilog's force). The net holds @p v
     *        regardless of its drivers until release() is called. Sinks are
     *        woken immediately.
     *
     * @param v   Value to hold.
     * @param now Current simulation time.
     * @param eq  Queue the woken gates schedule into.
     */
    void forceValue(LogicValue v, uint64_t now, EventQueue& eq);

    /**
     * @brief Ends a forceValue(): the net goes back to its resolved driver
     *        value.
     *
     * @param now Current simulation time.
     * @param eq  Queue the woken gates schedule into.
     */
    void release(uint64_t now, EventQueue& eq);

    /// Back to power-on: every driver released, no force, value HIGHZ.
    /// Does not wake anything; the Simulator re-initialises afterwards.
    void resetState();
};

#endif
