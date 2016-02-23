// see LICENSE.md for license.
#pragma once
#include "densityxx/block.def.hpp"

namespace density {
    const uint64_t preferred_copy_block_size = 1 << 19;
    const uint64_t spookyhash_seed_1 = 0xabc;
    const uint64_t spookyhash_seed_2 = 0xdef;
    // encode.
    inline block_encode_state_t
    block_encode_t::init(kernel_encode_t *kernel_encode, const block_type_t block_type)
    {
        this->kernel_encode = kernel_encode;
        target_mode = kernel_encode ? kernel_encode->mode(): compression_mode_copy;
        current_mode = target_mode;
        this->block_type = block_type;
        total_read = total_written = 0;
        if (block_type == block_type_with_hashsum_integrity_check) update = true;
        if (kernel_encode != NULL) kernel_encode->init();
        return exit_process(block_encode_process_write_block_header,
                            block_encode_state_ready);
    }
    inline block_encode_state_t
    block_encode_t::continue_(teleport_t *RESTRICT in, location_t *RESTRICT out)
    {
        block_encode_state_t block_encode_state;
        kernel_encode_state_t kernel_encode_state;
        uint_fast64_t available_in_before, available_out_before;
        // Add to the integrity check hashsum
        if (block_type == block_type_with_hashsum_integrity_check && update)
            update_integrity_data(in);
        // Dispatch
        switch (process) {
        case block_encode_process_write_block_header: goto write_block_header;
        case block_encode_process_write_block_mode_marker: goto write_mode_marker;
        case block_encode_process_write_data: goto write_data;
        case block_encode_process_write_block_footer: goto write_block_footer;
        default: return block_encode_state_error;
        }
    write_mode_marker:
        if ((block_encode_state = write_mode_marker(out)))
            return exit_process(block_encode_process_write_block_mode_marker,
                                block_encode_state);
        goto write_data;
    write_block_header:
        if ((block_encode_state = write_block_header(in, out)))
            return exit_process(block_encode_process_write_block_header, block_encode_state);
    write_data:
        available_in_before = in->available_bytes();
        available_out_before = out->available_bytes;
        if (current_mode == compression_mode_copy) {
            uint_fast64_t block_remaining = (uint_fast64_t)
                preferred_copy_block_size - (total_read - in_start);
            uint_fast64_t in_remaining = in->available_bytes();
            uint_fast64_t out_remaining = out->available_bytes;
            if (in_remaining <= out_remaining) {
                if (block_remaining <= in_remaining) goto copy_until_end_of_block;
                else {
                    in->copy(out, in_remaining);
                    update_totals(in, out, available_in_before, available_out_before);
                    if (block_type == block_type_with_hashsum_integrity_check)
                        update_integrity_hash(in, true);
                    in->copy_from_direct_buffer_to_staging_buffer();
                    return exit_process(block_encode_process_write_data,
                                        block_encode_state_stall_on_input);
                }
            } else {
                if (block_remaining <= out_remaining) goto copy_until_end_of_block;
                else {
                    in->copy(out, out_remaining);
                    update_totals(in, out, available_in_before, available_out_before);
                    return exit_process(block_encode_process_write_data,
                                        block_encode_state_stall_on_output);
                }
            }
        copy_until_end_of_block:
            in->copy(out, block_remaining);
            update_totals(in, out, available_in_before, available_out_before);
            goto write_block_footer;
        } else { // current_mode == target_mode
            kernel_encode_state = kernel_encode->continue_(in, out);
            update_totals(in, out, available_in_before, available_out_before);
            switch (kernel_encode_state) {
            case kernel_encode_state_stall_on_input:
                if (block_type == block_type_with_hashsum_integrity_check)
                    update_integrity_hash(in, true);
                return exit_process(block_encode_process_write_data,
                                    block_encode_state_stall_on_input);
            case kernel_encode_state_stall_on_output:
                return exit_process(block_encode_process_write_data,
                                    block_encode_state_stall_on_output);
            case kernel_encode_state_info_new_block: goto write_block_footer;
            case kernel_encode_state_info_efficiency_check: goto write_mode_marker;
            default: return block_encode_state_error;
            }
        }
    write_block_footer:
        if (block_type == block_type_with_hashsum_integrity_check &&
            (block_encode_state = write_block_footer(in, out)))
            return exit_process(block_encode_process_write_block_footer, block_encode_state);
        goto write_block_header;
    }
    inline block_encode_state_t
    block_encode_t::finish(teleport_t *RESTRICT in, location_t *RESTRICT out)
    {
        block_encode_state_t block_encode_state;
        kernel_encode_state_t kernel_encode_state;
        uint_fast64_t available_in_before, available_out_before;
        // Add to the integrity check hashsum
        if (block_type == block_type_with_hashsum_integrity_check && update)
            update_integrity_data(in);
        // Dispatch
        switch (process) {
        case block_encode_process_write_block_header: goto write_block_header;
        case block_encode_process_write_block_mode_marker: goto write_mode_marker;
        case block_encode_process_write_data: goto write_data;
        case block_encode_process_write_block_footer: goto write_block_footer;
        default: return block_encode_state_error;
        }
    write_mode_marker:
        if ((block_encode_state = write_mode_marker(out)))
            return exit_process(block_encode_process_write_block_mode_marker,
                                block_encode_state);
        goto write_data;
    write_block_header:
        if ((block_encode_state = write_block_header(in, out)))
            return exit_process(block_encode_process_write_block_header, block_encode_state);
    write_data:
        available_in_before = in->available_bytes();
        available_out_before = out->available_bytes;
        if (current_mode == compression_mode_copy) {
            uint_fast64_t block_remaining = (uint_fast64_t)
                preferred_copy_block_size - (total_read - in_start);
            uint_fast64_t in_remaining = in->available_bytes();
            uint_fast64_t out_remaining = out->available_bytes;
            if (in_remaining <= out_remaining) {
                if (block_remaining <= in_remaining) goto copy_until_end_of_block;
                else {
                    in->copy(out, in_remaining);
                    update_totals(in, out, available_in_before, available_out_before);
                    goto write_block_footer;
                }
            } else {
                if (block_remaining <= out_remaining) goto copy_until_end_of_block;
                else {
                    in->copy(out, out_remaining);
                    update_totals(in, out, available_in_before, available_out_before);
                    return exit_process(block_encode_process_write_data,
                                        block_encode_state_stall_on_output);
                }
            }
        copy_until_end_of_block:
            in->copy(out, block_remaining);
            update_totals(in, out, available_in_before, available_out_before);
            goto write_block_footer;
        } else { // current_mode == target_mode
            kernel_encode_state = kernel_encode->finish(in, out);
            update_totals(in, out, in->available_bytes(), out->available_bytes);
            switch (kernel_encode_state) {
            case kernel_encode_state_stall_on_input: return block_encode_state_error;
            case kernel_encode_state_stall_on_output:
                return exit_process(block_encode_process_write_data,
                                    block_encode_state_stall_on_output);
            case kernel_encode_state_ready:
            case kernel_encode_state_info_new_block: goto write_block_footer;
            case kernel_encode_state_info_efficiency_check: goto write_mode_marker;
            default: return block_encode_state_error;
            }
        }
    write_block_footer:
        if (block_type == block_type_with_hashsum_integrity_check &&
            (block_encode_state = write_block_footer(in, out)))
            return exit_process(block_encode_process_write_block_footer, block_encode_state);
        if (in->available_bytes()) goto write_block_header;
        if (kernel_encode != NULL) delete kernel_encode;
        return block_encode_state_ready;
    }

