/**
 * @file Mux.cpp
 * @author Sebastian Paulus
 * @date 2026/11/15
 * @brief Implementation of Mux (see Mux.h).
 */

#include "Mux.h"
#include <stdexcept>
#include <algorithm>

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
            throw std::invalid_argument(std::string("Mux: too many ") + what);
        pins.resize(n, nullptr);
        return pins;
    }

    /// @brief Validates the number of data inputs.
    /// @param width The value given.
    /// @return @p width, unchanged.
    /// @throws std::invalid_argument if @p width is less than 2.
    std::size_t checkedWidth(std::size_t width)
    {
        if (width < 2)
            throw std::invalid_argument("Mux: width must be at least 2");
        return width;
    }

    /// @brief Validates the bits per data input.
    /// @param bit_width The value given.
    /// @return @p bit_width, unchanged.
    /// @throws std::invalid_argument if @p bit_width is 0.
    std::size_t checkedBitWidth(std::size_t bit_width)
    {
        if (bit_width == 0)
            throw std::invalid_argument("Mux: bit_width must be at least 1");
        return bit_width;
    }

    /// @brief Select bits needed to pick one of @p n inputs.
    /// @param n Number of data inputs.
    /// @return The smallest k with 2^k >= @p n.
    std::size_t selectBitsFor(std::size_t n)
    {
        std::size_t k = 0;
        while (k < 64 && (std::size_t{1} << k) < n) ++k;
        return k;
    }
} // namespace

Mux::Mux(std::vector<Net*> inputs,
        std::vector<Net*> outputs,
        std::string name,
        std::size_t width,
        std::size_t bit_width)
    : Gate(padded(std::move(inputs),
                  checkedWidth(width) * checkedBitWidth(bit_width) + selectBitsFor(width),
                  "inputs"),
           padded(std::move(outputs), bit_width, "outputs"),
           std::move(name)),
      width_(width),
      bit_width_(bit_width),
      select_bits_(selectBitsFor(width))
{}

// Pin layout, least significant first:
//   inputs:  D0[0..b-1], D1[0..b-1], ..., D(w-1)[0..b-1], S0..S(k-1)
//   outputs: Y[0..b-1]
// where w = width_, b = bit_width_, k = select_bits_.
void Mux::computeOutputs(std::vector<LogicValue>& out)
{
    // Read the select lines into a number. Bits that are X or floating are
    // left as 0 in `chosen` and recorded in `unknown_bits`.
    const std::size_t select_base = width_ * bit_width_;
    std::size_t chosen = 0;
    std::size_t unknown_bits = 0;
    for (std::size_t s = 0; s < select_bits_; ++s)
    {
        const LogicValue v = asInput(readInput_(select_base + s));
        if (v == LogicValue::HIGH)
            chosen |= std::size_t{1} << s;
        else if (v == LogicValue::UNKNOWN)
            unknown_bits |= std::size_t{1} << s;
    }

    // The largest value the select lines could hold. If that is past the
    // last input (width_ not a power of two), the output can't be known.
    if ((chosen | unknown_bits) >= width_)
    {
        std::fill(out.begin(), out.end(), LogicValue::UNKNOWN);
        return;
    }

    // Common case: every select bit known, just copy the chosen input.
    if (unknown_bits == 0)
    {
        for (std::size_t bit = 0; bit < bit_width_; ++bit)
            out[bit] = asInput(readInput_(chosen * bit_width_ + bit));
        return;
    }

    // Some select bits unknown: an output bit has a value only if every
    // input the select lines might be choosing agrees on it.
    for (std::size_t bit = 0; bit < bit_width_; ++bit)
    {
        LogicValue result = asInput(readInput_(chosen * bit_width_ + bit));
        for (std::size_t i = 0; i < width_ && result != LogicValue::UNKNOWN; ++i)
        {
            if ((i & ~unknown_bits) != chosen) continue;
            if (asInput(readInput_(i * bit_width_ + bit)) != result)
                result = LogicValue::UNKNOWN;
        }
        out[bit] = result;
    }
}
