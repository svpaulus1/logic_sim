// File: Types.h

#ifndef TYPES_H
#define TYPES_H

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

#endif
