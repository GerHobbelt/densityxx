// see LICENSE.md for license.
#pragma once
#include "densityxx/stream.hpp"

namespace density {
    // buffer.
    typedef enum {
        buffer_state_ok = 0,                                // Everything went alright
        buffer_state_error_output_buffer_too_small,         // Output buffer size is too small
        buffer_state_error_during_processing,               // Error during processing
        buffer_state_error_integrity_check_fail             // Integrity check has failed
    } buffer_state_t;
    DENSITY_ENUM_RENDER4(buffer_state, ok, error_output_buffer_too_small,
                         error_during_processing, error_integrity_check_fail);

    struct buffer_processing_result_t {
        buffer_state_t state;
        uint_fast64_t bytes_read, bytes_written;
    };

    buffer_processing_result_t
    buffer_compress(const uint8_t *in, const uint_fast64_t szin,
                    uint8_t *out, const uint_fast64_t szout,
                    const compression_mode_t compression_mode,
                    const block_type_t block_type);
    buffer_processing_result_t
    buffer_decompress(const uint8_t *in, const uint_fast64_t szin,
                      uint8_t *out, const uint_fast64_t szout);
}
