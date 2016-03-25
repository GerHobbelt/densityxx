// see LICENSE.md for license.
#pragma once
#include "densityxx/main.def.hpp"
#include "densityxx/chameleon.hpp"
#include "densityxx/cheetah.hpp"
#include "densityxx/lion.hpp"

namespace density {
    // encode.
    inline encode_t::state_t
    encode_t::init(location_t *RESTRICT out, const compression_mode_t mode,
                   const block_type_t block_type)
    {
        kernel_encode_t *kernel_encode;
        compression_mode = mode;
        this->block_type = block_type;
        total_read = total_written = 0;
#if DENSITY_WRITE_MAIN_HEADER == DENSITY_YES
        state_t state;
        if ((state = write_header(out, mode, block_type)))
            return exit_process(process_write_header, state);
#endif
        switch (mode) {
        case compression_mode_copy: kernel_encode = NULL; break;
        case compression_mode_chameleon_algorithm:
            kernel_encode = new chameleon_encode_t();
            break;
        case compression_mode_cheetah_algorithm:
            kernel_encode = new cheetah_encode_t();
            break;
        case compression_mode_lion_algorithm:
            kernel_encode = new lion_encode_t();
            break;
        default: return state_error;
        }
        block_encode.init(kernel_encode, block_type);
        return exit_process(process_write_blocks, state_ready);
    }

    inline encode_t::state_t
    encode_t::continue_(teleport_t *RESTRICT in, location_t *RESTRICT out)
    {
        block_encode_t::state_t block_encode_state;
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
        case block_encode_t::state_ready: break;
        case block_encode_t::state_stall_on_input:
            return exit_process(process_write_blocks, state_stall_on_input);
        case block_encode_t::state_stall_on_output:
            return exit_process(process_write_blocks, state_stall_on_output);
        case block_encode_t::state_error: return state_error;
        }
        goto write_blocks;
    }

    inline encode_t::state_t
    encode_t::finish(teleport_t *RESTRICT in, location_t *RESTRICT out)
    {
        block_encode_t::state_t block_encode_state;
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
        case block_encode_t::state_ready: break;
        case block_encode_t::state_stall_on_output:
            return exit_process(process_write_blocks, state_stall_on_output);
        default:  return state_error;
        }
    write_footer:
#if DENSITY_WRITE_MAIN_FOOTER == DENSITY_YES && DENSITY_ENABLE_PARALLELIZABLE_DECOMPRESSIBLE_OUTPUT == DENSITY_YES
        state_t state;
        if ((state = write_footer(out)))
            return exit_process(process_write_footer, state);
#endif
        return state_ready;
    }

    inline encode_t::state_t
    encode_t::write_header(location_t *RESTRICT out, const compression_mode_t compression_mode,
                           const block_type_t block_type)
    {
        main_header_t main_header;
        if (sizeof(main_header_t) > out->available_bytes)
            return state_stall_on_output;
        main_header_parameters_t parameters;
        memset(&parameters, 0, sizeof(parameters));
#if DENSITY_ENABLE_PARALLELIZABLE_DECOMPRESSIBLE_OUTPUT == DENSITY_YES
        parameters.as_bytes[0] = dictionary_preferred_reset_cycle_shift;
#endif
        total_written += main_header.write(out, compression_mode, block_type, parameters);
        return state_ready;
    }

    inline encode_t::state_t
    encode_t::write_footer(location_t *RESTRICT out)
    {
        main_footer_t main_footer;
        if (sizeof(main_footer_t) > out->available_bytes) return state_stall_on_output;
        uint32_t sz = block_encode.read_bytes();
        if (sz > 0) total_written += main_footer.write(out, sz);
        return state_ready;
    }

    // decode.
    inline decode_t::state_t
    decode_t::init(teleport_t *in)
    {
        kernel_decode_t *kernel_decode;
        total_read = total_written = 0;
#if DENSITY_WRITE_MAIN_HEADER == DENSITY_YES
        state_t state;
        if ((state = read_header(in))) return exit_process(process_read_header, state);
#endif
        switch (header.compression_mode) {
        case compression_mode_copy: kernel_decode = NULL; break;
        case compression_mode_chameleon_algorithm:
            kernel_decode = new chameleon_decode_t();
            break;
        case compression_mode_cheetah_algorithm:
            kernel_decode = new cheetah_decode_t();
            break;
        case compression_mode_lion_algorithm:
            kernel_decode = new lion_decode_t();
            break;
        default: return state_error;
        }
        block_decode.init(kernel_decode, (block_type_t)header.block_type, header.parameters,
                          end_data_overhead);
        return exit_process(process_read_blocks, state_ready);
    }
    
    inline decode_t::state_t
    decode_t::continue_(teleport_t *RESTRICT in, location_t *RESTRICT out)
    {
        block_decode_t::state_t block_decode_state;
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
        case block_decode_t::state_ready: break;
        case block_decode_t::state_stall_on_input:
            return exit_process(process_read_blocks, state_stall_on_input);
        case block_decode_t::state_stall_on_output:
            return exit_process(process_read_blocks, state_stall_on_output);
        case block_decode_t::state_integrity_check_fail:
            return exit_process(process_read_blocks, state_integrity_check_fail);
        case block_decode_t::state_error:  return state_error;
        }
        goto read_blocks;
    }

    inline decode_t::state_t
    decode_t::finish(teleport_t *RESTRICT in, location_t *RESTRICT out)
    {
        block_decode_t::state_t block_decode_state;
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
        case block_decode_t::state_ready: break;
        case block_decode_t::state_stall_on_output:
            return exit_process(process_read_blocks, state_stall_on_output);
        case block_decode_t::state_integrity_check_fail:
            return exit_process(process_read_blocks, state_integrity_check_fail);
        default:   return state_error;
        }
    read_footer:
#if DENSITY_WRITE_MAIN_FOOTER == DENSITY_YES && DENSITY_ENABLE_PARALLELIZABLE_DECOMPRESSIBLE_OUTPUT == DENSITY_YES
        decode_t::state_t state;
        if ((state = read_footer(in))) return state;
#endif
        return state_ready;
    }

    inline decode_t::state_t
    decode_t::read_header(teleport_t *RESTRICT in)
    {
        location_t *read_location;
        if (!(read_location = in->read_reserved(sizeof(header), end_data_overhead)))
            return state_stall_on_input;
        total_read += header.read(read_location);
        return state_ready;
    }
    inline decode_t::state_t
    decode_t::read_footer(teleport_t *RESTRICT in)
    {
        location_t *read_location;
        if (!(read_location = in->read(sizeof(footer)))) return state_stall_on_input;
        total_read += footer.read(read_location);
        return state_ready;
    }
}
