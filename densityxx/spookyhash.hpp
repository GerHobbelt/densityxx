// see LICENSE.md for license.
#pragma once
#include "densityxx/spookyhash.def.hpp"

namespace density {
#define SPOOKYHASH_ALLOW_UNALIGNED_READS   1
    DENSITY_INLINE uint64_t spookyhash_rotate(uint64_t x, uint64_t k)
    {   return (x << k) | (x >> (64 - k)); }

    const size_t spookyhash_block_size = spookyhash_variables * 8;
    const size_t spookyhash_buffer_size = 2 * spookyhash_block_size;
    const uint64_t spookyhash_constant = 0xdeadbeefdeadbeefLL;

    DENSITY_INLINE void
    spookyhash_short_end(uint64_t *h0, uint64_t *h1, uint64_t *h2, uint64_t *h3) {
        *h3 ^= *h2;
        *h2 = spookyhash_rotate(*h2, 15);
        *h3 += *h2;
        *h0 ^= *h3;
        *h3 = spookyhash_rotate(*h3, 52);
        *h0 += *h3;
        *h1 ^= *h0;
        *h0 = spookyhash_rotate(*h0, 26);
        *h1 += *h0;
        *h2 ^= *h1;
        *h1 = spookyhash_rotate(*h1, 51);
        *h2 += *h1;
        *h3 ^= *h2;
        *h2 = spookyhash_rotate(*h2, 28);
        *h3 += *h2;
        *h0 ^= *h3;
        *h3 = spookyhash_rotate(*h3, 9);
        *h0 += *h3;
        *h1 ^= *h0;
        *h0 = spookyhash_rotate(*h0, 47);
        *h1 += *h0;
        *h2 ^= *h1;
        *h1 = spookyhash_rotate(*h1, 54);
        *h2 += *h1;
        *h3 ^= *h2;
        *h2 = spookyhash_rotate(*h2, 32);
        *h3 += *h2;
        *h0 ^= *h3;
        *h3 = spookyhash_rotate(*h3, 25);
        *h0 += *h3;
        *h1 ^= *h0;
        *h0 = spookyhash_rotate(*h0, 63);
        *h1 += *h0;
    }

    DENSITY_INLINE void
    spookyhash_short_mix(uint64_t *h0, uint64_t *h1, uint64_t *h2, uint64_t *h3) {
        *h2 = spookyhash_rotate(*h2, 50);
        *h2 += *h3;
        *h0 ^= *h2;
        *h3 = spookyhash_rotate(*h3, 52);
        *h3 += *h0;
        *h1 ^= *h3;
        *h0 = spookyhash_rotate(*h0, 30);
        *h0 += *h1;
        *h2 ^= *h0;
        *h1 = spookyhash_rotate(*h1, 41);
        *h1 += *h2;
        *h3 ^= *h1;
        *h2 = spookyhash_rotate(*h2, 54);
        *h2 += *h3;
        *h0 ^= *h2;
        *h3 = spookyhash_rotate(*h3, 48);
        *h3 += *h0;
        *h1 ^= *h3;
        *h0 = spookyhash_rotate(*h0, 38);
        *h0 += *h1;
        *h2 ^= *h0;
        *h1 = spookyhash_rotate(*h1, 37);
        *h1 += *h2;
        *h3 ^= *h1;
        *h2 = spookyhash_rotate(*h2, 62);
        *h2 += *h3;
        *h0 ^= *h2;
        *h3 = spookyhash_rotate(*h3, 34);
        *h3 += *h0;
        *h1 ^= *h3;
        *h0 = spookyhash_rotate(*h0, 5);
        *h0 += *h1;
        *h2 ^= *h0;
        *h1 = spookyhash_rotate(*h1, 36);
        *h1 += *h2;
        *h3 ^= *h1;
    }

