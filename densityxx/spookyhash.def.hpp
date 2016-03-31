// see LICENSE.md for license.
#pragma once

#include "globals.hpp"

namespace density {
    const size_t spookyhash_variables = 12;
    class spookyhash_context_t {
    private:
        uint64_t m_data[2 * spookyhash_variables];
        uint64_t m_state[spookyhash_variables];
        size_t m_length;
        uint8_t m_remainder;
    public:
        void init(uint64_t seed1, uint64_t seed2);
        void update(const void *message, size_t length);
        void final(uint64_t *hash1, uint64_t *hash2);
    };
}
