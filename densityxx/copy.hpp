#pragma once

#include "densityxx/kernel.hpp"

namespace density {
    class copy_encode_t: public kernel_encode_t {
    public:
        DENSITY_INLINE const compression_mode_t mode(void) const
        {   return compression_mode_copy; }
        DENSITY_INLINE state_t init(void) { return state_ready; }
        DENSITY_INLINE state_t continue_(teleport_t *in, location_t *out)
        {   return state_ready; }
        DENSITY_INLINE state_t finish(teleport_t *in, location_t *out) { return state_ready; }
    };
    class copy_decode_t: public kernel_decode_t {
    public:
        DENSITY_INLINE const compression_mode_t mode(void) const
        {   return compression_mode_copy; }
        DENSITY_INLINE state_t init(main_header_parameters_t parameters,
                                    const uint_fast8_t end_data_overhead)
        {   return state_ready; }
        DENSITY_INLINE state_t continue_(teleport_t *in, location_t *out)
        {   return state_ready; }
        DENSITY_INLINE state_t finish(teleport_t *in, location_t *out) { return state_ready; }
    };
}