    DENSITY_INLINE void
    spookyhash_short(const void *message, size_t length, uint64_t *hash1, uint64_t *hash2) {
#if !SPOOKYHASH_ALLOW_UNALIGNED_READS
        uint64_t buffer[2 * spookyhash_variables];
#endif

        union {
            const uint8_t *p8;
            uint32_t *p32;
            uint64_t *p64;
            size_t i;
        } u;
        u.p8 = (const uint8_t *) message;

#if !SPOOKYHASH_ALLOW_UNALIGNED_READS
        if (u.i & 0x7) {
            memcpy(buffer, message, length);
            u.p64 = buffer;
        }
#endif

        size_t remainder = length % 32;
        uint64_t a = *hash1;
        uint64_t b = *hash2;
        uint64_t c = spookyhash_constant;
        uint64_t d = spookyhash_constant;

        if (length > 15) {
            const uint64_t *end = u.p64 + (length / 32) * 4;
            for (; u.p64 < end; u.p64 += 4) {
                c += LITTLE_ENDIAN_64(u.p64[0]);
                d += LITTLE_ENDIAN_64(u.p64[1]);
                spookyhash_short_mix(&a, &b, &c, &d);
                a += LITTLE_ENDIAN_64(u.p64[2]);
                b += LITTLE_ENDIAN_64(u.p64[3]);
            }

            if (remainder >= 16) {
                c += LITTLE_ENDIAN_64(u.p64[0]);
                d += LITTLE_ENDIAN_64(u.p64[1]);
                spookyhash_short_mix(&a, &b, &c, &d);
                u.p64 += 2;
                remainder -= 16;
            }
        }

        d += ((uint64_t) length) << 56;
        switch (remainder) {
        default:
            break;
        case 15:
            d += ((uint64_t) u.p8[14]) << 48;
        case 14:
            d += ((uint64_t) u.p8[13]) << 40;
        case 13:
            d += ((uint64_t) u.p8[12]) << 32;
        case 12:
            d += u.p32[2];
            c += u.p64[0];
            break;
        case 11:
            d += ((uint64_t) u.p8[10]) << 16;
        case 10:
            d += ((uint64_t) u.p8[9]) << 8;
        case 9:
            d += (uint64_t) u.p8[8];
        case 8:
            c += u.p64[0];
            break;
        case 7:
            c += ((uint64_t) u.p8[6]) << 48;
        case 6:
            c += ((uint64_t) u.p8[5]) << 40;
        case 5:
            c += ((uint64_t) u.p8[4]) << 32;
        case 4:
            c += u.p32[0];
            break;
        case 3:
            c += ((uint64_t) u.p8[2]) << 16;
        case 2:
            c += ((uint64_t) u.p8[1]) << 8;
        case 1:
            c += (uint64_t) u.p8[0];
            break;
        case 0:
            c += spookyhash_constant;
            d += spookyhash_constant;
        }
        spookyhash_short_end(&a, &b, &c, &d);
        *hash1 = a;
        *hash2 = b;
    }

    DENSITY_INLINE void
    spookyhash_mix(const uint64_t *data,
                   uint64_t *s0, uint64_t *s1, uint64_t *s2, uint64_t *s3,
                   uint64_t *s4, uint64_t *s5, uint64_t *s6, uint64_t *s7,
                   uint64_t *s8, uint64_t *s9, uint64_t *s10, uint64_t *s11) {
        *s0 += LITTLE_ENDIAN_64(data[0]);
        *s2 ^= *s10;
        *s11 ^= *s0;
        *s0 = spookyhash_rotate(*s0, 11);
        *s11 += *s1;
        *s1 += LITTLE_ENDIAN_64(data[1]);
        *s3 ^= *s11;
        *s0 ^= *s1;
        *s1 = spookyhash_rotate(*s1, 32);
        *s0 += *s2;
        *s2 += LITTLE_ENDIAN_64(data[2]);
        *s4 ^= *s0;
        *s1 ^= *s2;
        *s2 = spookyhash_rotate(*s2, 43);
        *s1 += *s3;
        *s3 += LITTLE_ENDIAN_64(data[3]);
        *s5 ^= *s1;
        *s2 ^= *s3;
        *s3 = spookyhash_rotate(*s3, 31);
        *s2 += *s4;
        *s4 += LITTLE_ENDIAN_64(data[4]);
        *s6 ^= *s2;
        *s3 ^= *s4;
        *s4 = spookyhash_rotate(*s4, 17);
        *s3 += *s5;
        *s5 += LITTLE_ENDIAN_64(data[5]);
        *s7 ^= *s3;
        *s4 ^= *s5;
        *s5 = spookyhash_rotate(*s5, 28);
        *s4 += *s6;
        *s6 += LITTLE_ENDIAN_64(data[6]);
        *s8 ^= *s4;
        *s5 ^= *s6;
        *s6 = spookyhash_rotate(*s6, 39);
        *s5 += *s7;
        *s7 += LITTLE_ENDIAN_64(data[7]);
        *s9 ^= *s5;
        *s6 ^= *s7;
        *s7 = spookyhash_rotate(*s7, 57);
        *s6 += *s8;
        *s8 += LITTLE_ENDIAN_64(data[8]);
        *s10 ^= *s6;
        *s7 ^= *s8;
        *s8 = spookyhash_rotate(*s8, 55);
        *s7 += *s9;
        *s9 += LITTLE_ENDIAN_64(data[9]);
        *s11 ^= *s7;
        *s8 ^= *s9;
        *s9 = spookyhash_rotate(*s9, 54);
        *s8 += *s10;
        *s10 += LITTLE_ENDIAN_64(data[10]);
        *s0 ^= *s8;
        *s9 ^= *s10;
        *s10 = spookyhash_rotate(*s10, 22);
        *s9 += *s11;
        *s11 += LITTLE_ENDIAN_64(data[11]);
        *s1 ^= *s9;
        *s10 ^= *s11;
        *s11 = spookyhash_rotate(*s11, 46);
        *s10 += *s0;
    }

