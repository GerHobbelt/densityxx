// see LICENSE.md for license.
#pragma once

#include "densityxx/main.hpp"

namespace density {
    // buffer.
#define DENSITY_MINIMUM_OUTPUT_BUFFER_SIZE         (1 << 10)
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

    // stream.
#define DENSITY_STREAM_MEMORY_TELEPORT_BUFFER_SIZE     (1 << 16)
    typedef enum {
        stream_state_ready = 0,                             // Awaiting further instructions (new action or adding data to the input buffer)
        stream_state_stall_on_input,                        // There is not enough space left in the input buffer to continue
        stream_state_stall_on_output,                       // There is not enough space left in the output buffer to continue
        stream_state_error_output_buffer_too_small,         // Output buffer size is too small
        stream_state_error_invalid_internal_state,          // Error during processing
        stream_state_error_integrity_check_fail             // Integrity check has failed
    } stream_state_t;
    DENSITY_ENUM_RENDER6(stream_state, ready, stall_on_input, stall_on_output,
                         error_output_buffer_too_small,
                         error_invalid_internal_state,
                         error_integrity_check_fail);

    struct stream_header_information_t {
        uint8_t major_version, minor_version, revision;
        compression_mode_t compression_mode;
        block_type_t block_type;
    };

    typedef enum {
        stream_process_prepared = 0,
        stream_process_compression_inited,
        stream_process_compression_started,
        stream_process_compression_finished,
        stream_process_decompression_inited,
        stream_process_decompression_started,
        stream_process_decompression_finished,
    } stream_process_t;
    DENSITY_ENUM_RENDER7(stream_process, prepared,
                         compression_inited, compression_started, compression_finished,
                         decompression_inited, decompression_started, decompression_finished);

    class stream_t {
    private:
        stream_process_t process;
        encode_t encode;
        decode_t decode;
    public:
        teleport_t *in;
        location_t *out;
        uint_fast64_t *total_bytes_read, *total_bytes_written;

        stream_t(void);
        ~stream_t();

        stream_state_t prepare(const uint8_t *RESTRICT in, const uint_fast64_t sz,
                               uint8_t *RESTRICT out, const uint_fast64_t szout);
        stream_state_t update_input(const uint8_t *RESTRICT in, const uint_fast64_t szin);
        stream_state_t update_output(uint8_t *RESTRICT out, const uint_fast64_t szout);
        uint_fast64_t output_available_for_use(void) const;
        stream_state_t check_conformity(void) const;

        stream_state_t compress_init(const compression_mode_t mode,
                                     const block_type_t block_type);
        stream_state_t compress_continue(void);
        stream_state_t compress_finish(void);

        stream_state_t
        decompress_init(stream_header_information_t *RESTRICT header_information);
        stream_state_t decompress_continue(void);
        stream_state_t decompress_finish(void);
    };
}
