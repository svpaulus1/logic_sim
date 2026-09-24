// File: Event.h

#ifndef EVENT_H
#define EVENT_H

#include <cstdint>
#include "Types.h"

class Gate;

/**
 * @struct Event
 * @brief Encapsulates a scheduled state change in the event simulation.
 *
 *        Current size of struct = 16 bytes.
 */
struct Event
{
    Gate* gate = nullptr;
    uint16_t out_index = 0;
    LogicValue value = LogicValue::UNKNOWN;
    uint32_t serial = 0;
};

#endif

