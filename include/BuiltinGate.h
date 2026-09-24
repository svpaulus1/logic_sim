/**
 * @file BuiltinGate.h
 * @author Sebastian Paulus
 * @date 2026/9/24
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
 * Follows the Verilog gate primitives: a floating (HIGHZ) input is read as
 * UNKNOWN, so only the enable gates can ever put HIGHZ on a net, and pullup /
 * pulldown drive at Pull strength so any active driver overrides them.
 */
class BuiltinGate : public Gate
{
public:
    enum class Type
    {
        AND, NAND, OR, NOR, XOR, XNOR,  ///< N-input folds
        BUF, NOT,                       ///< 1 input, may drive several outputs
        BUFIF0, BUFIF1, NOTIF0, NOTIF1, ///< data + enable, can release to HIGHZ
        PULLUP, PULLDOWN,               ///< constant drivers, no inputs
        COUNT                           ///< table sizing only, never a gate
    };

    /**
     * @brief Constructs a gate of the given type and wires it into the netlist.
     *
     * Pin conventions:
     *  - folds:     >=1 input, exactly 1 output (BUF/NOT: exactly 1 input,
     *               >=1 outputs, all driven alike)
     *  - enables:   inputs are {data, enable}, exactly 1 output
     *  - constants: no inputs, >=1 output
     *
     * @throws std::invalid_argument if the pin counts break these rules.
     */
    BuiltinGate(Type t,
                std::vector<Net*> inputs,
                std::vector<Net*> outputs,
                std::string name);

    Type type() const { return type_; }

    /// Lowercase Verilog keyword for this type ("nand", "bufif1", ...).
    const char* gateName() const;

    /// Lowercase Verilog keyword for @p t.
    static const char* gateName(Type t);

    /// Parses a Verilog keyword ("and", "bufif1", ...); nullopt if unknown.
    static std::optional<Type> typeFromName(std::string_view name);

    /**
     * @brief Validates pin counts for @p t without building a gate.
     * @throws std::invalid_argument describing the first rule broken.
     */
    static void checkPins(Type t, std::size_t num_inputs, std::size_t num_outputs);

    /// True if this type can stop driving, i.e. its decay delay is meaningful.
    bool canTristate() const;

    uint32_t intrinsicStages() const override;
    const char* typeName() const override { return gateName(); }
    bool usesTimingModel() const override;
    void computeOutputs(std::vector<LogicValue>& out) override;

private:
    /// How the inputs are consumed.
    enum class Shape { Fold, Enable, Constant };

    struct Info
    {
        const char* name;
        uint32_t    stages;                          ///< CMOS stage count.
        Shape       shape;
        LogicValue (*fold)(LogicValue, LogicValue);  ///< Fold shape only.
        bool        invert;                          ///< Fold and Enable.
        LogicValue  enable_active;                   ///< Enable shape only.
        LogicValue  constant;                        ///< Constant shape only.
    };

    static const Info& info_(Type t);

    Type type_;
};

#endif
