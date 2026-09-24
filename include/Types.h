/**
 * @file Types.h
 * @date 2026/09/24
 * @brief 4-state logic values, their operators, and driver strengths.
 */

#ifndef TYPES_H
#define TYPES_H

#include <cstdint>
#include <optional>

/**
 * @enum LogicValue
 * @brief Represents the 4-state logic levels of a digital circuit.
 *
 * @note Backed by char so the enumerator values are the printable characters
 *       used in VCD output: static_cast<char>(v) yields '0', '1', 'X' or 'Z'.
 */
enum class LogicValue : char
{
    LOW     = '0', ///< Logic level 0 (Ground / False).
    HIGH    = '1', ///< Logic level 1 (Vcc / True).
    UNKNOWN = 'X', ///< Indeterminate state or bus contention.
    HIGHZ   = 'Z'  ///< High impedance state (disconnected / floating wire).
};

/**
 * @enum Strength
 * @brief How hard a driver pushes its value onto a net.
 *
 * When several drivers are active on one net, only the strongest ones count;
 * if those disagree the net resolves to UNKNOWN. This is what lets a pullup
 * hold a bus HIGH while every tri-state driver is released, yet lose cleanly
 * to any driver that is enabled.
 */
enum class Strength : uint8_t
{
    Pull   = 1, ///< Resistive: pullup / pulldown.
    Strong = 2  ///< Ordinary gate output.
};

/**
 * @brief Computes the logical AND of two 4-state logic values.
 * @param a The first logic value.
 * @param b The second logic value.
 * @return The resulting logic value (LOW, HIGH, or UNKNOWN).
 */
inline LogicValue operator&(LogicValue a, LogicValue b)
{
    if (a == LogicValue::LOW || b == LogicValue::LOW)
    {
        return LogicValue::LOW;
    }
    if (a == LogicValue::HIGH && b == LogicValue::HIGH)
    {
        return LogicValue::HIGH;
    }
    return LogicValue::UNKNOWN;
}

/**
 * @brief Computes the logical OR of two 4-state logic values.
 * @param a The first logic value.
 * @param b The second logic value.
 * @return The resulting logic value (LOW, HIGH, or UNKNOWN).
 */
inline LogicValue operator|(LogicValue a, LogicValue b)
{
    if (a == LogicValue::HIGH || b == LogicValue::HIGH)
    {
        return LogicValue::HIGH;
    }
    if (a == LogicValue::LOW && b == LogicValue::LOW)
    {
        return LogicValue::LOW;
    }
    return LogicValue::UNKNOWN;
}

/**
 * @brief Computes the logical XOR of two 4-state logic values.
 * @param a The first logic value.
 * @param b The second logic value.
 * @return The resulting logic value (LOW, HIGH, or UNKNOWN).
 */
inline LogicValue operator^(LogicValue a, LogicValue b)
{
    if (a == LogicValue::UNKNOWN || a == LogicValue::HIGHZ ||
        b == LogicValue::UNKNOWN || b == LogicValue::HIGHZ)
    {
        return LogicValue::UNKNOWN;
    }
    if (a != b)
    {
        return LogicValue::HIGH;
    }
    return LogicValue::LOW;
}

/**
 * @brief Computes the logical NOT (inversion) of a 4-state logic value.
 * @param a The logic value to invert.
 * @return The inverted logic value (LOW, HIGH, or UNKNOWN).
 */
inline LogicValue operator~(LogicValue a)
{
    if (a == LogicValue::LOW)  return LogicValue::HIGH;
    if (a == LogicValue::HIGH) return LogicValue::LOW;
    return LogicValue::UNKNOWN;
}

// --- Helpers -------------------------------------------------------------

/// True for LOW or HIGH.
constexpr bool isKnown(LogicValue v)
{
    return v == LogicValue::LOW || v == LogicValue::HIGH;
}

/// Converts a boolean to LOW / HIGH.
constexpr LogicValue toLogic(bool b)
{
    return b ? LogicValue::HIGH : LogicValue::LOW;
}

/// The printable character for @p v: '0', '1', 'X' or 'Z'.
constexpr char toChar(LogicValue v)
{
    return static_cast<char>(v);
}

/**
 * @brief Parses '0', '1', 'x'/'X' or 'z'/'Z'.
 * @return The value, or std::nullopt for any other character.
 */
constexpr std::optional<LogicValue> logicFromChar(char c)
{
    switch (c)
    {
        case '0': return LogicValue::LOW;
        case '1': return LogicValue::HIGH;
        case 'x': case 'X': return LogicValue::UNKNOWN;
        case 'z': case 'Z': return LogicValue::HIGHZ;
        default: return std::nullopt;
    }
}

/**
 * @brief How a gate input sees a net: a floating (Z) input reads as UNKNOWN.
 *
 * Gates never pass HIGHZ through a logic function; only a driver that is
 * deliberately released (bufif/notif) puts Z on its output.
 */
constexpr LogicValue asInput(LogicValue v)
{
    return v == LogicValue::HIGHZ ? LogicValue::UNKNOWN : v;
}

#endif
