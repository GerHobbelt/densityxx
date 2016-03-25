// see LICENSE.md for license.
#include "densityxx/api.def.hpp"

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
}
