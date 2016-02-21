// see LICENSE.md for license.
#pragma once

#include "globals.hpp"

namespace density {
#define SPOOKYHASH_VARIABLES (12)
    class spookyhash_context_t {
    private:
        uint64_t m_data[2 * SPOOKYHASH_VARIABLES];
        uint64_t m_state[SPOOKYHASH_VARIABLES];
        size_t m_length;
        uint8_t m_remainder;
    public:
        void init(uint64_t seed1, uint64_t seed2);
        void update(const void *RESTRICT message, size_t length);
        void final(uint64_t *hash1, uint64_t *hash2);
    };
}
