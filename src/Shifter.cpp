/**
 * @file Shifter.cpp
 * @author Sebastian Paulus
 * @date 2026/11/15
 * @brief Implementation of Shifter (see Shifter.h).
 */

#include "Shifter.h"
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
            throw std::invalid_argument(std::string("Shifter: too many ") + what);
        pins.resize(n, nullptr);
        return pins;
    }

    /// @brief Validates the width of the value.
    /// @param size The value given.
    /// @return @p size, unchanged.
    /// @throws std::invalid_argument if @p size is less than 2.
    std::size_t checkedSize(std::size_t size)
    {
        if (size < 2)
            throw std::invalid_argument("Shifter: size must be at least 2");
        return size;
    }

    /// @brief Shift-amount bits needed for a @p n bit value.
    /// @param n Bits in the value.
    /// @return The smallest k with 2^k >= @p n.
    std::size_t shiftBitsFor(std::size_t n)
    {
        std::size_t k = 0;
        while (k < 64 && (std::size_t{1} << k) < n) ++k;
        return k;
    }
} // namespace

Shifter::Shifter(Type t,
                 std::vector<Net*> inputs,
                 std::vector<Net*> outputs,
                 std::string name,
                 std::size_t size)
    : Gate(padded(std::move(inputs), checkedSize(size) + shiftBitsFor(size), "inputs"),
           padded(std::move(outputs), size, "outputs"),
           std::move(name)),
      type_(t),
      size_(size),
      shift_bits_(shiftBitsFor(size))
{}

const char* Shifter::typeName() const
{
    switch (type_)
    {
        case Type::SLL: return "sll";
        case Type::SRL: return "srl";
        case Type::SRA: return "sra";
        case Type::ROL: return "rol";
        case Type::ROR: return "ror";
    }
    return "shifter";
}

LogicValue Shifter::shiftedBit_(std::size_t i, std::size_t amount) const
{
    const std::size_t n = size_;

    switch (type_)
    {
        case Type::SLL:
            return i >= amount ? asInput(readInput_(i - amount)) : LogicValue::LOW;

        case Type::SRL:
            return i + amount < n ? asInput(readInput_(i + amount)) : LogicValue::LOW;

        case Type::SRA:
            return i + amount < n ? asInput(readInput_(i + amount))
                                  : asInput(readInput_(n - 1));   // sign bit

        case Type::ROL:
            return asInput(readInput_((i + n - amount % n) % n));

        case Type::ROR:
            return asInput(readInput_((i + amount) % n));
    }
    return LogicValue::UNKNOWN;
}

void Shifter::computeOutputs(std::vector<LogicValue>& out)
{
    // Read the shift amount (S0 least significant). Bits that are X or
    // floating are left as 0 in `amount` and recorded in `unknown_bits`.
    std::size_t amount = 0;
    std::size_t unknown_bits = 0;
    for (std::size_t s = 0; s < shift_bits_; ++s)
    {
        const LogicValue v = asInput(readInput_(size_ + s));
        if (v == LogicValue::HIGH)
            amount |= std::size_t{1} << s;
        else if (v == LogicValue::UNKNOWN)
            unknown_bits |= std::size_t{1} << s;
    }

    // Common case: the amount is known.
    if (unknown_bits == 0)
    {
        for (std::size_t i = 0; i < size_; ++i)
            out[i] = shiftedBit_(i, amount);
        return;
    }

    // Some amount bits unknown: an output bit keeps its value only if every
    // amount the pins might hold gives the same value.
    const std::size_t amounts = std::size_t{1} << shift_bits_;
    for (std::size_t i = 0; i < size_; ++i)
    {
        LogicValue result = shiftedBit_(i, amount);
        for (std::size_t a = 0; a < amounts && result != LogicValue::UNKNOWN; ++a)
        {
            if ((a & ~unknown_bits) != amount) continue;
            if (shiftedBit_(i, a) != result)
                result = LogicValue::UNKNOWN;
        }
        out[i] = result;
    }
}
