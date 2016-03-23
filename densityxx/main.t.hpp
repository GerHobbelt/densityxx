// see LICENSE.md for license.
#pragma once
#include "densityxx/main.def.t.hpp"

namespace density {
    // encode.
    template<class KERNEL_ENCODE_T> inline encode_base_t::state_t
    encode_t<KERNEL_ENCODE_T>::init(location_t *RESTRICT out, const block_type_t block_type)
    {
        this->block_type = block_type;
        total_read = total_written = 0;
#if DENSITY_WRITE_MAIN_HEADER == DENSITY_YES
        state_t state;
        if ((state = write_header(out, block_encode.mode(), block_type)))
            return exit_process(process_write_header, state);
#endif
        block_encode.init(block_type);
        return exit_process(process_write_blocks, state_ready);
    }

    template<class KERNEL_ENCODE_T> inline encode_base_t::state_t
    encode_t<KERNEL_ENCODE_T>::continue_(teleport_t *RESTRICT in, location_t *RESTRICT out)
    {
        state_t block_encode_state;
        uint_fast64_t available_in_before, available_out_before;
        // Dispatch
        switch (process) {
        case process_write_blocks: goto write_blocks;
        default: return state_error;
        }
    write_blocks:
        available_in_before = in->available_bytes();
        available_out_before = out->available_bytes;
        block_encode_state = block_encode.continue_(in, out);
        update_totals(in, out, available_in_before, available_out_before);
        switch (block_encode_state) {
        case state_ready: break;
        case state_stall_on_input:
            return exit_process(process_write_blocks, state_stall_on_input);
        case state_stall_on_output:
            return exit_process(process_write_blocks, state_stall_on_output);
        case state_error: return state_error;
        }
        goto write_blocks;
    }

    template<class KERNEL_ENCODE_T> inline encode_base_t::state_t
    encode_t<KERNEL_ENCODE_T>::finish(teleport_t *RESTRICT in, location_t *RESTRICT out)
    {
        state_t block_encode_state;
        uint_fast64_t available_in_before, available_out_before;
        // Dispatch
        switch (process) {
        case process_write_blocks:  goto write_blocks;
        case process_write_footer:  goto write_footer;
        default:  return state_error;
        }
    write_blocks:
        available_in_before = in->available_bytes();
        available_out_before = out->available_bytes;
        block_encode_state = block_encode.finish(in, out);
        update_totals(in, out, available_in_before, available_out_before);
        switch (block_encode_state) {
        case state_ready: break;
        case state_stall_on_output:
            return exit_process(process_write_blocks, state_stall_on_output);
        default:  return state_error;
        }
    write_footer:
#if DENSITY_WRITE_MAIN_FOOTER == DENSITY_YES && DENSITY_ENABLE_PARALLELIZABLE_DECOMPRESSIBLE_OUTPUT == DENSITY_YES
        state_t state;
        if ((state = write_footer(out, block_encode.read_bytes())))
            return exit_process(process_write_footer, state);
#endif
        return state_ready;
    }

    inline encode_base_t::state_t
    encode_base_t::write_header(location_t *RESTRICT out,
                                const compression_mode_t compression_mode,
                                const block_type_t block_type)
    {
        main_header_t main_header;
        if (sizeof(main_header_t) > out->available_bytes) return state_stall_on_output;
        main_header_parameters_t parameters;
        memset(&parameters, 0, sizeof(parameters));
#if DENSITY_ENABLE_PARALLELIZABLE_DECOMPRESSIBLE_OUTPUT == DENSITY_YES
        parameters.as_bytes[0] = dictionary_preferred_reset_cycle_shift;
#endif
        total_written += main_header.write(out, compression_mode, block_type, parameters);
        return state_ready;
    }

    inline encode_base_t::state_t
    encode_base_t::write_footer(location_t *RESTRICT out, uint32_t sz)
    {
        main_footer_t main_footer;
        if (sizeof(main_footer_t) > out->available_bytes) return state_stall_on_output;
        if (sz > 0) total_written += main_footer.write(out, sz);
        return state_ready;
    }

    // decode.
    template<class KERNEL_DECODE_T> inline decode_base_t::state_t
    decode_t<KERNEL_DECODE_T>::init(teleport_t *in)
    {
        total_read = total_written = 0;
#if DENSITY_WRITE_MAIN_HEADER == DENSITY_YES
        state_t state;
        if ((state = read_header(in))) return exit_process(process_read_header, state);
#endif
        block_decode.init((block_type_t)header.block_type, header.parameters,
                          end_data_overhead);
        return exit_process(process_read_blocks, state_ready);
    }
    
    template<class KERNEL_DECODE_T> inline decode_base_t::state_t
    decode_t<KERNEL_DECODE_T>::continue_(teleport_t *RESTRICT in, location_t *RESTRICT out)
    {
        state_t block_decode_state;
        uint_fast64_t available_in_before, available_out_before;
        switch (process) {
        case process_read_blocks: goto read_blocks;
        default:  return state_error;
        }
    read_blocks:
        available_in_before = in->available_bytes_reserved(end_data_overhead);
        available_out_before = out->available_bytes;

        block_decode_state = block_decode.continue_(in, out);
        update_totals(in, out, available_in_before, available_out_before);
        switch (block_decode_state) {
        case state_ready: break;
        case state_stall_on_input:
            return exit_process(process_read_blocks, state_stall_on_input);
        case state_stall_on_output:
            return exit_process(process_read_blocks, state_stall_on_output);
        case state_integrity_check_fail:
            return exit_process(process_read_blocks, state_integrity_check_fail);
        case state_error:  return state_error;
        }
        goto read_blocks;
    }

    template<class KERNEL_DECODE_T> inline decode_base_t::state_t
    decode_t<KERNEL_DECODE_T>::finish(teleport_t *RESTRICT in, location_t *RESTRICT out)
    {
        state_t block_decode_state;
        uint_fast64_t available_in_before, available_out_before;
        switch (process) {
        case process_read_blocks:  goto read_blocks;
        case process_read_footer:  goto read_footer;
        default:  return state_error;
        }
    read_blocks:
        available_in_before = in->available_bytes_reserved(end_data_overhead);
        available_out_before = out->available_bytes;
        block_decode_state = block_decode.finish(in, out);
        update_totals(in, out, available_in_before, available_out_before);
        switch (block_decode_state) {
        case state_ready: break;
        case state_stall_on_output:
            return exit_process(process_read_blocks, state_stall_on_output);
        case state_integrity_check_fail:
            return exit_process(process_read_blocks, state_integrity_check_fail);
        default:   return state_error;
        }
    read_footer:
#if DENSITY_WRITE_MAIN_FOOTER == DENSITY_YES && DENSITY_ENABLE_PARALLELIZABLE_DECOMPRESSIBLE_OUTPUT == DENSITY_YES
        state_t state;
        if ((state = read_footer(in))) return state;
#endif
        return state_ready;
    }

    inline decode_base_t::state_t
    decode_base_t::read_header(teleport_t *RESTRICT in)
    {
        location_t *read_location;
        if (!(read_location = in->read_reserved(sizeof(header), end_data_overhead)))
            return state_stall_on_input;
        total_read += header.read(read_location);
        return state_ready;
    }
    inline decode_base_t::state_t
    decode_base_t::read_footer(teleport_t *RESTRICT in)
    {
        location_t *read_location;
        if (!(read_location = in->read(sizeof(footer)))) return state_stall_on_input;
        total_read += footer.read(read_location);
        return state_ready;
    }
}
