// see LICENSE.md for license.
#pragma once

#include "densityxx/globals.hpp"

namespace density {
    // buffer.
    typedef enum {
        buffer_state_ok = 0,                                // Everything went alright
        buffer_state_error_output_buffer_too_small,         // Output buffer size is too small
        buffer_state_error_during_processing,               // Error during processing
        buffer_state_error_integrity_check_fail             // Integrity check has failed
    } buffer_state_t;

    struct buffer_processing_result_t {
        buffer_state_t state;
        uint_fast64_t bytes_read, bytes_written;
    };

    buffer_processing_result_t
    buffer_compress(const uint8_t *in, const uint_fast64_t szin,
                    uint8_t* out, const uint_fast64_t szout,
                    const compression_mode_t compression_mode,
                    const block_type_t block_type);
    buffer_processing_result_t
    buffer_decompress(const uint8_t *in, const uint_fast64_t szin,
                      uint8_t *out, const uint_fast64_t szout);

    // stream.
    typedef enum {
        stream_state_ready = 0,                             // Awaiting further instructions (new action or adding data to the input buffer)
        stream_state_stall_on_input,                        // There is not enough space left in the input buffer to continue
        stream_state_stall_on_output,                       // There is not enough space left in the output buffer to continue
        stream_state_error_output_buffer_too_small,         // Output buffer size is too small
        stream_state_error_invalid_internal_state,          // Error during processing
        stream_state_error_integrity_check_fail             // Integrity check has failed
    } stream_state_t;

    struct stream_header_information_t {
        uint8_t majorVersion, minorVersion, revision;
        compression_mode_t compression_mode;
        block_type_t block_type;
    };

    class stream_t {
    public:
        stream_t(void);
        ~stream_t();

        stream_state_t prepare(const uint8_t *in, const uint_fast64_t sz,
                               uint8_t *out, const uint_fast64_t szout);
        stream_state_t update_input(const uint8_t *in, const uint_fast64_t szin);
        stream_state_t update_output(uint8_t *out, const uint_fast64_t szout);
        uint_fast64_t output_available_for_use(void) const;

        stream_state_t compress_init(const compression_mode_t mode,
                                     const block_type_t block_type);
        stream_state_t compress_continue(void);
        stream_state_t compress_finish(void);

        stream_state_t decompress_init(stream_header_information_t *header_information);
        stream_state_t decompress_continue(void);
        stream_state_t decompress_finish(void);
    };
}
