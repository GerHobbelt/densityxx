// see LICENSE.md for license.
#include "densityxx/stream.def.hpp"

namespace density {
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
    template<class KERNEL_ENCODE_T>stream_t::state_t
    stream_encode_t<KERNEL_ENCODE_T>::init(const block_type_t block_type)
    {
        if (process != process_prepared) return state_error_invalid_internal_state;
        state_t state = check_conformity();
        if (state) return state;
        encode_base_t::state_t encode_state = encode.init(&out, block_type);
        switch (encode_state) {
        case encode_base_t::state_ready: break;
        case encode_base_t::state_stall_on_input: return state_stall_on_input;
        case encode_base_t::state_stall_on_output: return state_stall_on_output;
        default: return state_error_invalid_internal_state;
        }
        process = process_inited;
        return state_ready;
    }
    template<class KERNEL_ENCODE_T>stream_t::state_t
    stream_encode_t<KERNEL_ENCODE_T>::continue_(void)
    {
        if (process != process_started) {
            if (process != process_inited) return state_error_invalid_internal_state;
            process = process_started;
        }
        state_t state = check_conformity();
        if (state) return state;
        encode_base_t::state_t encode_state = encode.continue_(&in, &out);
        switch (encode_state) {
        case encode_base_t::state_ready: return state_ready;
        case encode_base_t::state_stall_on_input: return state_stall_on_input;
        case encode_base_t::state_stall_on_output: return state_stall_on_output;
        default: return state_error_invalid_internal_state;
        }
    }
    template<class KERNEL_ENCODE_T>stream_t::state_t
    stream_encode_t<KERNEL_ENCODE_T>::finish(void)
    {
        if (process != process_started) {
            if (process != process_inited) return state_error_invalid_internal_state;
            process = process_started;
        }
        state_t state = check_conformity();
        if (state) return state;
        encode_base_t::state_t encode_state = encode.finish(&in, &out);
        switch (encode_state) {
        case encode_base_t::state_ready: break;
        case encode_base_t::state_stall_on_output: return state_stall_on_output;
        default: return state_error_invalid_internal_state;
        }
        process = process_finished;
        return state_ready;
    }

    template<class KERNEL_DECODE_T>stream_t::state_t
    stream_decode_t<KERNEL_DECODE_T>::init(stream_t::header_information_t *header_information)
    {
        if (process != process_prepared)
            return state_error_invalid_internal_state;
        stream_t::state_t state = check_conformity();
        if (state) return state;
        decode_base_t::state_t decode_state = decode.init(&in);
        switch (decode_state) {
        case decode_base_t::state_ready: break;
        case decode_base_t::state_stall_on_input: return state_stall_on_input;
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

    template<class KERNEL_DECODE_T>stream_t::state_t
    stream_decode_t<KERNEL_DECODE_T>::continue_(void)
    {
        if (process != process_started) {
            if (process != process_inited) return state_error_invalid_internal_state;
            process = process_started;
        }
        state_t state = check_conformity();
        if (state) return state;
        decode_base_t::state_t decode_state = decode.continue_(&in, &out);
        switch (decode_state) {
        case decode_base_t::state_ready: return state_ready;
        case decode_base_t::state_stall_on_input: return state_stall_on_input;
        case decode_base_t::state_stall_on_output: return state_stall_on_output;
        case decode_base_t::state_integrity_check_fail: return state_error_integrity_check_fail;
        default: return state_error_invalid_internal_state;
        }
    }

    template<class KERNEL_DECODE_T>stream_t::state_t
    stream_decode_t<KERNEL_DECODE_T>::finish(void)
    {
        if (process != process_started) {
            if (process != process_inited) return state_error_invalid_internal_state;
            process = process_started;
        }
        decode_base_t::state_t decode_state = decode.finish(&in, &out);
        switch (decode_state) {
        case decode_base_t::state_ready: break;
        case decode_base_t::state_stall_on_output: return state_stall_on_output;
        case decode_base_t::state_integrity_check_fail: return state_error_integrity_check_fail;
        default: return state_error_invalid_internal_state;
        }
        process = process_finished;
        return state_ready;
    }
}
