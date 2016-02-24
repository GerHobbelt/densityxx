// see LICENSE.md for license.
#include "densityxx/api.def.hpp"
#include "densityxx/main.hpp"

namespace density {
    // buffer.
    inline buffer_processing_result_t
    buffer_return_processing_result(stream_t *stream, buffer_state_t state)
    {
        buffer_processing_result_t result;
        result.state = state;
        DENSITY_MEMCPY(&result.bytes_read, stream->total_bytes_read, sizeof(uint64_t));
        DENSITY_MEMCPY(&result.bytes_written, stream->total_bytes_written, sizeof(uint64_t));
        delete stream;
        return result;
    }

#define BUFFER_RETURN(suffix) \
    return buffer_return_processing_result(stream, buffer_state_##suffix)
    buffer_processing_result_t
    buffer_compress(const uint8_t *in, const uint_fast64_t szin,
                    uint8_t *out, const uint_fast64_t szout,
                    const compression_mode_t compression_mode,
                    const block_type_t block_type)
    {
        stream_state_t stream_state;
        stream_t *stream = new stream_t();
        if ((stream_state = stream->prepare(in, szin, out, szout)))
            BUFFER_RETURN(error_during_processing);
        switch (stream_state = stream->compress_init(compression_mode, block_type)) {
        case stream_state_ready: break;
        case stream_state_error_output_buffer_too_small:
            BUFFER_RETURN(error_output_buffer_too_small);
        default:
            BUFFER_RETURN(error_during_processing);
        }
        switch (stream_state = stream->compress_continue()) {
        case stream_state_ready:
        case stream_state_stall_on_input: break;
        case stream_state_error_output_buffer_too_small:
            BUFFER_RETURN(error_output_buffer_too_small);
        default:
            BUFFER_RETURN(error_during_processing);
        }
        switch (stream_state = stream->compress_finish()) {
        case stream_state_ready: BUFFER_RETURN(ok);
        case stream_state_error_output_buffer_too_small:
            BUFFER_RETURN(error_output_buffer_too_small);
        default:
            BUFFER_RETURN(error_during_processing);
        }
    }

    buffer_processing_result_t
    buffer_decompress(const uint8_t *in, const uint_fast64_t szin,
                      uint8_t *out, const uint_fast64_t szout)
    {
        stream_state_t stream_state;
        stream_t *stream = new stream_t();
        if ((stream_state = stream->prepare(in, szin, out, szout)))
            BUFFER_RETURN(error_during_processing);
        switch (stream_state = stream->decompress_init(NULL)) {
        case stream_state_ready: break;
        case stream_state_error_output_buffer_too_small:
            BUFFER_RETURN(error_output_buffer_too_small);
        default:
            BUFFER_RETURN(error_during_processing);
        }
        switch (stream_state = stream->decompress_continue()) {
        case stream_state_ready:
        case stream_state_stall_on_input:
        case stream_state_stall_on_output: break;
        case stream_state_error_output_buffer_too_small:
            BUFFER_RETURN(error_output_buffer_too_small);
        case stream_state_error_integrity_check_fail:
            BUFFER_RETURN(error_integrity_check_fail);
        default:
            BUFFER_RETURN(error_during_processing);
        }
        switch (stream_state = stream->decompress_finish()) {
        case stream_state_ready: BUFFER_RETURN(ok);
        case stream_state_error_output_buffer_too_small:
            BUFFER_RETURN(error_output_buffer_too_small);
        case stream_state_error_integrity_check_fail:
            BUFFER_RETURN(error_integrity_check_fail);
        default:
            BUFFER_RETURN(error_during_processing);
        }
    }

    // stream.
    stream_state_t
    stream_t::prepare(const uint8_t *RESTRICT in, const uint_fast64_t available_in,
                      uint8_t *RESTRICT out, const uint_fast64_t available_out)
    {
        this->in.reset_staging_buffer();
        update_input(in, available_in);
        update_output(out, available_out);
        process = stream_process_prepared;
        return stream_state_ready;
    }
    stream_state_t
    stream_t::compress_init(const compression_mode_t compression_mode,
                            const block_type_t block_type)
    {
        if (process != stream_process_prepared)
            return stream_state_error_invalid_internal_state;
        stream_state_t stream_state = check_conformity();
        if (stream_state) return stream_state;
        encode_state_t encode_state = encode.init(&out, compression_mode, block_type);
        switch (encode_state) {
        case encode_state_ready: break;
        case encode_state_stall_on_input: return stream_state_stall_on_input;
        case encode_state_stall_on_output: return stream_state_stall_on_output;
        default: return stream_state_error_invalid_internal_state;
        }
        total_bytes_read = &encode.total_read;
        total_bytes_written = &encode.total_written;
        process = stream_process_compression_inited;
        return stream_state_ready;
    }
    stream_state_t
    stream_t::compress_continue(void)
    {
        if (process != stream_process_compression_started) {
            if (process != stream_process_compression_inited)
                return stream_state_error_invalid_internal_state;
            process = stream_process_compression_started;
        }
        stream_state_t stream_state = check_conformity();
        if (stream_state) return stream_state;
        encode_state_t encode_state = encode.continue_(&in, &out);
        switch (encode_state) {
        case encode_state_ready: return stream_state_ready;
        case encode_state_stall_on_input: return stream_state_stall_on_input;
        case encode_state_stall_on_output: return stream_state_stall_on_output;
        default: return stream_state_error_invalid_internal_state;
        }
    }
    stream_state_t
    stream_t::compress_finish(void)
    {
        if (process != stream_process_compression_started) {
            if (process != stream_process_compression_inited)
                return stream_state_error_invalid_internal_state;
            process = stream_process_compression_started;
        }
        stream_state_t stream_state = check_conformity();
        if (stream_state) return stream_state;
        encode_state_t encode_state = encode.finish(&in, &out);
        switch (encode_state) {
        case encode_state_ready: break;
        case encode_state_stall_on_output: return stream_state_stall_on_output;
        default: return stream_state_error_invalid_internal_state;
        }
        process = stream_process_compression_finished;
        return stream_state_ready;
    }

    stream_state_t
    stream_t::decompress_init(stream_header_information_t *RESTRICT header_information)
    {
        if (process != stream_process_prepared)
            return stream_state_error_invalid_internal_state;
        stream_state_t stream_state = check_conformity();
        if (stream_state) return stream_state;
        decode_state_t decode_state = decode.init(&in);
        switch (decode_state) {
        case decode_state_ready: break;
        case decode_state_stall_on_input: return stream_state_stall_on_input;
        default: return stream_state_error_invalid_internal_state;
        }
        total_bytes_read = &decode.total_read;
        total_bytes_written = &decode.total_written;
        process = stream_process_decompression_inited;
        if (header_information != NULL) {
            main_header_t header = decode.header;
            header_information->major_version = header.version[0];
            header_information->minor_version = header.version[1];
            header_information->revision = header.version[2];
            header_information->compression_mode = (compression_mode_t)header.compression_mode;
            header_information->block_type = (block_type_t)header.block_type;
        }
        return stream_state_ready;
    }

    stream_state_t
    stream_t::decompress_continue(void)
    {
        if (process != stream_process_decompression_started) {
            if (process != stream_process_decompression_inited)
                return stream_state_error_invalid_internal_state;
            process = stream_process_decompression_started;
        }
        stream_state_t stream_state = check_conformity();
        if (stream_state) return stream_state;
        decode_state_t decode_state = decode.continue_(&in, &out);
        switch (decode_state) {
        case decode_state_ready: return stream_state_ready;
        case decode_state_stall_on_input: return stream_state_stall_on_input;
        case decode_state_stall_on_output: return stream_state_stall_on_output;
        case decode_state_integrity_check_fail: return stream_state_error_integrity_check_fail;
        default: return stream_state_error_invalid_internal_state;
        }
    }

    stream_state_t
    stream_t::decompress_finish(void)
    {
        if (process != stream_process_decompression_started) {
            if (process != stream_process_decompression_inited)
                return stream_state_error_invalid_internal_state;
            process = stream_process_decompression_started;
        }
        decode_state_t decode_state = decode.finish(&in, &out);
        switch (decode_state) {
        case decode_state_ready: break;
        case decode_state_stall_on_output: return stream_state_stall_on_output;
        case decode_state_integrity_check_fail: return stream_state_error_integrity_check_fail;
        default: return stream_state_error_invalid_internal_state;
        }
        process = stream_process_decompression_finished;
        return stream_state_ready;
    }
}
