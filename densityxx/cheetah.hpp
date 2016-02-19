// see LICENSE.md for license.
#pragma once

#include "densityxx/kernel.hpp"

namespace density {
    class cheetah_encode_t: public kernel_encode_t {
    public:
        cheetah_encode_t(void);
        virtual ~cheetah_encode_t();

        virtual compression_mode_t mode(void) const
        {   return compression_mode_cheetah_algorithm; }
        virtual kernel_encode_state_t
        process(teleport_t *RESTRICT in, location_t *RESTRICT out);
        virtual kernel_encode_state_t
        finish(teleport_t *RESTRICT in, location_t *RESTRICT out);
    };

    class cheetah_decode_t: public kernel_decode_t {
    public:
        cheetah_decode_t(const main_header_parameters_t parameters,
                         const uint_fast8_t end_data_overhead);
        virtual ~cheetah_decode_t();

        virtual compression_mode_t mode(void) const
        {   return compression_mode_cheetah_algorithm; }
        virtual kernel_decode_state_t
        process(teleport_t *RESTRICT in, location_t *RESTRICT out);
        virtual kernel_decode_state_t
        finish(teleport_t *RESTRICT in, location_t *RESTRICT out);
    };
}
