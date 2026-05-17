/**
 * @file   Net.h
 * @brief  Class declaration for simulated electrical nets (wires).
 *
 * This file contains the Net class, which models the physical wires connecting 
 * components in the logic simulator. It stores the current logic state and 
 * manages the relationship between the component driving the signal and the 
 * components receiving it.
 *
 * @author Sebastian Paulus
 * @date   2026-05-15
 */

#ifndef NET_H
#define NET_H

#include <vector>
#include "LogicValue.h"

class Component; // Forward declaration.
class Circuit;   // Forward declaration.

/**
 * @brief Represents an electrical connection between hardware components.
 *
 * A Net acts as a single contiguous wire or bus in the simulation. It holds 
 * a specific LogicValue at any given time and maps the flow of data from a 
 * single driving source (driver_) to one or more destination inputs
 * (consumers_).
 */
class Net
{
    friend class Circuit;
    
private:
    /// A unique identifier for this net instance.
    uint64_t id_;
    
    /// The current simulated electrical state of the wire.
    LogicValue value_ = LogicValue::UNKNOWN;

    /// Pointer to the component output that determines the state of this net.
    Component* driver_ = nullptr;
    
    /// Collection of component inputs that react to changes on this net.
    std::vector<Component*> consumers_;


    /**
     * @brief Directly sets the logic value of the net.
     * @note  This is private to prevent gates from instantly changing wires.
     * Only the Circuit class (via the friend declaration) can call this 
     * during the event loop.
     */
    void setValue(LogicValue val) { value_ = val; }

public:
    /**
     * @brief Constructs a new Net with fully initialized connections.
     * 
     * @param id        The unique identifier for this net.
     * @param val       The initial logic state of the wire (usually UNKNOWN).
     * @param driver    Pointer to the component output driving this net.
     * @param consumers Collection of component inputs reading from this net.
     */
    Net(uint64_t id,
        LogicValue val,
        Component* driver,
        std::vector<Component*> consumers)
        : id_(id),
          value_(val),
          driver_(driver),
          consumers_(consumers)
    {}

    // --- Getters ---

    /** @return The unique identifier of this net. */
    uint64_t getId() const { return id_; }
    
    /** @return The current simulated electrical state of the net. */
    LogicValue getValue() const { return value_; }
    
    /** @return Pointer to the component driving this signal. */
    Component* getDriver() const { return driver_; }
    
    /** @return A copy of the vector with all components reading this signal. */
    std::vector<Component*> getConsumers() const { return consumers_; }

    // --- Setters ---
    
    /**
     * @brief Sets the source component driving this net.
     * @param d Pointer to the driving component.
     */
    void setDriver(Component* d) { driver_ = d; }
    
    /**
     * @brief Overwrites the list of components reading this net.
     * @param c The new collection of consumer components.
     */
    void setConsumers(std::vector<Component*> c) { consumers_ = c; }

    /** @brief Adds a new component to the consumers_ vector.
     *  @param c A pointer to a Component object to be added to consumers_.
     */
    void addConsumer(Component* c) { consumers_.push_back(c); }
};

#endif
