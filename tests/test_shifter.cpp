#include <string>
#include <vector>
#include "Shifter.h"
#include "Simulator.h"
#include "TestFramework.h"

namespace
{
    constexpr LogicValue L = LogicValue::LOW;
    constexpr LogicValue H = LogicValue::HIGH;
    constexpr LogicValue X = LogicValue::UNKNOWN;
    using Type = Shifter::Type;

    /// Reference model on plain integers. s may be n or more.
    unsigned expected(Type t, unsigned d, unsigned s, unsigned n)
    {
        const unsigned mask = (1u << n) - 1;
        const bool sign = (d >> (n - 1)) & 1;
        switch (t)
        {
            case Type::SLL: return s >= n ? 0 : (d << s) & mask;
            case Type::SRL: return s >= n ? 0 : d >> s;
            case Type::SRA:
                if (s >= n) return sign ? mask : 0;
                return (d >> s) | (sign ? (mask & ~(mask >> s)) : 0);
            case Type::ROL: s %= n; return s ? ((d << s) | (d >> (n - s))) & mask : d;
            case Type::ROR: s %= n; return s ? ((d >> s) | (d << (n - s))) & mask : d;
        }
        return 0;
    }

    /// n-bit shifter of type t with an Input on every pin.
    struct Bench
    {
        Simulator sim;
        std::vector<Input*> d, s;
        std::vector<Net*> y;
        Shifter* sh = nullptr;

        Bench(Type t, std::size_t n)
        {
            std::vector<Net*> ins;
            for (std::size_t i = 0; i < n; ++i)
            {
                ins.push_back(&sim.addNet());
                d.push_back(&sim.addInput(ins.back()));
            }
            std::size_t k = 0;
            while ((std::size_t{1} << k) < n) ++k;
            for (std::size_t i = 0; i < k; ++i)
            {
                ins.push_back(&sim.addNet());
                s.push_back(&sim.addInput(ins.back()));
            }
            for (std::size_t i = 0; i < n; ++i) y.push_back(&sim.addNet());
            sh = &sim.emplace<Shifter>(t, ins, y, "sh", n);
        }

        void data(unsigned v)
        {
            for (std::size_t i = 0; i < d.size(); ++i) sim.setInput(*d[i], toLogic((v >> i) & 1));
        }
        void amount(std::vector<LogicValue> bits)    // S0 first
        {
            for (std::size_t i = 0; i < bits.size(); ++i) sim.setInput(*s[i], bits[i]);
            sim.runUntilIdle();
        }
        void amount(unsigned v)
        {
            std::vector<LogicValue> bits;
            for (std::size_t i = 0; i < s.size(); ++i) bits.push_back(toLogic((v >> i) & 1));
            amount(bits);
        }
        int out() const    // -1 if any output is not 0/1
        {
            int v = 0;
            for (std::size_t i = 0; i < y.size(); ++i)
            {
                if (!isKnown(y[i]->getValue())) return -1;
                if (y[i]->getValue() == H) v |= 1 << i;
            }
            return v;
        }
    };

    const Type kAll[] = {Type::SLL, Type::SRL, Type::SRA, Type::ROL, Type::ROR};
} // namespace

TEST(shifter_matches_integer_model_8bit)
{
    for (Type t : kAll)
    {
        Bench b(t, 8);
        CHECK_EQ(b.sh->shiftBits(), std::size_t{3});
        for (unsigned d : {0x00u, 0xFFu, 0x01u, 0x80u, 0xA5u, 0x3Cu, 0x96u})
        {
            b.data(d);
            for (unsigned s = 0; s < 8; ++s)
            {
                b.amount(s);
                CHECK_EQ(b.out(), static_cast<int>(expected(t, d, s, 8)));
            }
        }
    }
}

TEST(shifter_non_power_of_two_size)
{
    // 5 bits need 3 amount pins, so amounts 5..7 can appear on them.
    for (Type t : kAll)
    {
        Bench b(t, 5);
        for (unsigned d : {0x00u, 0x1Fu, 0x11u, 0x16u, 0x0Bu})
        {
            b.data(d);
            for (unsigned s = 0; s < 8; ++s)
            {
                b.amount(s);
                CHECK_EQ(b.out(), static_cast<int>(expected(t, d, s, 5)));
            }
        }
    }
}

TEST(shifter_unknown_amount_keeps_agreeing_bits)
{
    // SLL of 0b0000_0011 by 0 or 1 (S0 unknown): 0b011 or 0b110.
    Bench b(Type::SLL, 8);
    b.data(0x03);
    b.amount({X, L, L});
    CHECK_EQ(b.y[0]->getValue(), X);   // 1 or 0
    CHECK_EQ(b.y[1]->getValue(), H);   // 1 either way
    CHECK_EQ(b.y[2]->getValue(), X);   // 0 or 1
    CHECK_EQ(b.y[7]->getValue(), L);   // 0 either way

    // All-ones data rotated by any amount is still all ones.
    Bench r(Type::ROL, 8);
    r.data(0xFF);
    r.amount({X, X, X});
    CHECK_EQ(r.out(), 0xFF);
}

TEST(shifter_floating_amount_and_unknown_sign)
{
    // Amount pins left unconnected: zero data stays zero whatever the amount.
    Simulator sim;
    std::vector<Net*> d, y;
    for (int i = 0; i < 4; ++i)
    {
        d.push_back(&sim.addNet());
        sim.addInput(d.back(), "", L);
        y.push_back(&sim.addNet());
    }
    sim.emplace<Shifter>(Type::SRL, d, y, "srl", std::size_t{4});
    sim.runUntilIdle();
    for (Net* n : y) CHECK_EQ(n->getValue(), L);

    // SRA with an unknown sign bit fills with X.
    Bench b(Type::SRA, 4);
    b.data(0x1);
    b.sim.setInput(*b.d[3], X);
    b.amount(2u);
    CHECK_EQ(b.y[0]->getValue(), L);   // was D2
    CHECK_EQ(b.y[1]->getValue(), X);   // was D3 (the sign)
    CHECK_EQ(b.y[2]->getValue(), X);   // sign fill
    CHECK_EQ(b.y[3]->getValue(), X);
}

TEST(shifter_names_and_validation)
{
    const char* names[] = {"sll", "srl", "sra", "rol", "ror"};
    for (int i = 0; i < 5; ++i)
    {
        Bench b(kAll[i], 4);
        CHECK_EQ(std::string(b.sh->typeName()), std::string(names[i]));
        CHECK_EQ(b.sh->intrinsicStages(), uint32_t{4});
    }

    Simulator sim;
    Net& n = sim.addNet();
    using V = std::vector<Net*>;
    CHECK_THROWS(sim.emplace<Shifter>(Type::SLL, V{}, V{}, "a", std::size_t{1}));
    CHECK_THROWS(sim.emplace<Shifter>(Type::SLL, V{&n, &n, &n, &n}, V{}, "b", std::size_t{2}));
    CHECK_THROWS(sim.emplace<Shifter>(Type::SLL, V{}, V{&n, &n, &n}, "c", std::size_t{2}));
    CHECK_EQ(n.sinkCount(), std::size_t{0});
}
