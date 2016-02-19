// see LICENSE.md for license.
#pragma once

#include "densityxx/kernel.hpp"

namespace density {
    class lion_encode_t: public kernel_encode_t {
    public:
        lion_encode_t(void);
        virtual ~lion_encode_t();

        virtual compression_mode_t mode(void) const
        {   return compression_mode_lion_algorithm; }

        virtual kernel_encode_state_t init(void);
        virtual kernel_encode_state_t
        continue_(teleport_t *RESTRICT in, location_t *RESTRICT out);
        virtual kernel_encode_state_t
        finish(teleport_t *RESTRICT in, location_t *RESTRICT out);
    };

    class lion_decode_t: public kernel_decode_t {
    public:
        virtual compression_mode_t mode(void) const
        {   return compression_mode_lion_algorithm; }

        kernel_decode_state_t
        init(const main_header_parameters_t parameters, const uint_fast8_t end_data_overhead);
        virtual kernel_decode_state_t
        continue_(teleport_t *RESTRICT in, location_t *RESTRICT out);
        virtual kernel_decode_state_t
        finish(teleport_t *RESTRICT in, location_t *RESTRICT out);
    };
}
