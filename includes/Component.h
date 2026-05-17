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
    uint64_t id_;      ///< Unique internal identifier.
    std::string name_; ///< Human-readable identifier.
    uint64_t delay_;   ///< The propagation delay of the component.

    /// Collection of incoming connections driving this component.
    std::vector<Net*> inputs_;
    /// Collection of outgoing connections driven by this component.
    std::vector<Net*> outputs_;

public:
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

    // --- Getters ---

    /** @return The unique identifier of this component. */
    uint64_t getId() const { return id_; }

    /** @return The name of this component. */
    std::string getName() const { return name_; }

    /** @return The propagation delay of this component. */
    uint64_t getPropDelay() const { return prop_delay_; }

    /** @return A vector of Net inputs of this component. */
    std::vector<Net*> getInputs() const { return inputs_; }

    /** @return A vector of Net outputs of this component. */
    std::vector<Net*> getOutputs() const { return outputs_; }

    // --- Setters ---

    /** @brief Set a new name for the component.
     *  @param name New string value to set name_ to.
     */
    void setName(std::string name) { name_ = name; }

    /** @brief Set a new vector of Nets as inputs.
     *  @param inputs A vector of Nets to be set as the new input_ vector.
     */
    void setInputs(std::vector<Net*> inputs) { inputs_ = inputs; }

    /** @brief Set a new vector of Nets as outputs.
     *  @param outputs A vector of Nets to be set as the new output_ vector.
     */
    void setOutputs(std::vector<Net*> outputs) { outputs_ = outputs; }

    /** @brief Add one input to the inputs_ vector.
     *  @param input A pointer to a Net object to be added to inputs_.
     */
    void addInput(Net* input) { inputs_.push_back(input); }

    /** @brief Add one output to the outputs_ vector.
     *  @param output A pointer to a Net object to be added to outputs_.
     */
    void addOutput(Net* output) { outputs_.push_back(output); }
};

#endif
