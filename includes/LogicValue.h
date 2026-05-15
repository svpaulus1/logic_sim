/**
 * @file   LogicValue.h
 * @brief  Enum declaration of LogicValue
 *
 * This file contains the LogicValue declaration that is used by objects to
 * simulate a logical value, either Low, High, Unknown, or High Impedance.
 *
 * @author Sebastian Paulus
 * @date   2026-5-15
 */


#ifndef LOGICVALUE_H
#define LOGICVALUE_H

/**
 * @brief Represents the fundamental states of a digital logic signal.
 *
 * This strongly-typed enum is designed for hardware simulation contexts
 * where standard booleans are insufficient. It models physical wire states,
 * including indeterminate voltages and disconnected lines.
 */
enum class LogicValue : char
{
    LOW     = '0', ///< Logic level 0 (Ground / False).
    HIGH    = '1', ///< Logic level 1 (Vcc / True).
    UNKNOWN = 'X', ///< Indeterminate state.
    HIGHZ   = 'Z'  ///< High impedance state (disconnected / floating wire).
};

#endif
