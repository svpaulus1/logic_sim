/**
 * @file BuiltinGate.h
 * @author Sebastian Paulus
 * @date 2026/9/24
 * @brief One class covering every basic primitive gate, selected by an enum.
 */

#ifndef BUILTIN_GATE_H
#define BUILTIN_GATE_H

#include <cstddef>
#include <string>
#include <vector>
#include "Gate.h"
#include "Types.h"

/**
 * @class BuiltinGate
 * @brief A primitive logic gate whose behaviour is selected by Type.
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
     * Pin conventions, checked by assertion:
     *  - folds:     >=1 input, >=1 output (BUF/NOT drive every output alike)
     *  - enables:   inputs are {data, enable}, exactly 1 output
     *  - constants: no inputs, >=1 output
     */
    BuiltinGate(Type t,
                std::vector<Net*> inputs,
                std::vector<Net*> outputs,
                std::string name);

    Type type() const { return type_; }

    /// Lowercase Verilog keyword for this type ("nand", "bufif1", ...).
    const char* gateName() const;

    /// True if this type can stop driving, i.e. its decay delay is meaningful.
    bool canTristate() const;

    unsigned intrinsicStages() const override;
    void computeOutputs(std::vector<LogicValue>& out) const override;

private:
    /// How the inputs are consumed.
    enum class Shape { Fold, Enable, Constant };

    struct Info
    {
        const char* name;
        unsigned    stages;                          ///< CMOS stage count.
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
