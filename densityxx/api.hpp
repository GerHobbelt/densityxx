// see LICENSE.md for license.
#include "densityxx/api.def.hpp"
#include "densityxx/main.hpp"

namespace density {
    // buffer.
    inline buffer_processing_result_t
    buffer_return_processing_result(stream_encode_t *stream, buffer_state_t state)
    {
        buffer_processing_result_t result;
        result.state = state;
        result.bytes_read = stream->get_total_read();
        result.bytes_written = stream->get_total_written();
        delete stream;
        return result;
    }
    inline buffer_processing_result_t
    buffer_return_processing_result(stream_decode_t *stream, buffer_state_t state)
    {
        buffer_processing_result_t result;
        result.state = state;
        result.bytes_read = stream->get_total_read();
        result.bytes_written = stream->get_total_written();
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
        stream_t::state_t stream_state;
        stream_encode_t *stream = new stream_encode_t();
        if ((stream_state = stream->prepare(in, szin, out, szout)))
            BUFFER_RETURN(error_during_processing);
        switch (stream_state = stream->init(compression_mode, block_type)) {
        case stream_t::state_ready: break;
        case stream_t::state_error_output_buffer_too_small:
            BUFFER_RETURN(error_output_buffer_too_small);
        default:
            BUFFER_RETURN(error_during_processing);
        }
        switch (stream_state = stream->continue_()) {
        case stream_t::state_ready:
        case stream_t::state_stall_on_input: break;
        case stream_t::state_error_output_buffer_too_small:
            BUFFER_RETURN(error_output_buffer_too_small);
        default:
            BUFFER_RETURN(error_during_processing);
        }
        switch (stream_state = stream->finish()) {
        case stream_t::state_ready: BUFFER_RETURN(ok);
        case stream_t::state_error_output_buffer_too_small:
            BUFFER_RETURN(error_output_buffer_too_small);
        default:
            BUFFER_RETURN(error_during_processing);
        }
    }

    buffer_processing_result_t
    buffer_decompress(const uint8_t *in, const uint_fast64_t szin,
                      uint8_t *out, const uint_fast64_t szout)
    {
        stream_t::state_t stream_state;
        stream_decode_t *stream = new stream_decode_t();
        if ((stream_state = stream->prepare(in, szin, out, szout)))
            BUFFER_RETURN(error_during_processing);
        switch (stream_state = stream->init(NULL)) {
        case stream_t::state_ready: break;
        case stream_t::state_error_output_buffer_too_small:
            BUFFER_RETURN(error_output_buffer_too_small);
        default:
            BUFFER_RETURN(error_during_processing);
        }
        switch (stream_state = stream->continue_()) {
        case stream_t::state_ready:
        case stream_t::state_stall_on_input:
        case stream_t::state_stall_on_output: break;
        case stream_t::state_error_output_buffer_too_small:
            BUFFER_RETURN(error_output_buffer_too_small);
        case stream_t::state_error_integrity_check_fail:
            BUFFER_RETURN(error_integrity_check_fail);
        default:
            BUFFER_RETURN(error_during_processing);
        }
        switch (stream_state = stream->finish()) {
        case stream_t::state_ready: BUFFER_RETURN(ok);
        case stream_t::state_error_output_buffer_too_small:
            BUFFER_RETURN(error_output_buffer_too_small);
        case stream_t::state_error_integrity_check_fail:
            BUFFER_RETURN(error_integrity_check_fail);
        default:
            BUFFER_RETURN(error_during_processing);
        }
    }