    DENSITY_INLINE void
    spookyhash_end_partial(uint64_t *h0, uint64_t *h1, uint64_t *h2, uint64_t *h3,
                           uint64_t *h4, uint64_t *h5, uint64_t *h6, uint64_t *h7,
                           uint64_t *h8, uint64_t *h9, uint64_t *h10, uint64_t *h11) {
        *h11 += *h1;
        *h2 ^= *h11;
        *h1 = spookyhash_rotate(*h1, 44);
        *h0 += *h2;
        *h3 ^= *h0;
        *h2 = spookyhash_rotate(*h2, 15);
        *h1 += *h3;
        *h4 ^= *h1;
        *h3 = spookyhash_rotate(*h3, 34);
        *h2 += *h4;
        *h5 ^= *h2;
        *h4 = spookyhash_rotate(*h4, 21);
        *h3 += *h5;
        *h6 ^= *h3;
        *h5 = spookyhash_rotate(*h5, 38);
        *h4 += *h6;
        *h7 ^= *h4;
        *h6 = spookyhash_rotate(*h6, 33);
        *h5 += *h7;
        *h8 ^= *h5;
        *h7 = spookyhash_rotate(*h7, 10);
        *h6 += *h8;
        *h9 ^= *h6;
        *h8 = spookyhash_rotate(*h8, 13);
        *h7 += *h9;
        *h10 ^= *h7;
        *h9 = spookyhash_rotate(*h9, 38);
        *h8 += *h10;
        *h11 ^= *h8;
        *h10 = spookyhash_rotate(*h10, 53);
        *h9 += *h11;
        *h0 ^= *h9;
        *h11 = spookyhash_rotate(*h11, 42);
        *h10 += *h0;
        *h1 ^= *h10;
        *h0 = spookyhash_rotate(*h0, 54);
    }

    DENSITY_INLINE void
    spookyhash_end(const uint64_t *data,
                   uint64_t *h0, uint64_t *h1, uint64_t *h2, uint64_t *h3,
                   uint64_t *h4, uint64_t *h5, uint64_t *h6, uint64_t *h7,
                   uint64_t *h8, uint64_t *h9, uint64_t *h10, uint64_t *h11) {
        *h0 += LITTLE_ENDIAN_64(data[0]);
        *h1 += LITTLE_ENDIAN_64(data[1]);
        *h2 += LITTLE_ENDIAN_64(data[2]);
        *h3 += LITTLE_ENDIAN_64(data[3]);
        *h4 += LITTLE_ENDIAN_64(data[4]);
        *h5 += LITTLE_ENDIAN_64(data[5]);
        *h6 += LITTLE_ENDIAN_64(data[6]);
        *h7 += LITTLE_ENDIAN_64(data[7]);
        *h8 += LITTLE_ENDIAN_64(data[8]);
        *h9 += LITTLE_ENDIAN_64(data[9]);
        *h10 += LITTLE_ENDIAN_64(data[10]);
        *h11 += LITTLE_ENDIAN_64(data[11]);
        spookyhash_end_partial(h0, h1, h2, h3, h4, h5, h6, h7, h8, h9, h10, h11);
        spookyhash_end_partial(h0, h1, h2, h3, h4, h5, h6, h7, h8, h9, h10, h11);
        spookyhash_end_partial(h0, h1, h2, h3, h4, h5, h6, h7, h8, h9, h10, h11);
    }

