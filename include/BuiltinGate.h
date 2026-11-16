/**
 * @file BuiltinGate.h
 * @author Sebastian Paulus
 * @date 2026/09/24
 * @brief One class covering every basic primitive gate, selected by an enum.
 */

#ifndef BUILTIN_GATE_H
#define BUILTIN_GATE_H

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "Gate.h"
#include "Types.h"

/**
 * @class BuiltinGate
 * @brief A primitive logic gate whose behaviour is selected by Type.
 *
 * Covers the Verilog gate primitives. Pin layout by kind:
 *  - folds (AND, NAND, OR, NOR, XOR, XNOR): inputs {A0..A(n-1)}, n >= 1,
 *    outputs {Y}.
 *  - BUF / NOT: inputs {A}, outputs {Y0..Y(m-1)}, m >= 1, all driven alike.
 *  - enable gates (BUFIF0, BUFIF1, NOTIF0, NOTIF1): inputs {DATA, EN},
 *    outputs {Y}.
 *  - constants (PULLUP, PULLDOWN): no inputs, outputs {Y0..Y(m-1)}, m >= 1.
 *
 * Behaviour:
 *  - A floating (HIGHZ / unconnected) input reads as UNKNOWN, so only the
 *    enable gates can ever put HIGHZ on a net.
 *  - An enable gate drives DATA (or its inverse) while EN is at its active
 *    level, releases its output to HIGHZ while EN is at the other level, and
 *    outputs UNKNOWN while EN is UNKNOWN or floating.
 *  - PULLUP / PULLDOWN drive at Strength::Pull, so any active driver on the
 *    same net overrides them, and they have no propagation delay.
 *
 * @code
 *   // 3-input NAND and a tri-state buffer onto a pulled-up bus.
 *   sim.addGate(BuiltinGate::Type::NAND, {&a, &b, &c}, {&y});
 *   sim.addGate(BuiltinGate::Type::BUFIF1, {&data, &en}, {&bus});
 *   sim.addGate(BuiltinGate::Type::PULLUP, {}, {&bus});
 * @endcode
 */
class BuiltinGate : public Gate
{
public:
    /// Which primitive the gate is (the lowercase names are Verilog keywords).
    enum class Type
    {
        AND,      ///< N-input AND.
        NAND,     ///< N-input NAND.
        OR,       ///< N-input OR.
        NOR,      ///< N-input NOR.
        XOR,      ///< N-input XOR (odd parity).
        XNOR,     ///< N-input XNOR (even parity).
        BUF,      ///< Buffer: 1 input, may drive several outputs.
        NOT,      ///< Inverter: 1 input, may drive several outputs.
        BUFIF0,   ///< Tri-state buffer, enabled while EN is LOW.
        BUFIF1,   ///< Tri-state buffer, enabled while EN is HIGH.
        NOTIF0,   ///< Tri-state inverter, enabled while EN is LOW.
        NOTIF1,   ///< Tri-state inverter, enabled while EN is HIGH.
        PULLUP,   ///< Constant HIGH at Pull strength, no inputs.
        PULLDOWN, ///< Constant LOW at Pull strength, no inputs.
        COUNT     ///< Table sizing only, never a gate.
    };

    /**
     * @brief Constructs a gate of the given type and wires it into the netlist.
     *
     * @param t       Which primitive.
     * @param inputs  Input pins, as laid out in the class description.
     *                nullptr leaves a pin unconnected.
     * @param outputs Output pins.
     * @param name    Descriptive name (diagnostics, GUI, VCD output).
     *
     * @throws std::invalid_argument if the pin counts break the rules in the
     *         class description.
     */
    BuiltinGate(Type t,
                std::vector<Net*> inputs,
                std::vector<Net*> outputs,
                std::string name);

    /// @brief Which primitive this gate is.
    /// @return The type passed to the constructor.
    Type type() const { return type_; }

    /// @brief Lowercase Verilog keyword for this gate's type.
    /// @return "nand", "bufif1", ...
    const char* gateName() const;

    /**
     * @brief Lowercase Verilog keyword for a type.
     * @param t The type.
     * @return "and", "pullup", ...
     * @throws std::invalid_argument if @p t is Type::COUNT.
     */
    static const char* gateName(Type t);

    /**
     * @brief Parses a Verilog keyword.
     * @param name "and", "bufif1", ...
     * @return The type, or std::nullopt if @p name is not a primitive.
     */
    static std::optional<Type> typeFromName(std::string_view name);

    /**
     * @brief Validates pin counts for @p t without building a gate.
     *
     * @param t           The type.
     * @param num_inputs  Input pins the gate would have.
     * @param num_outputs Output pins the gate would have.
     *
     * @throws std::invalid_argument describing the first rule broken.
     */
    static void checkPins(Type t,
                          std::size_t num_inputs,
                          std::size_t num_outputs);

    /// @brief Tells whether this type can stop driving, i.e. whether its decay
    ///        delay is meaningful.
    /// @return true for BUFIF0, BUFIF1, NOTIF0 and NOTIF1.
    bool canTristate() const;

    /// @brief CMOS stage count for this type (NAND = 1, AND = 2, XOR = 4).
    /// @return The stage count from the gate table.
    uint32_t intrinsicStages() const override;

    /// @brief Type name used in diagnostics, generated gate names and the GUI.
    /// @return Same as gateName().
    const char* typeName() const override { return gateName(); }

    /// @brief Tells whether the TimingModel sets this gate's delays.
    /// @return false for PULLUP / PULLDOWN (resistors, no delay), true
    ///         otherwise.
    bool usesTimingModel() const override;

    /**
     * @brief Sets every output from the current input values.
     *
     * See the class description for how floating inputs and the enable
     * gates are handled.
     *
     * @param out One value per output, written in pin order.
     */
    void computeOutputs(std::vector<LogicValue>& out) override;

private:
    /// How the inputs are consumed.
    enum class Shape
    {
        Fold,    ///< Combine every input with one operator.
        Enable,  ///< {data, enable}, can release to HIGHZ.
        Constant ///< No inputs, fixed output.
    };

    /// One row of the gate table: everything that differs between types.
    struct Info
    {
        const char* name;                            ///< Verilog keyword.
        uint32_t    stages;                          ///< CMOS stage count.
        Shape       shape;                           ///< How inputs are used.
        LogicValue (*fold)(LogicValue, LogicValue);  ///< Fold shape only.
        bool        invert;                          ///< Fold and Enable.
        LogicValue  enable_active;                   ///< Enable shape only.
        LogicValue  constant;                        ///< Constant shape only.
    };

    /**
     * @brief Looks up the table row for a type.
     * @param t The type.
     * @return Its row.
     * @throws std::invalid_argument if @p t is Type::COUNT.
     */
    static const Info& info_(Type t);

    Type type_; ///< Which primitive.
};

#endif