    // stream.
    stream_t::state_t
    stream_t::prepare(const uint8_t *RESTRICT in, const uint_fast64_t available_in,
                      uint8_t *RESTRICT out, const uint_fast64_t available_out)
    {
        this->in.reset_staging_buffer();
        update_input(in, available_in);
        update_output(out, available_out);
        process = process_prepared;
        return state_ready;
    }
    stream_t::state_t
    stream_encode_t::init(const compression_mode_t compression_mode,
                           const block_type_t block_type)
    {
        if (process != process_prepared) return state_error_invalid_internal_state;
        state_t state = check_conformity();
        if (state) return state;
        encode_t::state_t encode_state = encode.init(&out, compression_mode, block_type);
        switch (encode_state) {
        case encode_t::state_ready: break;
        case encode_t::state_stall_on_input: return state_stall_on_input;
        case encode_t::state_stall_on_output: return state_stall_on_output;
        default: return state_error_invalid_internal_state;
        }
        process = process_inited;
        return state_ready;
    }
    stream_t::state_t
    stream_encode_t::continue_(void)
    {
        if (process != process_started) {
            if (process != process_inited) return state_error_invalid_internal_state;
            process = process_started;
        }
        state_t state = check_conformity();
        if (state) return state;
        encode_t::state_t encode_state = encode.continue_(&in, &out);
        switch (encode_state) {
        case encode_t::state_ready: return state_ready;
        case encode_t::state_stall_on_input: return state_stall_on_input;
        case encode_t::state_stall_on_output: return state_stall_on_output;
        default: return state_error_invalid_internal_state;
        }
    }
    stream_t::state_t
    stream_encode_t::finish(void)
    {
        if (process != process_started) {
            if (process != process_inited) return state_error_invalid_internal_state;
            process = process_started;
        }
        state_t state = check_conformity();
        if (state) return state;
        encode_t::state_t encode_state = encode.finish(&in, &out);
        switch (encode_state) {
        case encode_t::state_ready: break;
        case encode_t::state_stall_on_output: return state_stall_on_output;
        default: return state_error_invalid_internal_state;
        }
        process = process_finished;
        return state_ready;
    }

    stream_t::state_t
    stream_decode_t::init(stream_decode_t::header_information_t *RESTRICT header_information)
    {
        if (process != process_prepared)
            return state_error_invalid_internal_state;
        stream_t::state_t state = check_conformity();
        if (state) return state;
        decode_t::state_t decode_state = decode.init(&in);
        switch (decode_state) {
        case decode_t::state_ready: break;
        case decode_t::state_stall_on_input: return state_stall_on_input;
        default: return state_error_invalid_internal_state;
        }
        process = process_inited;
        if (header_information != NULL) {
            main_header_t header(decode.get_header());
            header_information->major_version = header.version[0];
            header_information->minor_version = header.version[1];
            header_information->revision = header.version[2];
            header_information->compression_mode = (compression_mode_t)header.compression_mode;
            header_information->block_type = (block_type_t)header.block_type;
        }
        return state_ready;
    }

    stream_t::state_t
    stream_decode_t::continue_(void)
    {
        if (process != process_started) {
            if (process != process_inited) return state_error_invalid_internal_state;
            process = process_started;
        }
        state_t state = check_conformity();
        if (state) return state;
        decode_t::state_t decode_state = decode.continue_(&in, &out);
        switch (decode_state) {
        case decode_t::state_ready: return state_ready;
        case decode_t::state_stall_on_input: return state_stall_on_input;
        case decode_t::state_stall_on_output: return state_stall_on_output;
        case decode_t::state_integrity_check_fail: return state_error_integrity_check_fail;
        default: return state_error_invalid_internal_state;
        }
    }

    stream_t::state_t
    stream_decode_t::finish(void)
    {
        if (process != process_started) {
            if (process != process_inited) return state_error_invalid_internal_state;
            process = process_started;
        }
        decode_t::state_t decode_state = decode.finish(&in, &out);
        switch (decode_state) {
        case decode_t::state_ready: break;
        case decode_t::state_stall_on_output: return state_stall_on_output;
        case decode_t::state_integrity_check_fail: return state_error_integrity_check_fail;
        default: return state_error_invalid_internal_state;
        }
        process = process_finished;
        return state_ready;
    }
}
