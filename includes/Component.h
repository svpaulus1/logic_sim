/**
 * @file   Component.h
 * @brief  Base class declaration for simulated hardware components.
 *
 * This file defines the abstract base Component class, which establishes 
 * the required interface and shared state (delays, inputs, outputs) for 
 * all logic gates and complex modules in the circuit simulation.
 *
 * @author Sebastian Paulus
 * @date   2026-05-15
 */

#ifndef COMPONENT_H
#define COMPONENT_H

#include <vector>
#include <string>
#include <memory>

class Net; // Forward declaration

/**
 * @brief Abstract base class representing a generic hardware element.
 *
 * The Component class defines the fundamental properties of a hardware 
 * element, including its propagation delay and its connections to the 
 * rest of the circuit via input and output Nets. Derived classes must 
 * implement the evaluate() method to define their specific logic behavior.
 */
class Component
{
protected:
    std::string name_; ///< The unique identifier for this component instance.
    uint64_t delay_; ///< The propagation delay of the component.

    /// Collection of incoming connections driving this component.
    std::vector<std::shared_ptr<Net>> inputs_;
    /// Collection of outgoing connections driven by this component.
    std::vector<std::shared_ptr<Net>> outputs_;

    /**
     * @brief Computes the logic state of the component based on its inputs.
     *
     * This pure virtual method must be overridden by derived classes. 
     * It reads the current logic values from the inputs_, applies the 
     * component's specific Boolean logic (e.g., AND, OR, NOT), and schedules 
     * updates to the outputs_ factoring in the prop_delay_.
     */
    virtual void evaluate() = 0;

    /**
     * @brief Virtual destructor.
     * 
     * Ensures proper cleanup of derived component classes when they are 
     * destroyed through a base class pointer.
     */
    virtual ~Component() = default;
};

#endif
