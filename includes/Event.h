/**
 * @file   Event.h
 * @brief  Class declaration for scheduled simulation events.
 *
 * This file defines the Event class, which encapsulates a future state 
 * change for a specific Net in the digital simulation. It also defines 
 * the necessary comparison operators to allow events to be sorted 
 * chronologically in a priority queue.
 *
 * @author Sebastian Paulus
 * @date   2026-05-15
 */

#ifndef EVENT_H
#define EVENT_H

#include <vector>
#include <memory>

#include "LogicValue.h"

class Net; // Forward declaration

/**
 * @brief Represents a scheduled state transition within the simulation.
 *
 * Events are the driving force of the logic simulator. When a component 
 * evaluates its logic and determines an output will change, it schedules 
 * an Event for a future timestamp. The main simulation loop processes 
 * these chronologically to maintain causality and resolve race conditions.
 */
class Event
{
private:
    /// The exact simulation time this event occurs.
    uint64_t timestamp_;
    /// The wire/connection that will receive the new logic value.
    std::shared_ptr<Net> target_;
    /// The logic state the target net will transition to.
    LogicValue value_;

public:
    /**
     * @brief Constructs a new simulation Event.
     * 
     * @param time  The future timestamp when this event should execute.
     * @param net   The target Net to update.
     * @param val   The new LogicValue to apply to the net.
     */
    Event(uint64_t time, std::shared_ptr<Net> net, LogicValue val)
        : timestamp_(time), target_(std::move(net)), value_(val) 
    {}

    // --- Getters ---

    /** @return The timestamp of when this event executes. */
    uint64_t getTimestamp() const { return timestamp_; }

    /** @return A pointer to the net being modified. */
    std::shared_ptr<Net> getTarget() const { return target_; }

    /** @return The logic value to apply. */
    LogicValue getValue() const { return value_; }

    // --- Operators ---

    /**
     * @brief Compares two events based on their chronological timestamp.
     *
     * This operator is used by std::priority_queue and std::greater to 
     * ensure that the queue acts as a Min-Heap. Events with the smallest 
     * (earliest) timestamp will bubble to the top of the queue.
     *
     * @param other  The event to compare against.
     * @return True  if this event occurs strictly after the other event.
     */
    bool operator>(const Event& other) const
    {
        return timestamp_ > other.timestamp_;
    }
};

#endif
