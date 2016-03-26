#pragma once

#include "densityxx/kernel.hpp"

namespace density {
    class copy_encode_t: public kernel_encode_t {
    public:
        inline const compression_mode_t mode(void) const { return compression_mode_copy; }
        inline state_t init(void) { return state_ready; }
        inline state_t continue_(teleport_t *in, location_t *out) { return state_ready; }
        inline state_t finish(teleport_t *in, location_t *out) { return state_ready; }
    };
    class copy_decode_t: public kernel_decode_t {
    public:
        inline const compression_mode_t mode(void) const { return compression_mode_copy; }
        inline state_t init(main_header_parameters_t parameters,
                            const uint_fast8_t end_data_overhead)
        {   return state_ready; }
        inline state_t continue_t(teleport_t *in, location_t *out) { return state_ready; }
        inline state_t finish(teleport_t *in, location_t *out) { return state_ready; }
    };
}
