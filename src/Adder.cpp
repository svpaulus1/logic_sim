/**
 * @file Adder.cpp
 * @author Sebastian Paulus
 * @date 2026/11/07
 * @brief Implementation of Adder (see Adder.h).
 */

#include "Adder.h"
#include <stdexcept>

namespace
{
    /// @brief Pads @p pins with unconnected (nullptr) pins up to @p n.
    /// @param pins The pins given.
    /// @param n    Pins in the full layout.
    /// @param what "inputs" or "outputs", for the error message.
    /// @return @p pins, resized to @p n.
    /// @throws std::invalid_argument if more than @p n pins were given.
    std::vector<Net*> padded(std::vector<Net*> pins,
                             std::size_t n,
                             const char* what)
    {
        if (pins.size() > n)
            throw std::invalid_argument(std::string("Adder: too many ") + what);
        pins.resize(n, nullptr);
        return pins;
    }

    /// @brief Validates the operand size.
    /// @param size The value given.
    /// @return @p size, unchanged.
    /// @throws std::invalid_argument if @p size is 0.
    std::size_t checkedSize(std::size_t size)
    {
        if (size == 0)
            throw std::invalid_argument("Adder: size must be at least 1");
        return size;
    }
} // namespace

Adder::Adder(std::vector<Net*> inputs,
             std::vector<Net*> outputs,
             std::string name,
             std::size_t size)
    : Gate(padded(std::move(inputs), checkedSize(size) * 2 + 1, "inputs"),
           padded(std::move(outputs), size + 1, "outputs"),
           std::move(name)),
      size_(size)
{}

void Adder::computeOutputs(std::vector<LogicValue>& out)
{
    // Unconnected carry-in means "no carry", not "unknown".
    LogicValue carry = readInput_(size_ * 2);
    carry = (carry == LogicValue::HIGHZ) ? LogicValue::LOW : asInput(carry);

    for (std::size_t i = 0; i < size_; ++i)
    {
        const LogicValue a = asInput(readInput_(i));
        const LogicValue b = asInput(readInput_(i + size_));
        out[i] = a ^ b ^ carry;
        carry = (a & b) | (a & carry) | (b & carry);
    }
    out[size_] = carry;
}
