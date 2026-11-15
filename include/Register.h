/**
 * @file Register.h
 * @author Sebastian Paulus
 * @date 2026/11/15
 * @brief N-bit register as a single primitive.
 */

#ifndef REGISTER_H
#define REGISTER_H

#include <cstddef>
#include <string>
#include <vector>
#include "FlipFlop.h"
#include "Gate.h"
#include "Types.h"

/**
 * @class Register
 * @brief Stores an @c size bit value, loaded on a clock edge.
 *
 * Pin layout, least significant bit first (n = size):
 *  - inputs:  {D0..D(n-1), CLK, EN, RESET, OE}
 *  - outputs: {Q0..Q(n-1)}
 *
 * Use clockPin(), enablePin(), resetPin() and outputEnablePin() for the
 * control pin indices. Trailing pins may be omitted; they are padded with
 * unconnected pins, so a plain register only needs the data pins and CLK.
 *
 * Behaviour:
 *  - CLK: the value on D is stored on a clean edge (LOW to HIGH for
 *    Edge::Rising) while EN is active. Transitions from or to HIGHZ are
 *    ignored, so an undriven clock at power-up is never an edge.
 *  - EN (load enable): HIGH or unconnected loads on the edge, LOW holds.
 *  - RESET: asynchronous and active-high; clears every bit to 0 at once,
 *    with no clock needed, and wins over a clock edge. Unconnected is
 *    inactive.
 *  - OE (output enable): HIGH or unconnected drives Q; LOW releases Q to
 *    HIGHZ, so several registers can share a bus. The stored value is kept
 *    either way.
 *  - Outputs are committed in the Update region, so with zero delay every
 *    register and flip-flop on the same clock samples before any changes.
 *
 * Unknown values, as in FlipFlop:
 *  - Floating (HIGHZ / unconnected) data inputs read as UNKNOWN.
 *  - A clock passing through UNKNOWN (LOW to X, or X to HIGH) or an UNKNOWN
 *    EN on an edge might or might not load: each bit becomes UNKNOWN unless
 *    loading would not have changed it.
 *  - An UNKNOWN RESET makes each bit UNKNOWN unless it is already 0.
 *  - An UNKNOWN OE makes every output UNKNOWN.
 *
 * @code
 *   // 8-bit register: D0..D7, CLK, EN; RESET and OE left unconnected.
 *   std::vector<Net*> ins(d.begin(), d.end());
 *   ins.push_back(&clk);
 *   ins.push_back(&load);
 *   Register& reg = sim.emplace<Register>(ins, q, "a_reg", 8);
 * @endcode
 */
class Register : public Gate
{
public:
    /// Which clock edge loads the register.
    enum class Edge
    {
        Rising, ///< LOW to HIGH.
        Falling ///< HIGH to LOW.
    };

    /**
     * @brief Builds the register and wires it into the netlist.
     *
     * @param inputs  Data pins, then CLK, EN, RESET and OE, in the order shown
     *                in the class description. nullptr leaves a pin
     *                unconnected.
     * @param outputs One pin per bit, Q0 first.
     * @param name    Descriptive name (diagnostics, GUI, VCD output).
     * @param size    Bits stored, at least 1.
     * @param edge    Clock edge that loads the register.
     *
     * @throws std::invalid_argument if @p size is 0 or more pins are given
     *         than the layout has.
     */
    Register(std::vector<Net*> inputs,
             std::vector<Net*> outputs,
             std::string name,
             std::size_t size,
             Edge edge = Edge::Rising);

    /// @brief Bits stored; also the number of data pins and outputs.
    /// @return The size passed to the constructor (at least 1).
    std::size_t size() const { return size_; }

    /// @brief Clock edge that loads the register.
    /// @return The edge passed to the constructor.
    Edge edge() const { return edge_; }

    /// @brief Input pin index of CLK.
    /// @return size().
    std::size_t clockPin() const { return size_; }

    /// @brief Input pin index of EN (load enable).
    /// @return size() + 1.
    std::size_t enablePin() const { return size_ + 1; }

    /// @brief Input pin index of RESET (asynchronous clear).
    /// @return size() + 2.
    std::size_t resetPin() const { return size_ + 2; }

    /// @brief Input pin index of OE (output enable).
    /// @return size() + 3.
    std::size_t outputEnablePin() const { return size_ + 3; }

    /// @brief The stored bits, for display (e.g. a GUI showing the contents).
    /// @return One value per bit, bit 0 first. Unaffected by OE.
    const std::vector<LogicValue>& state() const { return state_; }

    /**
     * @brief Value of every bit at power-up and after Simulator::reset().
     *
     * Also sets the current value, so call it before simulating (or reset).
     *
     * @param v Value for every bit; LOW by default.
     */
    void setInitialState(LogicValue v);

    /// @brief Value of every bit at power-up and after Simulator::reset().
    /// @return The value set by setInitialState(), LOW by default.
    LogicValue initialState() const { return initial_; }

    /// @brief Same stage count as FlipFlop (clock to Q).
    /// @return 3.
    uint32_t intrinsicStages() const override { return 3; }

    /// @brief Type name used in diagnostics, generated gate names and the GUI.
    /// @return "register".
    const char* typeName() const override { return "register"; }

    /// @brief Sequential element: outputs commit after every reader sampled.
    /// @return Region::Update.
    Region outputRegion() const override { return Region::Update; }

    /**
     * @brief Updates the stored value from the clock and control pins and
     *        sets every output.
     *
     * See the class description for the rules, including UNKNOWN inputs.
     *
     * @param out One value per output, written in pin order.
     */
    void computeOutputs(std::vector<LogicValue>& out) override;

protected:
    /// Restores initialState() and forgets the last clock level.
    void onReset() override;

private:
    std::size_t size_;                           ///< Bits stored.
    Edge edge_;                                  ///< Clock edge that loads.
    LogicValue initial_ = LogicValue::LOW;       ///< Power-up value of every bit.
    std::vector<LogicValue> state_;              ///< Stored bits, bit 0 first.
    LogicValue last_clock_ = LogicValue::HIGHZ;  ///< Clock level at the last evaluation.
};

#endif