    DENSITY_INLINE void
    spookyhash_128(const void *message, size_t length, uint64_t *hash1, uint64_t *hash2)
    {
        if (length < spookyhash_buffer_size) {
            spookyhash_short(message, length, hash1, hash2);
            return;
        }

        uint64_t h0, h1, h2, h3, h4, h5, h6, h7, h8, h9, h10, h11;
        uint64_t buf[spookyhash_variables];
        uint64_t *end;
        union {
            const uint8_t *p8;
            uint64_t *p64;
            size_t i;
        } u;
        size_t remainder;

        h0 = h3 = h6 = h9 = *hash1;
        h1 = h4 = h7 = h10 = *hash2;
        h2 = h5 = h8 = h11 = spookyhash_constant;

        u.p8 = (const uint8_t *) message;
        end = u.p64 + (length / spookyhash_block_size) * spookyhash_variables;

        if (SPOOKYHASH_ALLOW_UNALIGNED_READS || ((u.i & 0x7) == 0)) {
            while (u.p64 < end) {
                spookyhash_mix(u.p64, &h0, &h1, &h2, &h3, &h4, &h5,
                               &h6, &h7, &h8, &h9, &h10, &h11);
                u.p64 += spookyhash_variables;
            }
        } else {
            while (u.p64 < end) {
                memcpy(buf, u.p64, spookyhash_block_size);
                spookyhash_mix(buf, &h0, &h1, &h2, &h3, &h4, &h5,
                               &h6, &h7, &h8, &h9, &h10, &h11);
                u.p64 += spookyhash_variables;
            }
        }

        remainder = (length - ((const uint8_t *) end - (const uint8_t *) message));
        memcpy(buf, end, remainder);
        memset(((uint8_t *) buf) + remainder, 0, spookyhash_block_size - remainder);
        ((uint8_t *) buf)[spookyhash_block_size - 1] = (uint8_t) remainder;

        spookyhash_end(buf, &h0, &h1, &h2, &h3, &h4, &h5, &h6, &h7, &h8, &h9, &h10, &h11);
        *hash1 = h0;
        *hash2 = h1;
    }

    DENSITY_INLINE uint64_t
    spookyhash_64(const void *message, size_t length, uint64_t seed)
    {
        uint64_t hash1 = seed;
        spookyhash_128(message, length, &hash1, &seed);
        return hash1;
    }

    DENSITY_INLINE uint32_t
    spookyhash_32(const void *message, size_t length, uint32_t seed)
    {
        uint64_t hash1 = seed, hash2 = seed;
        spookyhash_128(message, length, &hash1, &hash2);
        return (uint32_t) hash1;
    }

    DENSITY_INLINE void
    spookyhash_context_t::init(uint64_t seed1, uint64_t seed2)
    {
        m_length = 0; m_remainder = 0;
        m_state[0] = seed1; m_state[1] = seed2;
    }

