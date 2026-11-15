/**
 * @file   NandGate.h
 * @brief  Class declaration for an N-input logical NAND gate.
 *
 * This file defines the NandGate class, which inherits from the base Component 
 * class. It implements standard digital NAND logic: the output is LOW strictly 
 * if all inputs are HIGH. If any input is LOW, the output is forced HIGH. 
 * Otherwise, the output is UNKNOWN.
 *
 * @author Sebastian Paulus
 * @date   2026-05-25
 */

#ifndef NAND_GATE_H
#define NAND_GATE_H

#include <string>
#include "Component.h"
#include "SimulationTypes.h"
#include "LogicValue.h"
#include "Net.h"

/**
 * @brief Represents a hardware NAND gate with an arbitrary number of inputs.
 *
 * The NandGate evaluates its input nets. It supports N-inputs (2-input,
 * 3-input, etc.) based on how many Nets are added to its inputs_ vector. When
 * evaluated, it calculates the future state and schedules an Event on the
 * simulation queue.
 */
class NandGate : public Component
{
public:
    /**
     * @brief Constructs a new NandGate.
     *
     * Initializes the protected member variables inherited from the base 
     * Component class.
     *
     * @param id    The unique identifier for this component.
     * @param name  The human-readable name of the gate (e.g., "NAND_1").
     * @param delay The propagation delay in simulation time units.
     */
    NandGate(uint64_t id, const std::string& name, uint64_t delay)
    {
        this->id_ = id;
        this->name_ = name;
        this->delay_ = delay;
    }

    /**
     * @brief Computes the NAND logic and schedules an output change.
     *
     * Iterates through all connected input Nets. Applies standard hardware 
     * logic resolution (LOW dominates and forces output HIGH). If the
     * calculated result differs from the output Net's current state, a future
     * Event is pushed to the queue.
     *
     * @param queue        Reference to the master simulation event queue.
     * @param current_time The current simulation clock.
     */
    void evaluate(EventQueue& queue, uint64_t current_time) override;
};

#endif