    inline void
    block_encode_t::update_integrity_hash(teleport_t *RESTRICT in, bool pending_exit)
    {
        const uint8_t *const pointer_before = input_pointer;
        const uint8_t *const pointer_after = in->direct.pointer;
        const uint_fast64_t processed = pointer_after - pointer_before;
        if (pending_exit) {
            context.update(input_pointer, processed);
            update = true;
        } else {
            context.update(input_pointer, processed - in->staging.available_bytes);
            update_integrity_data(in);
        }
    }
    inline block_encode_state_t
    block_encode_t::write_block_header(teleport_t *RESTRICT in, location_t *RESTRICT out)
    {
        if (sizeof(block_header_t) > out->available_bytes)
            return block_encode_state_stall_on_output;
        current_mode = target_mode;
#if DENSITY_ENABLE_PARALLELIZABLE_DECOMPRESSIBLE_OUTPUT == DENSITY_YES
        block_header_t block_header;
        if (total_read > 0) {
            uint32_t sz = total_read - in_start;
            total_written += block_header.write(out, sz);
        }
#endif
        in_start = total_read;
        out_start = total_written;

        if (block_type == block_type_with_hashsum_integrity_check) {
            context.init(spookyhash_seed_1, spookyhash_seed_2);
            context.update(in->staging.pointer, in->staging.available_bytes);
            update_integrity_data(in);
        }
        return block_encode_state_ready;
    }
    inline block_encode_state_t
    block_encode_t::write_block_footer(teleport_t *RESTRICT in, location_t *RESTRICT out)
    {
        block_footer_t block_footer;
        if (sizeof(block_footer) > out->available_bytes)
            return block_encode_state_stall_on_output;
        update_integrity_hash(in, false);
        context.final(&block_footer.hashsum1, &block_footer.hashsum2);
        total_written += block_footer.write(out);
        return block_encode_state_ready;
    }
    inline block_encode_state_t
    block_encode_t::write_mode_marker(location_t *RESTRICT out)
    {
        block_mode_marker_t block_mode_marker;
        if (sizeof(block_mode_marker) > out->available_bytes)
            return block_encode_state_stall_on_output;
        if (current_mode == target_mode && total_written > total_read)
            current_mode = compression_mode_copy;
        total_written += block_mode_marker.write(out, current_mode);
        return block_encode_state_ready;
    }

