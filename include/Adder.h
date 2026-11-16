/**
 * @file Adder.h
 * @author Sebastian Paulus
 * @date 2026/11/07
 * @brief N-bit binary adder as a single primitive.
 */

#ifndef ADDER_H
#define ADDER_H

#include <cstddef>
#include <string>
#include <vector>
#include "Gate.h"
#include "Types.h"

/**
 * @class Adder
 * @brief Adds two @c size bit numbers plus a carry-in, computed
 *        behaviourally.
 *
 * Pin layout, least significant bit first (n = size):
 *  - inputs:  {A0..A(n-1), B0..B(n-1), CIN}
 *  - outputs: {S0..S(n-1), COUT}
 *
 * Trailing pins may be omitted; they are padded with unconnected pins, like
 * Mux and Register.
 *
 * Behaviour:
 *  - S = A + B + CIN, with the carry out of the top bit on COUT.
 *  - An unconnected (HIGHZ) carry-in counts as 0, so a plain A + B needs no
 *    extra wiring.
 *  - Floating A or B bits read as UNKNOWN. The carry uses the majority form,
 *    so it stays known whenever two of a bit's three inputs agree (1 + X + 1
 *    always carries).
 *
 * All outputs change after one gate delay: this models the adder's result,
 * not the ripple of the carry through its stages. intrinsicStages() sets how
 * long that delay is under StageDelayModel.
 *
 * @code
 *   // 8-bit adder: A0..A7, B0..B7, CIN; outputs S0..S7, COUT.
 *   Adder& add = sim.addAdder(inputs, outputs, 8);
 * @endcode
 */
class Adder : public Gate
{
public:
    /**
     * @brief Builds the adder and wires it into the netlist.
     *
     * @param inputs  A pins, then B pins, then CIN, in the order shown in the
     *                class description. nullptr leaves a pin unconnected.
     * @param outputs Sum pins, S0 first, then COUT.
     * @param name    Descriptive name (diagnostics, GUI, VCD output).
     * @param size    Bits per operand, at least 1.
     *
     * @throws std::invalid_argument if @p size is 0 or more pins are given
     *         than the layout has.
     */
    Adder(std::vector<Net*> inputs,
          std::vector<Net*> outputs,
          std::string name,
          std::size_t size);

    /// @brief Bits per operand.
    /// @return The size passed to the constructor (at least 1).
    std::size_t size() const { return size_; }

    /**
     * @brief Worst-case path: the carry through every bit, about two CMOS
     *        stages per bit.
     * @return Twice size().
     */
    uint32_t intrinsicStages() const override { return static_cast<uint32_t>(2 * size_); }

    /// @brief Type name used in diagnostics, generated gate names and the GUI.
    /// @return "adder".
    const char* typeName() const override { return "adder"; }

    /**
     * @brief Sets every output from the current input values.
     *
     * See the class description for how floating inputs are handled.
     *
     * @param out One value per output, written in pin order.
     */
    void computeOutputs(std::vector<LogicValue>& out) override;

private:
    std::size_t size_; ///< Bits per operand.
};

#endif
