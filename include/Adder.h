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
 * @brief Adds two N-bit numbers plus a carry-in, computed behaviourally.
 *
 * Pin layout, least significant bit first:
 *  - inputs:  {A0..A(N-1), B0..B(N-1), CIN}
 *  - outputs: {S0..S(N-1), COUT}
 * Missing trailing pins are left unconnected. An unconnected (HIGHZ)
 * carry-in counts as 0, so a plain A + B needs no extra wiring.
 *
 * All outputs settle after one gate delay.
 */
class Adder : public Gate
{
public:
    /**
     * @param size Bits per operand, >= 1.
     * @throws std::invalid_argument if @p size is 0 or there are too many pins.
     */
    Adder(std::vector<Net*> inputs,
          std::vector<Net*> outputs,
          std::string name,
          std::size_t size);

    std::size_t size() const { return size_; }

    /// Worst-case path: the carry through every bit.
    uint32_t intrinsicStages() const override { return static_cast<uint32_t>(2 * size_); }
    const char* typeName() const override { return "adder"; }
    void computeOutputs(std::vector<LogicValue>& out) override;

private:
    std::size_t size_;
};

#endif