    // decode.
    inline block_decode_state_t
    block_decode_t::init(kernel_decode_t *kernel_decode, const block_type_t block_type,
                         const main_header_parameters_t parameters,
                         const uint_fast8_t end_data_overhead)
    {
        target_mode = kernel_decode ? kernel_decode->mode(): compression_mode_copy;
        current_mode = target_mode;
        this->block_type = block_type;
        this->kernel_decode = kernel_decode;
        read_block_header_content = parameters.as_bytes[0] ? true: false;
        total_read = total_written = 0;
        this->end_data_overhead = end_data_overhead;
        if (block_type == block_type_with_hashsum_integrity_check) {
            update = true;
            this->end_data_overhead += sizeof(block_footer_t);
        }
        if (kernel_decode != NULL) kernel_decode->init(parameters, this->end_data_overhead);
        return exit_process(block_decode_process_read_block_header, block_decode_state_ready);
    }
    inline block_decode_state_t
    block_decode_t::continue_(teleport_t *RESTRICT in, location_t *RESTRICT out)
    {
        kernel_decode_state_t kernel_decode_state;
        block_decode_state_t block_decode_state;
        uint_fast64_t available_in_before, available_out_before;
        // Update integrity pointers
        if (block_type == block_type_with_hashsum_integrity_check && update)
            update_integrity_data(out);
        // Dispatch
        switch (process) {
        case block_decode_process_read_block_header: goto read_block_header;
        case block_decode_process_read_block_mode_marker: goto read_mode_marker;
        case block_decode_process_read_data: goto read_data;
        case block_decode_process_read_block_footer: goto read_block_footer;
        default: return block_decode_state_error;
        }
    read_mode_marker:
        if ((block_decode_state = read_block_mode_marker(in)))
            return exit_process(block_decode_process_read_block_mode_marker,
                                block_decode_state);
        goto read_data;
    read_block_header:
        if ((block_decode_state = read_block_header(in, out)))
            return exit_process(block_decode_process_read_block_header, block_decode_state);
    read_data:
        available_in_before = in->available_bytes_reserved(end_data_overhead);
        available_out_before = out->available_bytes;
        if (current_mode == compression_mode_copy) {
            uint_fast64_t block_remaining = (uint_fast64_t)
                preferred_copy_block_size - (total_written - out_start);
            uint_fast64_t in_remaining = in->available_bytes_reserved(end_data_overhead);
            uint_fast64_t out_remaining = out->available_bytes;
            if (in_remaining <= out_remaining) {
                if (block_remaining <= in_remaining) goto copy_until_end_of_block;
                else {
                    in->copy(out, in_remaining);
                    update_totals(in, out, available_in_before, available_out_before);
                    in->copy_from_direct_buffer_to_staging_buffer();
                    return exit_process(block_decode_process_read_data,
                                        block_decode_state_stall_on_input);
                }
            } else {
                if (block_remaining <= out_remaining) goto copy_until_end_of_block;
                else {
                    in->copy(out, out_remaining);
                    update_totals(in, out, available_in_before, available_out_before);
                    if (block_type == block_type_with_hashsum_integrity_check)
                        update_integrity_hash(out, true);
                    return exit_process(block_decode_process_read_data,
                                        block_decode_state_stall_on_output);
                }
            }
        copy_until_end_of_block:
            in->copy(out, block_remaining);
            update_totals(in, out, available_in_before, available_out_before);
            goto read_block_footer;
        } else { // current_mode == target_mode
            kernel_decode_state = kernel_decode->continue_(in, out);
            update_totals(in, out, available_in_before, available_out_before);
            switch (kernel_decode_state) {
            case kernel_decode_state_stall_on_input:
                return exit_process(block_decode_process_read_data,
                                    block_decode_state_stall_on_input);
            case kernel_decode_state_stall_on_output:
                if (block_type == block_type_with_hashsum_integrity_check)
                    update_integrity_hash(out, true);
                return exit_process(block_decode_process_read_data,
                                    block_decode_state_stall_on_output);
            case kernel_decode_state_info_new_block: goto read_block_footer;
            case kernel_decode_state_info_efficiency_check: goto read_mode_marker;
            default: return block_decode_state_error;
            }
        }
    read_block_footer:
        if (block_type == block_type_with_hashsum_integrity_check &&
            (block_decode_state = read_block_footer(in, out)))
            return exit_process(block_decode_process_read_block_footer, block_decode_state);
        goto read_block_header;
    }
    inline block_decode_state_t
    block_decode_t::finish(teleport_t *RESTRICT in, location_t *RESTRICT out)
    {
        kernel_decode_state_t kernel_decode_state;
        block_decode_state_t block_decode_state;
        uint_fast64_t available_in_before, available_out_before;
        // Update integrity pointers
        if (block_type == block_type_with_hashsum_integrity_check && update)
            update_integrity_data(out);
        // Dispatch
        switch (process) {
        case block_decode_process_read_block_header: goto read_block_header;
        case block_decode_process_read_block_mode_marker: goto read_mode_marker;
        case block_decode_process_read_data: goto read_data;
        case block_decode_process_read_block_footer: goto read_block_footer;
        default: return block_decode_state_error;
        }
    read_mode_marker:
        if ((block_decode_state = read_block_mode_marker(in)))
            return exit_process(block_decode_process_read_block_mode_marker,
                                block_decode_state);
        goto read_data;
    read_block_header:
        if ((block_decode_state = read_block_header(in, out)))
            return exit_process(block_decode_process_read_block_header, block_decode_state);
    read_data:
        available_in_before = in->available_bytes_reserved(end_data_overhead);
        available_out_before = out->available_bytes;
        if (current_mode == compression_mode_copy) {
            uint_fast64_t block_remaining = (uint_fast64_t)
                preferred_copy_block_size - (total_written - out_start);
            uint_fast64_t in_remaining = in->available_bytes_reserved(end_data_overhead);
            uint_fast64_t out_remaining = out->available_bytes;
            if (in_remaining <= out_remaining) {
                if (block_remaining <= in_remaining) goto copy_until_end_of_block;
                else {
                    in->copy(out, in_remaining);
                    update_totals(in, out, available_in_before, available_out_before);
                    goto read_block_footer;
                }
            } else {
                if (block_remaining <= out_remaining) goto copy_until_end_of_block;
                else {
                    in->copy(out, out_remaining);
                    update_totals(in, out, available_in_before, available_out_before);
                    if (block_type == block_type_with_hashsum_integrity_check)
                        update_integrity_hash(out, true);
                    return exit_process(block_decode_process_read_data,
                                        block_decode_state_stall_on_output);
                }
            }
        copy_until_end_of_block:
            in->copy(out, block_remaining);
            update_totals(in, out, available_in_before, available_out_before);
            goto read_block_footer;
        } else { // current_mode == target_mode
            kernel_decode_state = kernel_decode->finish(in, out);
            update_totals(in, out, available_in_before, available_out_before);
            switch (kernel_decode_state) {
            case kernel_decode_state_stall_on_input: return block_decode_state_error;
            case kernel_decode_state_stall_on_output:
                if (block_type == block_type_with_hashsum_integrity_check)
                    update_integrity_hash(out, true);
                return exit_process(block_decode_process_read_data,
                                    block_decode_state_stall_on_output);
            case kernel_decode_state_ready:
            case kernel_decode_state_info_new_block: goto read_block_footer;
            case kernel_decode_state_info_efficiency_check: goto read_mode_marker;
            default: return block_decode_state_error;
            }
        }
    read_block_footer:
        if (block_type == block_type_with_hashsum_integrity_check &&
            (block_decode_state = read_block_footer(in, out)))
            return exit_process(block_decode_process_read_block_footer, block_decode_state);
        if (in->available_bytes_reserved(end_data_overhead)) goto read_block_header;
        if (kernel_decode != NULL) delete kernel_decode;
        return block_decode_state_ready;
    }

