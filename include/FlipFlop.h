/**
 * @file FlipFlop.h
 * @author Sebastian Paulus
 * @date 2026/09/24
 * @brief Edge-triggered D, T, JK and SR flip-flops with async set / reset.
 */

#ifndef FLIPFLOP_H
#define FLIPFLOP_H

#include <cstddef>
#include <string>
#include <vector>
#include "Gate.h"
#include "Types.h"

/**
 * @class FlipFlop
 * @brief An edge-triggered single-bit storage element.
 *
 * Pin layout (inputs), fixed per type so a GUI can label pins by index:
 *  - D:  {D, CLK, SET, RESET}
 *  - T:  {T, CLK, SET, RESET}
 *  - JK: {J, K, CLK, SET, RESET}
 *  - SR: {S, R, CLK, SET, RESET}
 *  - outputs: {Q, QN}
 *
 * Use clockPin(), setPin() and resetPin() for the control pin indices.
 * Trailing pins may be omitted; they are padded with unconnected pins, so
 * <tt>FlipFlop(Type::D, {&d, &clk}, {&q}, "ff")</tt> is a plain DFF with
 * no QN.
 *
 * Behaviour:
 *  - SET / RESET are asynchronous and active-high; RESET wins if both are
 *    high. A floating (HIGHZ / unconnected) SET or RESET is inactive, an
 *    UNKNOWN one makes the state UNKNOWN unless it already matches.
 *  - The state only changes on a clean edge (LOW to HIGH for Edge::Rising).
 *    A clock passing through UNKNOWN (LOW to X, or X to HIGH) is a possible
 *    edge: the state becomes UNKNOWN unless the edge would not have changed
 *    it. Transitions from or to HIGHZ are ignored, so an undriven clock at
 *    power-up never counts as an edge.
 *  - Floating data inputs read as UNKNOWN.
 *  - Outputs are committed in the Update region, so with zero delay every
 *    flip-flop on the same clock samples before any of them changes.
 *
 * @code
 *   // D flip-flop on the falling edge, with an async reset.
 *   FlipFlop& ff = sim.addFlipFlop(FlipFlop::Type::D,
 *                                  {&d, &clk, nullptr, &rst}, {&q, &qn},
 *                                  "ff0", FlipFlop::Edge::Falling);
 * @endcode
 */
class FlipFlop : public Gate
{
public:
    /// Which flip-flop.
    enum class Type
    {
        D,  ///< Loads D.
        T,  ///< Toggles when T is HIGH.
        JK, ///< J sets, K resets, both toggle.
        SR  ///< S sets, R resets, both is UNKNOWN.
    };

    /// Which clock edge loads the flip-flop.
    enum class Edge
    {
        Rising, ///< LOW to HIGH.
        Falling ///< HIGH to LOW.
    };

    /**
     * @brief Builds the flip-flop and wires it into the netlist.
     *
     * @param t       Which flip-flop.
     * @param inputs  Data pins, then CLK, SET and RESET, as laid out in the
     *                class description. nullptr leaves a pin unconnected.
     * @param outputs {Q, QN}; either may be omitted or nullptr.
     * @param name    Descriptive name (diagnostics, GUI, VCD output).
     * @param edge    Clock edge that loads the flip-flop.
     *
     * @throws std::invalid_argument if more pins are given than the layout
     *         has.
     */
    FlipFlop(Type t,
             std::vector<Net*> inputs,
             std::vector<Net*> outputs,
             std::string name,
             Edge edge = Edge::Rising);

    /// @brief Which flip-flop this is.
    /// @return The type passed to the constructor.
    Type type() const { return type_; }

    /// @brief Clock edge that loads the flip-flop.
    /// @return The edge passed to the constructor.
    Edge edge() const { return edge_; }

    /**
     * @brief Number of data pins before CLK.
     * @param t The type.
     * @return 1 for D and T, 2 for JK and SR.
     */
    static std::size_t dataPinCount(Type t);

    /**
     * @brief Total input pin count for a type.
     * @param t The type.
     * @return Data pins + CLK + SET + RESET.
     */
    static std::size_t inputCount(Type t) { return dataPinCount(t) + 3; }

    /// @brief Input pin index of CLK.
    /// @return dataPinCount() of this type.
    std::size_t clockPin() const { return dataPinCount(type_); }

    /// @brief Input pin index of SET (asynchronous).
    /// @return clockPin() + 1.
    std::size_t setPin() const { return dataPinCount(type_) + 1; }

    /// @brief Input pin index of RESET (asynchronous).
    /// @return clockPin() + 2.
    std::size_t resetPin() const { return dataPinCount(type_) + 2; }

    /// @brief The stored bit, for display (e.g. a GUI showing the contents).
    /// @return The current state.
    LogicValue state() const { return state_; }

    /**
     * @brief State at power-up and after Simulator::reset().
     *
     * Also sets the current state, so call it before simulating (or reset).
     *
     * @param v The state; LOW by default.
     */
    void setInitialState(LogicValue v) { initial_ = state_ = v; }

    /// @brief State at power-up and after Simulator::reset().
    /// @return The value set by setInitialState(), LOW by default.
    LogicValue initialState() const { return initial_; }

    /// @brief Stage count from clock to Q.
    /// @return 3.
    uint32_t intrinsicStages() const override { return 3; }

    /// @brief Type name used in diagnostics, generated gate names and the GUI.
    /// @return "dff", "tff", "jkff" or "srff".
    const char* typeName() const override;

    /// @brief Sequential element: outputs commit after every reader sampled.
    /// @return Region::Update.
    Region outputRegion() const override { return Region::Update; }

    /**
     * @brief Updates the state from the clock and control pins and sets Q and
     *        QN.
     *
     * See the class description for the rules, including UNKNOWN inputs.
     *
     * @param out {Q, QN}.
     */
    void computeOutputs(std::vector<LogicValue>& out) override;

protected:
    /// Restores initialState() and forgets the last clock level.
    void onReset() override;

private:
    /// @brief What a definite edge would load, given the data pins.
    /// @return The next state for this type.
    LogicValue nextState_() const;

    Type type_;                                 ///< Which flip-flop.
    Edge edge_;                                 ///< Clock edge that loads.
    LogicValue initial_ = LogicValue::LOW;      ///< Power-up state.
    LogicValue state_ = LogicValue::LOW;        ///< The stored bit.
    LogicValue last_clock_ = LogicValue::HIGHZ; ///< Clock at last evaluation.
};

#endif
