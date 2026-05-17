/**
 * @file   Types.h
 * @brief  Centralized type definitions for the simulation engine.
 */

#ifndef TYPES_H
#define TYPES_H

#include <queue>
#include <vector>
#include <functional>

#include "Event.h" 

/**
 * @brief The standard priority queue used for scheduling simulation events.
 * Orders events chronologically based on their timestamp.
 */
using EventQueue = std::priority_queue<Event,
                                       std::vector<Event>,
                                       std::greater<Event>>;

#endif
