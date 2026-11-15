/**
 * @file Shifter.h
 * @author Sebastian Paulus
 * @date 2026/11/15
 * @brief N-bit shifter as a single primitive.
 */

#ifndef SHIFTER_H
#define SHIFTER_H

#include <cstddef>
#include <string>
#include <vector>
#include "Gate.h"
#include "Types.h"

/**
 * @class Shifter
 * @brief Barrel shifter: shifts or rotates an @c size bit value by an amount
 *        read from its shift-amount pins.
 *
 * The number of shift-amount pins is the smallest @c k with 2^k >= size, so
 * an 8-bit shifter has 3 (amounts 0 to 7) and a 5-bit shifter also has 3.
 *
 * Pin layout, least significant bit first (n = size, k = shift-amount pins):
 *  - inputs:  {D0..D(n-1), S0..S(k-1)}
 *  - outputs: {Y0..Y(n-1)}
 *
 * "Left" means towards the most significant bit. For a shift amount @c s:
 *  - SLL: Y[i] = D[i-s], zeros shifted in at the bottom.
 *  - SRL: Y[i] = D[i+s], zeros shifted in at the top.
 *  - SRA: Y[i] = D[i+s], copies of the sign bit D(n-1) shifted in at the top.
 *  - ROL: Y[i] = D[(i-s) mod n], bits leaving the top come back at the bottom.
 *  - ROR: Y[i] = D[(i+s) mod n], bits leaving the bottom come back at the top.
 *
 * When @c size is not a power of two, amounts of @c size or more can appear
 * on the pins: shifts then move every bit out (all zeros, or all sign bits
 * for SRA) and rotates wrap around (amount mod size).
 *
 * Unknown values:
 *  - Floating (HIGHZ / unconnected) data inputs read as UNKNOWN.
 *  - A shift-amount bit that is UNKNOWN or floating: an output bit keeps its
 *    value only if every amount the pins might hold gives the same value,
 *    and is UNKNOWN otherwise (the same rule as Mux's select lines).
 *
 * All outputs change after one gate delay; intrinsicStages() sets how long
 * that is under StageDelayModel.
 *
 * @code
 *   // 8-bit arithmetic right shift: 8 data pins + 3 amount pins, 8 outputs.
 *   Shifter& sra = sim.emplace<Shifter>(Shifter::Type::SRA, inputs, outputs, "sra0", 8);
 * @endcode
 */
class Shifter : public Gate
{
public:
    /// Which shift or rotate the component performs.
    enum class Type
    {
        SLL, ///< Shift left logical: zeros in at the bottom.
        SRL, ///< Shift right logical: zeros in at the top.
        SRA, ///< Shift right arithmetic: sign bit copied in at the top.
        ROL, ///< Rotate left.
        ROR  ///< Rotate right.
    };

    /**
     * @brief Builds the shifter and wires it into the netlist.
     *
     * @param t       Which shift or rotate to perform.
     * @param inputs  Data pins, then shift-amount pins, in the order shown in
     *                the class description. nullptr leaves a pin unconnected.
     * @param outputs One pin per bit, Y0 first.
     * @param name    Descriptive name (diagnostics, GUI, VCD output).
     * @param size    Bits in the value being shifted, at least 2.
     *
     * @throws std::invalid_argument if @p size is less than 2 or more pins
     *         are given than the layout has.
     */
    Shifter(Type t,
            std::vector<Net*> inputs,
            std::vector<Net*> outputs,
            std::string name,
            std::size_t size);

    /// @brief Which shift or rotate this component performs.
    /// @return The type passed to the constructor.
    Type type() const { return type_; }

    /// @brief Bits in the value being shifted; also the number of outputs.
    /// @return The size passed to the constructor (at least 2).
    std::size_t size() const { return size_; }

    /// @brief Number of shift-amount pins: the smallest k with 2^k >= size().
    /// @return 1 for 2 bits, 2 for 3 or 4, 3 for 5 to 8, and so on.
    std::size_t shiftBits() const { return shift_bits_; }

    /**
     * @brief Two CMOS stages per level of a barrel shifter.
     *
     * A barrel shifter is shiftBits() levels of 2:1 muxes, one per amount
     * bit, so this uses the same scale as Mux and BuiltinGate.
     *
     * @return Twice shift_bits().
     */
    uint32_t intrinsicStages() const override
    {
        return static_cast<uint32_t>(2 * shift_bits_);
    }

    /// @brief Type name used in diagnostics, generated gate names and the GUI.
    /// @return "sll", "srl", "sra", "rol" or "ror".
    const char* typeName() const override;

    /**
     * @brief Sets every output from the current data and shift-amount values.
     *
     * See the class description for how UNKNOWN or floating shift-amount
     * pins and amounts of size() or more are handled.
     *
     * @param out One value per output, written in pin order.
     */
    void computeOutputs(std::vector<LogicValue>& out) override;
    
private:
    /**
     * @brief The value output bit @p i would have if the amount were @p amount.
     * @param i      Output bit, 0 = least significant.
     * @param amount Shift amount, may be size() or more.
     * @return The selected data bit (floating reads as UNKNOWN) or the fill bit.
     */
    LogicValue shiftedBit_(std::size_t i, std::size_t amount) const;

    Type type_;              ///< Which shift or rotate.
    std::size_t size_;       ///< Bits in the value being shifted.
    std::size_t shift_bits_; ///< Number of shift-amount pins, see shiftBits().
};

#endif
