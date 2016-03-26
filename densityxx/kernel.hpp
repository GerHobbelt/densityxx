// see LICENSE.md for license.
#pragma once

#include "densityxx/format.hpp"

namespace density {
    const size_t hash_bits = 16;
    const uint32_t hash_multiplier = 0x9D6EF916U;
    DENSITY_INLINE uint16_t hash_algorithm(const uint32_t value32)
    {   return (uint16_t)((value32 * hash_multiplier) >> (32 - hash_bits)); }

    // encode.
    class kernel_encode_t {
    public:
        typedef enum {
            state_ready = 0,
            state_info_new_block,
            state_info_efficiency_check,
            state_stall_on_input,
            state_stall_on_output,
            state_error
        } state_t;
        DENSITY_ENUM_RENDER6(state, ready, info_new_block, info_efficiency_check,
                             stall_on_input, stall_on_output, error);
    };

    // decode.
    class kernel_decode_t {
    public:
        typedef enum {
            state_ready = 0,
            state_info_new_block,
            state_info_efficiency_check,
            state_stall_on_input,
            state_stall_on_output,
            state_error
        } state_t;
        DENSITY_ENUM_RENDER6(state, ready, info_new_block, info_efficiency_check,
                             stall_on_input, stall_on_output, error);
    };
}
