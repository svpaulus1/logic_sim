/**
 * @file FlipFlop.h
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
 * @brief An edge-triggered storage element.
 *
 * Pin layout (inputs), fixed per type so a GUI can label pins by index:
 *  - D:  {D, CLK, SET, RESET}
 *  - T:  {T, CLK, SET, RESET}
 *  - JK: {J, K, CLK, SET, RESET}
 *  - SR: {S, R, CLK, SET, RESET}
 * Outputs: {Q, QN}.
 *
 * Trailing pins may be omitted (they are padded with unconnected pins), so
 * `FlipFlop(Type::D, {&d, &clk}, {&q})` is a plain DFF with no QN.
 *
 * Behaviour:
 *  - SET / RESET are asynchronous and active-high; RESET wins if both are
 *    high. A floating (HIGHZ / unconnected) SET or RESET is inactive, an
 *    UNKNOWN one makes the state UNKNOWN unless it already matches.
 *  - The state only changes on a clean edge (LOW->HIGH for Rising). A clock
 *    passing through UNKNOWN (LOW->X or X->HIGH) is a possible edge: the
 *    state becomes UNKNOWN unless the edge would not have changed it.
 *    Transitions from or to HIGHZ are ignored, so an undriven clock at
 *    power-up never counts as an edge.
 *  - Outputs are committed in the Update region, so with zero delay every
 *    flip-flop on the same clock samples before any of them changes.
 */
class FlipFlop : public Gate
{
public:
    enum class Type { D, T, JK, SR };
    enum class Edge { Rising, Falling };

    /**
     * @throws std::invalid_argument if there are too many pins.
     */
    FlipFlop(Type t,
             std::vector<Net*> inputs,
             std::vector<Net*> outputs,
             std::string name,
             Edge edge = Edge::Rising);

    Type type() const { return type_; }
    Edge edge() const { return edge_; }

    /// Number of data pins before CLK (1 for D and T, 2 for JK and SR).
    static std::size_t dataPinCount(Type t);
    /// Total input pin count for @p t (data + CLK + SET + RESET).
    static std::size_t inputCount(Type t) { return dataPinCount(t) + 3; }

    std::size_t clockPin() const { return dataPinCount(type_); }
    std::size_t setPin() const { return dataPinCount(type_) + 1; }
    std::size_t resetPin() const { return dataPinCount(type_) + 2; }

    /// The stored bit.
    LogicValue state() const { return state_; }

    /// State at power-up and after Simulator::reset(). Defaults to LOW.
    /// Also sets the current state, so call it before simulating (or reset).
    void setInitialState(LogicValue v) { initial_ = state_ = v; }
    LogicValue initialState() const { return initial_; }

    uint32_t intrinsicStages() const override { return 3; }
    const char* typeName() const override;
    Region outputRegion() const override { return Region::Update; }
    void computeOutputs(std::vector<LogicValue>& out) override;

protected:
    void onReset() override;

private:
    /// What a definite edge would load, given the data pins.
    LogicValue nextState_() const;

    Type type_;
    Edge edge_;
    LogicValue initial_ = LogicValue::LOW;
    LogicValue state_ = LogicValue::LOW;
    LogicValue last_clock_ = LogicValue::HIGHZ;
};

#endif
