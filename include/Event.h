/**
 * @file Event.h
 * @author Sebastian Paulus
 * @date 2026/09/24
 * @brief A scheduled change of one gate output, as stored in the EventQueue.
 */

#ifndef EVENT_H
#define EVENT_H

#include <cstdint>
#include "Types.h"

class Gate;

/**
 * @struct Event
 * @brief A scheduled change of one gate output.
 *
 * The EventQueue files each event under the time and Region it fires in; the
 * time itself is not stored here. When the event fires, the queue calls
 * Gate::commit(), which drops it if the gate has scheduled something newer
 * for that output since (the serials no longer match). That is how inertial
 * delay swallows short pulses.
 *
 * Kept small (16 bytes on 64-bit platforms) because the queue holds many.
 */
struct Event
{
    Gate* gate = nullptr;                   ///< Gate whose output changes.
    uint16_t out_index = 0;                 ///< Which output of @c gate.
    LogicValue value = LogicValue::UNKNOWN; ///< Value the output takes.

    /// Must match the gate's pending serial for this output, or the event is
    /// stale and gets dropped.
    uint32_t serial = 0;
};

#endif

