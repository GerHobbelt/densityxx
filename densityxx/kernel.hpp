// see LICENSE.md for license.
#pragma once

#include "densityxx/format.hpp"

namespace density {
    const size_t hash_bits = sizeof(uint16_t);
    const uint32_t hash_multiplier = 0x9D6EF916U;
    inline uint16_t hash_algorithm(const uint32_t value32)
    {   return (uint16_t)((value32 * hash_multiplier) >> (32 - hash_bits)); }

    // encode.
    typedef enum {
        kernel_encode_state_ready = 0,
        kernel_encode_state_info_new_block,
        kernel_encode_state_info_efficiency_check,
        kernel_encode_state_stall_on_input,
        kernel_encode_state_stall_on_output,
        kernel_encode_state_error
    } kernel_encode_state_t;
    DENSITY_ENUM_RENDER6(kernel_encode_state, ready, info_new_block, info_efficiency_check,
                         stall_on_input, stall_on_output, error);

    class kernel_encode_t {
    public:
        inline kernel_encode_t(void) {}
        virtual ~kernel_encode_t() {}

        virtual compression_mode_t mode(void) const = 0;

        virtual kernel_encode_state_t init(void) = 0;
        virtual kernel_encode_state_t
        continue_(teleport_t *RESTRICT in, location_t *RESTRICT out) = 0;
        virtual kernel_encode_state_t
        finish(teleport_t *RESTRICT in, location_t *RESTRICT out) = 0;
    };

    // decode.
    typedef enum {
        kernel_decode_state_ready = 0,
        kernel_decode_state_info_new_block,
        kernel_decode_state_info_efficiency_check,
        kernel_decode_state_stall_on_input,
        kernel_decode_state_stall_on_output,
        kernel_decode_state_error
    } kernel_decode_state_t;
    DENSITY_ENUM_RENDER6(kernel_decode_state, ready, info_new_block, info_efficiency_check,
                         stall_on_input, stall_on_output, error);

    class kernel_decode_t {
    public:
        inline kernel_decode_t(void) {}
        virtual ~kernel_decode_t() {}

        virtual compression_mode_t mode(void) const = 0;

        virtual kernel_decode_state_t
        init(const main_header_parameters_t parameters,
             const uint_fast8_t end_data_overhead) = 0;
        virtual kernel_decode_state_t
        continue_(teleport_t *RESTRICT in, location_t *RESTRICT out) = 0;
        virtual kernel_decode_state_t
        finish(teleport_t *RESTRICT in, location_t *RESTRICT out) = 0;
    };
}
