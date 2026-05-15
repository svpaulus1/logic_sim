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
private:
    /// A unique identifier for this net instance.
    uint64_t id_;
    /// The current simulated electrical state of the wire.
    LogicValue current_value_ = LogicValue::UNKNOWN;

    /// Pointer to the component output that determines the state of this net.
    Component* driver_ = nullptr; 
    /// Collection of component inputs that react to changes on this net.
    std::vector<Component*> consumers_;
};

#endif
