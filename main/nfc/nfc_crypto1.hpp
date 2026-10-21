#pragma once
#include <cstdint>

namespace nfc_app {
class Crypto1Local {
    public:
        void init(uint64_t key48)
        {
            odd_ = 0U;
            even_ = 0U;

            for (int bit = 47; bit > 0; bit -= 2) {
                odd_ = (odd_ << 1) | static_cast<uint32_t>((key48 >> (((bit - 1) ^ 7))) & 0x01ULL);
                even_ = (even_ << 1) | static_cast<uint32_t>((key48 >> ((bit ^ 7))) & 0x01ULL);
            }
        }

        uint32_t inject(uint32_t uid, uint32_t nt, bool encrypted = false)
        {
            return step32(uid ^ nt, encrypted);
        }

        bool step_with(bool in, bool enc = false)
        {
            const uint8_t out_bit = filter();
            uint32_t feedback = enc ? static_cast<uint32_t>(out_bit) : 0U;

            feedback ^= in ? 1U : 0U;
            feedback ^= (odd_ & 0x0029CE5CU);
            feedback ^= (even_ & 0x00870804U);

            even_ = ((even_ << 1) & 0x00FFFFFFU) | evenparity32(feedback);

            const uint32_t previous_odd = odd_;
            odd_ = even_;
            even_ = previous_odd;

            return out_bit != 0;
        }

        uint8_t step8(uint8_t in, bool enc = false, uint8_t *last_ks_bit = nullptr)
        {
            uint8_t v = 0U;
            for (uint8_t bit = 0U; bit < 8U; ++bit) {
                v |= static_cast<uint8_t>(step_with(((in >> bit) & 0x01U) != 0U, enc)) << bit;
            }
            if (last_ks_bit) *last_ks_bit = parity_keystream_bit();
            return v;
        }

        uint8_t parity_keystream_bit() const
        {
            return filter();
        }

        uint32_t step32(uint32_t in, bool enc = false)
        {
            uint32_t out = 0U;
            for (uint8_t bit = 0U; bit < 32U; ++bit) {
                out |= static_cast<uint32_t>(step_with(crypto_big_endian_bit(in, bit) != 0U, enc)) << (bit ^ 24U);
            }
            return out;
        }

        static uint8_t oddparity8(uint8_t x)
        {
            return static_cast<uint8_t>(!__builtin_parity(static_cast<unsigned int>(x)));
        }

        uint8_t encrypt_nr_ar(uint8_t out[8], uint32_t nr, uint32_t ar)
        {
            uint8_t parity = 0;
            for (uint_fast8_t i = 0; i < 4; ++i) {
                const uint8_t v = static_cast<uint8_t>((nr >> ((i ^ 0x03U) << 3)) & 0xFFU);
                out[i] = static_cast<uint8_t>(step8(v, false) ^ v);
                parity |= static_cast<uint8_t>((parity_keystream_bit() ^ oddparity8(v)) & 0x01U) << i;
            }
            for (uint_fast8_t pos = 4; pos < 8; ++pos) {
                const uint8_t i = static_cast<uint8_t>(pos - 4);
                const uint8_t v = static_cast<uint8_t>((ar >> (i << 3)) & 0xFFU);
                out[pos] = static_cast<uint8_t>(step8(v, false) ^ v);
                parity |= static_cast<uint8_t>((parity_keystream_bit() ^ oddparity8(v)) & 0x01U) << pos;
            }
            return parity;
        }

        uint32_t encrypt_stream(uint8_t *out, const uint8_t *in, uint8_t len)
        {
            uint32_t parity = 0;
            for (uint_fast8_t i = 0; i < len; ++i) {
                out[i] = static_cast<uint8_t>(in[i] ^ step8(0x00, false));
                parity |= static_cast<uint32_t>((parity_keystream_bit() ^ oddparity8(in[i])) & 1U) << i;
            }
            return parity;
        }

    private:
        static uint8_t evenparity32(uint32_t value)
        {
            return static_cast<uint8_t>(__builtin_parity(value));
        }

        static uint8_t crypto_big_endian_bit(uint32_t value, uint8_t bit)
        {
            return static_cast<uint8_t>((value >> (bit ^ 24U)) & 0x01U);
        }

        uint8_t filter() const
        {
            uint32_t f = 0U;
            f |= (0x000F22C0U >> (odd_ & 0x0FU)) & 0x10U;
            f |= (0x0006C9C0U >> ((odd_ >> 4U) & 0x0FU)) & 0x08U;
            f |= (0x0003C8B0U >> ((odd_ >> 8U) & 0x0FU)) & 0x04U;
            f |= (0x0001E458U >> ((odd_ >> 12U) & 0x0FU)) & 0x02U;
            f |= (0x0000D938U >> ((odd_ >> 16U) & 0x0FU)) & 0x01U;
            return static_cast<uint8_t>((0xEC57E80AU >> f) & 0x01U);
        }

        uint32_t odd_ = 0U;
        uint32_t even_ = 0U;
    };

} // namespace nfc_app
