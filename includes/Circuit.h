/**
 * @file   Circuit.h
 * @brief  Top-level simulation manager and memory owner.
 *
 * This file defines the central Circuit class, which acts as the "God Object"
 * for the simulation. It owns all allocated memory (Nets and Components) via 
 * unique pointers and manages the chronological execution of the simulation 
 * through its internal event queue.
 *
 * @author Sebastian Paulus
 * @date   2026-05-15
 */

#ifndef CIRCUIT_H
#define CIRCUIT_H

#include <vector>
#include <memory>
#include <queue>

#include "Component.h"
#include "Net.h"
#include "Event.h"
#include "Types.h"

/**
 * @brief The core simulation engine and memory manager.
 *
 * The Circuit class creates and securely stores all hardware elements. It 
 * prevents memory leaks by owning all objects while safely allowing them to 
 * communicate via raw observation pointers. It also houses the priority queue
 * that drives the discrete-event simulation forward.
 */
class Circuit
{
private:
    /// Master list of all components. The Circuit guarantees their lifetime.
    std::vector<std::unique_ptr<Component>> all_components_;
    
    /// Master list of all wires/nets. The Circuit guarantees their lifetime.
    std::vector<std::unique_ptr<Net>> all_nets_;

    /// The chronological engine driving the simulation state changes.
    EventQueue event_queue_;

    uint64_t time_ = 0; ///< Current time of the simulation.

public:
    /** @brief Default constructor. Initializes an empty circuit. */
    Circuit() = default;

    /** @brief Default destructor. Safely deletes all components and nets. */
    ~Circuit() = default;

    // Note: Future methods to be implemented in Circuit.cpp
    
    // void scheduleEvent(uint64_t time, Net* target, LogicValue value);
    // void runSimulation();
};

#endif
