/**
 * @file   AndGate.h
 * @brief  Class declaration for an N-input logical AND gate.
 *
 * This file defines the AndGate class, which inherits from the base Component 
 * class. It implements standard digital AND logic: the output is HIGH only if 
 * all inputs are HIGH. If any input is LOW, the output is LOW. Otherwise, 
 * the output is UNKNOWN.
 *
 * @author Sebastian Paulus
 * @date   2026-05-17
 */

#ifndef AND_GATE_H
#define AND_GATE_H

#include <string>
#include "Component.h"
#include "SimulationTypes.h"
#include "LogicValue.h"
#include "Net.h"

/**
 * @brief Represents a hardware AND gate with an arbitrary number of inputs.
 *
 * The AndGate evaluates its input nets. It supports N-inputs (2-input,
 * 3-input, etc.) based on how many Nets are added to its inputs_ vector. When
 * evaluated, it calculates the future state and schedules an Event on the
 * simulation queue.
 */
class AndGate : public Component
{
public:
    /**
     * @brief Constructs a new AndGate.
     * * Initializes the protected member variables inherited from the base 
     * Component class.
     *
     * @param id    The unique identifier for this component.
     * @param name  The human-readable name of the gate (e.g., "AND_1").
     * @param delay The propagation delay in simulation time units.
     */
    AndGate(uint64_t id, const std::string& name, uint64_t delay)
    {
        this->id_ = id;
        this->name_ = name;
        this->delay_ = delay;
    }

    /**
     * @brief Computes the AND logic and schedules an output change.
     *
     * Iterates through all connected input Nets. Applies standard hardware 
     * logic resolution (LOW dominates). If the calculated result differs 
     * from the output Net's current state, a future Event is pushed to the
     * queue.
     *
     * @param queue        Reference to the master simulation event queue.
     * @param current_time The current simulation clock
     */
    void evaluate(EventQueue& queue, uint64_t current_time) override;
};

#endif
