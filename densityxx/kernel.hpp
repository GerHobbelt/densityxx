// see LICENSE.md for license.
#pragma once

#include "densityxx/format.hpp"

namespace density {
    // encode.
    typedef enum {
        kernel_encode_state_ready = 0,
        kernel_encode_state_info_new_block,
        kernel_encode_state_info_efficiency_check,
        kernel_encode_state_stall_on_input,
        kernel_encode_state_stall_on_output,
        kernel_encode_state_error
    } kernel_encode_state_t;

    class kernel_encode_t {
    public:
        kernel_encode_t(void) {}
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

    class kernel_decode_t {
    public:
        kernel_decode_t(void) {}
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