    inline void
    block_decode_t::update_integrity_hash(location_t *RESTRICT out, bool pending_exit)
    {
        const uint8_t *const pointer_before = output_pointer;
        const uint8_t *const pointer_after = out->pointer;
        const uint_fast64_t processed = pointer_after - pointer_before;
        context.update(output_pointer, processed);
        if (pending_exit) update = true;
        else update_integrity_data(out);
    }
    inline block_decode_state_t
    block_decode_t::read_block_header(teleport_t *RESTRICT in, location_t *out)
    {
        location_t *read_location;
        if (!(read_location = in->read_reserved(sizeof(last_block_header), end_data_overhead)))
            return block_decode_state_stall_on_input;
        in_start = total_read;
        out_start = total_written;
        if (read_block_header_content)
            total_read += last_block_header.read(read_location);
        if (block_type == block_type_with_hashsum_integrity_check) {
            context.init(spookyhash_seed_1, spookyhash_seed_2);
            update_integrity_data(out);
        }
        return block_decode_state_ready;
    }
    inline block_decode_state_t
    block_decode_t::read_block_mode_marker(teleport_t *RESTRICT in)
    {
        location_t *read_location;
        if (!(read_location = in->read_reserved(sizeof(last_mode_marker), end_data_overhead)))
            return block_decode_state_stall_on_input;
        total_read += last_mode_marker.read(read_location);
        current_mode = (compression_mode_t)last_mode_marker.mode;
        return block_decode_state_ready;
    }
    inline block_decode_state_t
    block_decode_t::read_block_footer(teleport_t *RESTRICT in, location_t *RESTRICT out)
    {
        location_t *read_location;
        if (!(read_location = in->read(sizeof(last_block_footer))))
            return block_decode_state_stall_on_input;
        update_integrity_hash(out, false);
        uint64_t hashsum1, hashsum2;
        context.final(&hashsum1, &hashsum2);
        total_read += last_block_footer.read(read_location);
        return last_block_footer.check(hashsum1, hashsum2) ? block_decode_state_ready:
            block_decode_state_integrity_check_fail;
    }
}