    DENSITY_INLINE void
    spookyhash_context_t::update(const void *message, size_t length)
    {
        uint64_t h0, h1, h2, h3, h4, h5, h6, h7, h8, h9, h10, h11;
        size_t new_length = length + m_remainder;
        uint8_t remainder;
        union {
            const uint8_t *p8;
            uint64_t *p64;
            size_t i;
        } u;
        const uint64_t *end;

        if (new_length < spookyhash_buffer_size) {
            memcpy(&((uint8_t *) m_data)[m_remainder], message, length);
            m_length = length + m_length;
            m_remainder = (uint8_t) new_length;
            return;
        }

        if (m_length < spookyhash_buffer_size) {
            h0 = h3 = h6 = h9 = m_state[0];
            h1 = h4 = h7 = h10 = m_state[1];
            h2 = h5 = h8 = h11 = spookyhash_constant;
        } else {
            h0 = m_state[0];
            h1 = m_state[1];
            h2 = m_state[2];
            h3 = m_state[3];
            h4 = m_state[4];
            h5 = m_state[5];
            h6 = m_state[6];
            h7 = m_state[7];
            h8 = m_state[8];
            h9 = m_state[9];
            h10 = m_state[10];
            h11 = m_state[11];
        }
        m_length = length + m_length;

        if (m_remainder) {
            uint8_t prefix = (uint8_t) (spookyhash_buffer_size - m_remainder);
            memcpy(&(((uint8_t *) m_data)[m_remainder]), message, prefix);
            u.p64 = m_data;
            spookyhash_mix(u.p64, &h0, &h1, &h2, &h3, &h4, &h5,
                           &h6, &h7, &h8, &h9, &h10, &h11);
            spookyhash_mix(&u.p64[spookyhash_variables],
                           &h0, &h1, &h2, &h3, &h4, &h5,
                           &h6, &h7, &h8, &h9, &h10, &h11);
            u.p8 = ((const uint8_t *) message) + prefix;
            length -= prefix;
        } else {
            u.p8 = (const uint8_t *) message;
        }

        end = u.p64 + (length / spookyhash_block_size) * spookyhash_variables;
        remainder = (uint8_t) (length - ((const uint8_t *) end - u.p8));
        if (SPOOKYHASH_ALLOW_UNALIGNED_READS || (u.i & 0x7) == 0) {
            while (u.p64 < end) {
                spookyhash_mix(u.p64, &h0, &h1, &h2, &h3, &h4, &h5,
                               &h6, &h7, &h8, &h9, &h10, &h11);
                u.p64 += spookyhash_variables;
            }
        } else {
            while (u.p64 < end) {
                memcpy(m_data, u.p8, spookyhash_block_size);
                spookyhash_mix(m_data, &h0, &h1, &h2, &h3, &h4, &h5,
                               &h6, &h7, &h8, &h9, &h10, &h11);
                u.p64 += spookyhash_variables;
            }
        }

        m_remainder = remainder;
        memcpy(m_data, end, remainder);

        m_state[0] = h0;
        m_state[1] = h1;
        m_state[2] = h2;
        m_state[3] = h3;
        m_state[4] = h4;
        m_state[5] = h5;
        m_state[6] = h6;
        m_state[7] = h7;
        m_state[8] = h8;
        m_state[9] = h9;
        m_state[10] = h10;
        m_state[11] = h11;
    }

    DENSITY_INLINE void
    spookyhash_context_t::final(uint64_t *hash1, uint64_t *hash2)
    {
        if (m_length < spookyhash_buffer_size) {
            *hash1 = m_state[0];
            *hash2 = m_state[1];
            spookyhash_short(m_data, m_length, hash1, hash2);
            return;
        }

        const uint64_t *data = (const uint64_t *) m_data;
        uint8_t remainder = m_remainder;

        uint64_t h0 = m_state[0];
        uint64_t h1 = m_state[1];
        uint64_t h2 = m_state[2];
        uint64_t h3 = m_state[3];
        uint64_t h4 = m_state[4];
        uint64_t h5 = m_state[5];
        uint64_t h6 = m_state[6];
        uint64_t h7 = m_state[7];
        uint64_t h8 = m_state[8];
        uint64_t h9 = m_state[9];
        uint64_t h10 = m_state[10];
        uint64_t h11 = m_state[11];

        if (remainder >= spookyhash_block_size) {
            spookyhash_mix(data, &h0, &h1, &h2, &h3, &h4, &h5,
                           &h6, &h7, &h8, &h9, &h10, &h11);
            data += spookyhash_variables;
            remainder -= spookyhash_block_size;
        }

        memset(&((uint8_t *) data)[remainder], 0, (spookyhash_block_size - remainder));

        ((uint8_t *) data)[spookyhash_block_size - 1] = remainder;

        spookyhash_end(data, &h0, &h1, &h2, &h3, &h4, &h5,
                       &h6, &h7, &h8, &h9, &h10, &h11);

        *hash1 = h0;
        *hash2 = h1;
    }
}
