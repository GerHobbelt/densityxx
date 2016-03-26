// see LICENSE.md for license.
#pragma once
#include "densityxx/globals.hpp"

namespace density {
    // buffer.
    typedef enum {
        state_ok = 0,                                // Everything went alright
        state_error_output_buffer_too_small,         // Output buffer size is too small
        state_error_during_processing,               // Error during processing
        state_error_integrity_check_fail             // Integrity check has failed
    } state_t;
    DENSITY_ENUM_RENDER4(state, ok, error_output_buffer_too_small,
                         error_during_processing, error_integrity_check_fail);

    struct processing_result_t {
        state_t state;
        uint_fast64_t bytes_read, bytes_written;
    };

    processing_result_t
    compress(const uint8_t *in, const uint_fast64_t szin,
             uint8_t *out, const uint_fast64_t szout,
             const compression_mode_t compression_mode,
             const block_type_t block_type);
    processing_result_t
    decompress(const uint8_t *in, const uint_fast64_t szin,
               uint8_t *out, const uint_fast64_t szout);
}
