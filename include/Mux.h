/**
 * @file Mux.h
 * @author Sebastian Paulus
 * @date 2026/11/15
 * @brief N-bit mux as a single primitive.
 */

#ifndef MUX_H
#define MUX_H

#include <cstddef>
#include <string>
#include <vector>
#include "Gate.h"
#include "Types.h"

/**
 * @class Mux
 * @brief Multiplexer: passes one of @c width data inputs, each @c bit_width
 *        bits wide, through to the outputs, chosen by the select lines.
 *
 * The number of select lines is the smallest @c k with 2^k >= width, so a
 * 2-input mux has 1, a 3- or 4-input mux has 2, and an 8-input mux has 3.
 *
 * Pin layout, least significant bit first (w = width, b = bit_width,
 * k = select lines):
 *  - inputs:  {D0[0..b-1], D1[0..b-1], ..., D(w-1)[0..b-1], S0..S(k-1)}
 *  - outputs: {Y[0..b-1]}
 *
 * So bit @c j of data input @c i is input pin <tt>i*b + j</tt>, and select
 * bit @c s is input pin <tt>w*b + s</tt>. Trailing pins may be omitted;
 * they are padded with unconnected pins, like FlipFlop and Adder.
 *
 * Behaviour:
 *  - All select bits known: the outputs copy the chosen input.
 *  - A select value with no matching input (only possible when @c width is
 *    not a power of two, e.g. 3 on a 3-input mux) makes every output UNKNOWN.
 *  - A select bit that is UNKNOWN or floating: an output bit keeps its value
 *    only if every input the select lines might be choosing agrees on it,
 *    and is UNKNOWN otherwise. If one of those possible choices has no
 *    matching input, every output is UNKNOWN.
 *  - Floating (HIGHZ / unconnected) data inputs read as UNKNOWN.
 *
 * All outputs change after one gate delay; intrinsicStages() sets how long
 * that is under StageDelayModel.
 *
 * @code
 *   // 4 inputs of 8 bits: 32 data pins + 2 select pins, 8 outputs.
 *   Mux& mux = sim.emplace<Mux>(inputs, outputs, "mux0", 4, 8);
 * @endcode
 */
class Mux : public Gate
{
public:
    /**
     * @brief Builds the mux and wires it into the netlist.
     *
     * @param inputs    Data pins, then select pins, in the order shown in the
     *                  class description. nullptr leaves a pin unconnected.
     * @param outputs   One pin per bit, Y0 first.
     * @param name      Descriptive name (diagnostics, GUI, VCD output).
     * @param width     Number of data inputs to choose between, at least 2.
     * @param bit_width Bits per data input and per output, at least 1.
     *
     * @throws std::invalid_argument if @p width is less than 2, @p bit_width
     *         is 0, or more pins are given than the layout has.
     */
    Mux(std::vector<Net*> inputs,
        std::vector<Net*> outputs,
        std::string name,
        std::size_t width,
        std::size_t bit_width);

    /// @brief Number of data inputs to choose between.
    /// @return The width passed to the constructor (at least 2).
    std::size_t width() const { return width_; }

    /// @brief Bits per data input; also the number of outputs.
    /// @return The bit_width passed to the constructor (at least 1).
    std::size_t bitWidth() const { return bit_width_; }

    /// @brief Number of select lines: the smallest k with 2^k >= width().
    /// @return 1 for 2 inputs, 2 for 3 or 4, 3 for 5 to 8, and so on.
    std::size_t selectBits() const { return select_bits_; }

    /**
     * @brief Two CMOS stages per level of a tree of 2:1 muxes.
     *
     * That is 2 for a 2:1 mux, 4 for 4:1 and 6 for 8:1, on the same scale
     * as BuiltinGate (AND = 2, XOR = 4).
     *
     * @return Twice selectBits().
     */
    uint32_t intrinsicStages() const override
    {
        return select_bits_ ? 2 * select_bits_ : 1;
    }

    /// @brief Type name used in diagnostics, generated gate names and the GUI.
    /// @return "mux".
    const char* typeName() const override { return "mux"; }

    /**
     * @brief Sets every output from the current data and select values.
     *
     * See the class description for how UNKNOWN or floating select lines
     * and out-of-range select values are handled.
     *
     * @param out One value per output, written in pin order.
     */
    void computeOutputs(std::vector<LogicValue>& out) override;
    
private:
    std::size_t width_;       ///< Number of data inputs.
    std::size_t bit_width_;   ///< Bits per data input and per output.
    std::size_t select_bits_; ///< Number of select lines, see selectBits().
};

#endif
