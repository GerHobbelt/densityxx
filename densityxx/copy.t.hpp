#pragma once

#include "densityxx/kernel.t.hpp"

namespace density {
    class copy_encode_t: public kernel_encode_t {
    public:
        inline const compression_mode_t mode(void) const { return compression_mode_copy; }
        inline const state_t init(void) const { return state_ready; }
    };
    class copy_decode_t: public kernel_decode_t {
    public:
        inline const compression_mode_t mode(void) const { return compression_mode_copy; }
        inline const state_t init(const main_header_parameters_t parameters,
                                  const uint_fast8_t end_data_overhead) const
        {   return state_ready; }
    };
}
