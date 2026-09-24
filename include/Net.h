/**
 * @file Net.h
 * @author Sebastian Paulus
 * @date 2026/06/16
 * @brief The Net class.
 *
 * @details
 * The Net object allows for logic signals to transmit from one component to
 * another. It handles multiple driver components, which is what makes enable
 * buffers and tri-state buses work. Nets have no delay of their own: all timing
 * lives in the driving Component, so a resolved change reaches the sinks in
 * zero simulated time.
 */
 
#ifndef NET_H
#define NET_H
 
#include <cassert>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>
#include "Types.h"
 
class Gate;
class EventQueue;
 
/**
 * @class Net
 * @brief Represents a physical wire connecting hardware components.
 */
class Net
{
private:
    std::string name_;
    LogicValue value_;
    std::vector<std::pair<Gate*, LogicValue>> drivers_;
    std::vector<Gate*> output_;
 
    /// Recompute value_ from drivers_. Returns true if it changed.
    bool resolve_();
 
    void wakeSinks_(uint64_t now, EventQueue& eq);
 
public:
    /**
     * @brief Constructs a new Net object.
     * @param name The descriptive name of the net.
     * @param value The initial logic state.
     */
    Net(std::string name = "Wire",
        LogicValue value = LogicValue::UNKNOWN)
        : name_(std::move(name)), value_(value)
    {}
 
    // --- Getters ---
 
    const std::string& getName() const { return name_; }
    LogicValue getValue() const { return value_; }
    const std::vector<Gate*>& getOutputs() const { return output_; }
 
    /// Fanout: how many component inputs this net feeds. Read by the
    /// TimingModel at build time to scale the driver's delay by load.
    std::size_t sinkCount() const { return output_.size(); }
 
    /// One driver means contention is impossible.
    std::size_t driverCount() const { return drivers_.size(); }
 
    // --- Construction ---
 
    void setName(std::string name) { name_ = std::move(name); }
 
    /// Registers a gate that reads this net.
    void addOutput(Gate* gate)
    {
        assert(gate && "null sink");
        output_.push_back(gate);
    }
 
    /**
     * @brief Registers a driver and returns the index it must use later.
     * @return Index into drivers_.
     */
    std::size_t addDriver(Gate* gate)
    {
        assert(gate && "null driver");
        drivers_.push_back({gate, LogicValue::HIGHZ});
        return drivers_.size() - 1;
    }
 
    /// Resolve once after the netlist is fully wired, before simulation starts,
    /// so value_ reflects the drivers rather than the constructor default.
    void initialResolve() { resolve_(); }
 
    // --- Simulation ---
 
    /**
     * @brief Applies a driver's new contribution and, if the resolved value
     *        changed, wakes every sink.
     */
    void driveFrom(std::size_t driver_index,
                   LogicValue v,
                   uint64_t now,
                   EventQueue& eq);
 
    /**
     * @brief Testbench / initialisation override. Bypasses driver resolution
     *        but still wakes sinks. Not for use by components.
     */
    void forceValue(LogicValue v, uint64_t now, EventQueue& eq);
};
 
#endif
