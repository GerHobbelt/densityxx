#include "densityxx/cheetah.hpp"
#include "densityxx/mathmacros.hpp"

namespace density {
#define DENSITY_CHEETAH_PREFERRED_BLOCK_SIGNATURES_SHIFT             12
#define DENSITY_CHEETAH_PREFERRED_BLOCK_SIGNATURES              \
    (1 << DENSITY_CHEETAH_PREFERRED_BLOCK_SIGNATURES_SHIFT)

#define DENSITY_CHEETAH_PREFERRED_EFFICIENCY_CHECK_SIGNATURES_SHIFT  8
#define DENSITY_CHEETAH_PREFERRED_EFFICIENCY_CHECK_SIGNATURES           \
    (1 << DENSITY_CHEETAH_PREFERRED_EFFICIENCY_CHECK_SIGNATURES_SHIFT)

#define DENSITY_CHEETAH_MAXIMUM_COMPRESSED_BODY_SIZE_PER_SIGNATURE      \
    ((DENSITY_BITSIZEOF(cheetah_signature_t) >> 1) * sizeof(uint32_t))   // Uncompressed chunks
#define DENSITY_CHEETAH_DECOMPRESSED_BODY_SIZE_PER_SIGNATURE            \
    ((DENSITY_BITSIZEOF(cheetah_signature_t) >> 1) * sizeof(uint32_t))
#define DENSITY_CHEETAH_MAXIMUM_COMPRESSED_UNIT_SIZE                    \
    (sizeof(cheetah_signature_t) + DENSITY_CHEETAH_MAXIMUM_COMPRESSED_BODY_SIZE_PER_SIGNATURE)
#define DENSITY_CHEETAH_DECOMPRESSED_UNIT_SIZE                  \
    (DENSITY_CHEETAH_DECOMPRESSED_BODY_SIZE_PER_SIGNATURE)

    // encode.
#define DENSITY_CHEETAH_ENCODE_PROCESS_UNIT_SIZE                        \
    ((DENSITY_BITSIZEOF(cheetah_signature_t) >> 1) * sizeof(uint32_t))
    void
    cheetah_encode_t::prepare_new_signature(location_t *RESTRICT out)
    {
        signatures_count++;
        shift = 0;
        signature = (cheetah_signature_t *) (out->pointer);
        proximity_signature = 0;
        signature_copied_to_memory = false;
        out->pointer += sizeof(cheetah_signature_t);
        out->available_bytes -= sizeof(cheetah_signature_t);
    }
    kernel_encode_state_t
    cheetah_encode_t::prepare_new_block(location_t *RESTRICT out)
    {
        if (DENSITY_CHEETAH_MAXIMUM_COMPRESSED_UNIT_SIZE > out->available_bytes)
            return kernel_encode_state_stall_on_output;
        switch (signatures_count) {
        case DENSITY_CHEETAH_PREFERRED_EFFICIENCY_CHECK_SIGNATURES:
            if (!efficiency_checked) {
                efficiency_checked = true;
                return kernel_encode_state_info_efficiency_check;
            }
            break;
        case DENSITY_CHEETAH_PREFERRED_BLOCK_SIGNATURES:
            signatures_count = 0;
            efficiency_checked = 0;
#if DENSITY_ENABLE_PARALLELIZABLE_DECOMPRESSIBLE_OUTPUT == DENSITY_YES
            if (reset_cycle) --reset_cycle;
            else {
                dictionary.reset();
                reset_cycle = DENSITY_DICTIONARY_PREFERRED_RESET_CYCLE - 1;
            }
#endif
            return kernel_encode_state_info_new_block;
        default: break;
        }
        prepare_new_signature(out);
        return kernel_encode_state_ready;
    }
    kernel_encode_state_t
    cheetah_encode_t::check_state(location_t *RESTRICT out)
    {
        kernel_encode_state_t return_state;
        switch (shift) {
        case DENSITY_BITSIZEOF(cheetah_signature_t):
            if(DENSITY_LIKELY(!signature_copied_to_memory)) {
                // Avoid dual copying in case of mode reversion
                DENSITY_MEMCPY(signature, &proximity_signature, sizeof(proximity_signature));
                signature_copied_to_memory = true;
            }
            if ((return_state = prepare_new_block(out))) return return_state;
            break;
        default: break;
        }
        return kernel_encode_state_ready;
    }
    void
    cheetah_encode_t::kernel(location_t *RESTRICT out, const uint16_t hash,
                             const uint32_t chunk, const uint_fast8_t shift)
    {
        uint32_t *predicted_chunk = (uint32_t *)&dictionary.prediction_entries[last_hash];
        if (*predicted_chunk != chunk) {
            cheetah_dictionary_entry_t *found = &dictionary.entries[hash];
            uint32_t *found_a = &found->chunk_a;
            if (*found_a != chunk) {
                uint32_t *found_b = &found->chunk_b;
                if (*found_b != chunk) {
                    proximity_signature |= ((uint64_t)cheetah_signature_flag_chunk << shift);
                    DENSITY_MEMCPY(out->pointer, &chunk, sizeof(chunk));
                    out->pointer += sizeof(chunk);
                } else {
                    proximity_signature |= ((uint64_t)cheetah_signature_flag_map_b << shift);
                    DENSITY_MEMCPY(out->pointer, &hash, sizeof(hash));
                    out->pointer += sizeof(hash);
                }
                *found_b = *found_a;
                *found_a = chunk;
            } else {
                proximity_signature |= ((uint64_t)cheetah_signature_flag_map_a << shift);
                DENSITY_MEMCPY(out->pointer, &hash, sizeof(hash));
                out->pointer += sizeof(hash);
            }
            *predicted_chunk = chunk;
        }
        last_hash = hash;
    }
    void
    cheetah_encode_t::process_unit(location_t *RESTRICT in, location_t *RESTRICT out)
    {
        uint32_t chunk;
        uint_fast8_t count = 0;
#ifdef __clang__
        for(; count < DENSITY_BITSIZEOF(cheetah_signature_t); count += 2) {
            DENSITY_MEMCPY(&chunk, in->pointer, sizeof(chunk));
            kernel(out, DENSITY_CHEETAH_HASH_ALGORITHM(chunk), chunk, count);
            in->pointer += sizeof(uint32_t);
        }
#else
        for (uint_fast8_t count_b = 0; count_b < 16; count_b++) {
            DENSITY_UNROLL_2                                            \
                (DENSITY_MEMCPY(&chunk, in->pointer, sizeof(chunk));    \
                 kernel(out, DENSITY_CHEETAH_HASH_ALGORITHM(chunk), chunk, count); \
                 in->pointer += sizeof(chunk);                          \
                 count += 2);
        }
#endif
        shift = DENSITY_BITSIZEOF(cheetah_signature_t);
    }

    kernel_encode_state_t
    cheetah_encode_t::init(void)
    {
        signatures_count = 0;
        efficiency_checked = 0;
        dictionary.reset();
#if DENSITY_ENABLE_PARALLELIZABLE_DECOMPRESSIBLE_OUTPUT == DENSITY_YES
        reset_cycle = DENSITY_DICTIONARY_PREFERRED_RESET_CYCLE - 1;
#endif
        last_hash = 0;
        return exit_process(cheetah_encode_process_prepare_new_block,
                            kernel_encode_state_ready);
    }
    kernel_encode_state_t
    cheetah_encode_t::continue_(teleport_t *RESTRICT in, location_t *RESTRICT out)
    {
        kernel_encode_state_t return_state;
        uint8_t *pointer_out_before;
        location_t *read_memory_location;
        // Dispatch
        switch (process) {
        case cheetah_encode_process_prepare_new_block: goto prepare_new_block;
        case cheetah_encode_process_check_signature_state: goto check_signature_state;
        case cheetah_encode_process_read_chunk: goto read_chunk;
        default: return kernel_encode_state_error;
        }
        // Prepare new block
    prepare_new_block:
        if ((return_state = prepare_new_block(out)))
            return exit_process(cheetah_encode_process_prepare_new_block, return_state);
        // Check signature state
    check_signature_state:
        if ((return_state = check_state(out)))
            return exit_process(cheetah_encode_process_check_signature_state, return_state);
        // Try to read a complete chunk unit
    read_chunk:
        pointer_out_before = out->pointer;
        if (!(read_memory_location = in->read(DENSITY_CHEETAH_ENCODE_PROCESS_UNIT_SIZE)))
            return exit_process(cheetah_encode_process_read_chunk,
                                kernel_encode_state_stall_on_input);
        // Chunk was read properly, process
        process_unit(read_memory_location, out);
        read_memory_location->available_bytes -= DENSITY_CHEETAH_ENCODE_PROCESS_UNIT_SIZE;
        out->available_bytes -= (out->pointer - pointer_out_before);
        // New loop
        goto check_signature_state;
    }
    kernel_encode_state_t
    cheetah_encode_t::finish(teleport_t *RESTRICT in, location_t *RESTRICT out)
    {
        kernel_encode_state_t return_state;
        uint8_t *pointer_out_before;
        location_t *read_memory_location;
        // Dispatch
        switch (process) {
        case cheetah_encode_process_prepare_new_block: goto prepare_new_block;
        case cheetah_encode_process_check_signature_state: goto check_signature_state;
        case cheetah_encode_process_read_chunk: goto read_chunk;
        default: return kernel_encode_state_error;
        }
        // Prepare new block
    prepare_new_block:
        if ((return_state = prepare_new_block(out)))
            return exit_process(cheetah_encode_process_prepare_new_block, return_state);
        // Check signature state
    check_signature_state:
        if ((return_state = check_state(out)))
            return exit_process(cheetah_encode_process_check_signature_state, return_state);
        // Try to read a complete chunk unit
    read_chunk:
        pointer_out_before = out->pointer;
        if (!(read_memory_location = in->read(DENSITY_CHEETAH_ENCODE_PROCESS_UNIT_SIZE)))
            goto step_by_step;
        // Chunk was read properly, process
        process_unit(read_memory_location, out);
        read_memory_location->available_bytes -= DENSITY_CHEETAH_ENCODE_PROCESS_UNIT_SIZE;
        goto exit;
        // Read step by step
    step_by_step:
        while (this->shift != DENSITY_BITSIZEOF(cheetah_signature_t) &&
               (read_memory_location = in->read(sizeof(uint32_t)))) {
            uint32_t chunk;
            DENSITY_MEMCPY(&chunk, read_memory_location->pointer, sizeof(uint32_t));
            kernel(out, DENSITY_CHEETAH_HASH_ALGORITHM(LITTLE_ENDIAN_32(chunk)), chunk, shift);
            shift += 2;
            read_memory_location->pointer += sizeof(chunk);
            read_memory_location->available_bytes -= sizeof(chunk);
        }
    exit:
        out->available_bytes -= (out->pointer - pointer_out_before);
        if (in->available_bytes() >= sizeof(uint32_t)) goto check_signature_state;
        // Marker for decode loop exit
        if ((return_state = check_state(out)))
            return exit_process(cheetah_encode_process_check_signature_state, return_state);
        proximity_signature |= ((uint64_t)cheetah_signature_flag_chunk << shift);
        // Copy the remaining bytes
        DENSITY_MEMCPY(signature, &proximity_signature, sizeof(proximity_signature));
        in->copy_remaining(out);
        return kernel_encode_state_ready;
    }

    // decode.
    kernel_decode_state_t
    cheetah_decode_t::check_state(location_t *RESTRICT out)
    {
        if (out->available_bytes < DENSITY_CHEETAH_DECOMPRESSED_UNIT_SIZE)
            return kernel_decode_state_stall_on_output;
        switch (signatures_count) {
        case DENSITY_CHEETAH_PREFERRED_EFFICIENCY_CHECK_SIGNATURES:
            if (!efficiency_checked) {
                efficiency_checked = true;
                return kernel_decode_state_info_efficiency_check;
            }
            break;
        case DENSITY_CHEETAH_PREFERRED_BLOCK_SIGNATURES:
            signatures_count = 0;
            efficiency_checked = 0;
            if (reset_cycle) --reset_cycle;
            else {
                uint8_t reset_dictionary_cycle_shift = parameters.as_bytes[0];
                if (reset_dictionary_cycle_shift) {
                    dictionary.reset();
                    reset_cycle = ((uint_fast64_t) 1 << reset_dictionary_cycle_shift) - 1;
                }
            }
            return kernel_decode_state_info_new_block;
        default: break;
        }
        return kernel_decode_state_ready;
    }
    void
    cheetah_decode_t::read_signature(location_t *RESTRICT in)
    {
        DENSITY_MEMCPY(&signature, in->pointer, sizeof(signature));
        in->pointer += sizeof(signature);
        shift = 0;
        signatures_count++;
    }
    void
    cheetah_decode_t::process_predicted(location_t *RESTRICT out)
    {
        const uint32_t chunk = dictionary.prediction_entries[last_hash].next_chunk_prediction;
        DENSITY_MEMCPY(out->pointer, &chunk, sizeof(chunk));
        last_hash = DENSITY_CHEETAH_HASH_ALGORITHM(chunk);
    }
    void
    cheetah_decode_t::process_compressed_a(const uint16_t hash, location_t *RESTRICT out)
    {
        __builtin_prefetch(&dictionary.prediction_entries[hash]);
        const uint32_t chunk = dictionary.entries[hash].chunk_a;
        DENSITY_MEMCPY(out->pointer, &chunk, sizeof(chunk));
        dictionary.prediction_entries[last_hash].next_chunk_prediction = chunk;
        last_hash = hash;
    }
    void
    cheetah_decode_t::process_compressed_b(const uint16_t hash, location_t *RESTRICT out)
    {
        __builtin_prefetch(&dictionary.prediction_entries[hash]);
        cheetah_dictionary_entry_t *const entry = &dictionary.entries[hash];
        const uint32_t chunk = entry->chunk_b;
        entry->chunk_b = entry->chunk_a;
        entry->chunk_a = chunk;
        DENSITY_MEMCPY(out->pointer, &chunk, sizeof(chunk));
        dictionary.prediction_entries[last_hash].next_chunk_prediction = chunk;
        last_hash = hash;
    }
    void
    cheetah_decode_t::process_uncompressed(const uint32_t chunk, location_t *RESTRICT out)
    {
        const uint16_t hash = DENSITY_CHEETAH_HASH_ALGORITHM(chunk);
        __builtin_prefetch(&dictionary.prediction_entries[hash]);
        cheetah_dictionary_entry_t *const entry = &dictionary.entries[hash];
        entry->chunk_b = entry->chunk_a;
        entry->chunk_a = chunk;
        DENSITY_MEMCPY(out->pointer, &chunk, sizeof(chunk));
        dictionary.prediction_entries[last_hash].next_chunk_prediction = chunk;
        last_hash = hash;
    }
    void
    cheetah_decode_t::kernel(location_t *RESTRICT in, location_t *RESTRICT out,
                             const uint8_t mode)
    {
        uint16_t hash;
        uint32_t chunk;
        switch (mode) {
            DENSITY_CASE_GENERATOR_4_4_COMBINED                         \
                (process_predicted(out);,                               \
                 cheetah_signature_flag_predicted,                      \
                 DENSITY_MEMCPY(&hash, in->pointer, sizeof(hash));      \
                 process_compressed_a(hash, out);                       \
                 in->pointer += sizeof(hash);,                          \
                 cheetah_signature_flag_map_a,                          \
                 DENSITY_MEMCPY(&hash, in->pointer, sizeof(hash));      \
                 process_compressed_b(hash, out);                       \
                 in->pointer += sizeof(hash);,                          \
                 cheetah_signature_flag_map_b,                          \
                 DENSITY_MEMCPY(&chunk, in->pointer, sizeof(chunk));    \
                 process_uncompressed(chunk, out);                      \
                 in->pointer += sizeof(chunk);,                         \
                 cheetah_signature_flag_chunk,                          \
                 out->pointer += sizeof(chunk);,                        \
                 2                                                      \
                 );
        default: break;
        }
        out->pointer += sizeof(uint32_t);
    }
    void
    cheetah_decode_t::process_data(location_t *RESTRICT in, location_t *RESTRICT out)
    {
#ifdef __clang__
        uint_fast8_t count = 0;
        for (uint_fast8_t count_b = 0; count_b < 8; count_b ++) {
            kernel(in, out, (uint8_t const) ((signature >> count) & 0xFF));
            count += 8;
        }
#else
        for (uint_fast8_t count_b = 0;
             count_b < DENSITY_BITSIZEOF(cheetah_signature_t); count_b += 8)
            kernel(in, out, (uint8_t const) ((signature >> count_b) & 0xFF));
#endif
        shift = DENSITY_BITSIZEOF(cheetah_signature_t);
    }

    kernel_decode_state_t
    cheetah_decode_t::init(const main_header_parameters_t parameters,
                           const uint_fast8_t end_data_overhead)
    {
        signatures_count = 0;
        efficiency_checked = 0;
        dictionary.reset();
        this->parameters = parameters;
        uint8_t reset_dictionary_cycle_shift = parameters.as_bytes[0];
        if (reset_dictionary_cycle_shift)
            reset_cycle = ((uint_fast64_t) 1 << reset_dictionary_cycle_shift) - 1;
        this->end_data_overhead = end_data_overhead;
        last_hash = 0;
        return exit_process(cheetah_decode_process_check_signature_state,
                            kernel_decode_state_ready);
    }
    kernel_decode_state_t
    cheetah_decode_t::continue_(teleport_t *RESTRICT in, location_t *RESTRICT out)
    {
        kernel_decode_state_t return_state;
        location_t *read_memory_location;
        // Dispatch
        switch (process) {
        case cheetah_decode_process_check_signature_state: goto check_signature_state;
        case cheetah_decode_process_read_processing_unit: goto read_processing_unit;
        default: return kernel_decode_state_error;
        }
    check_signature_state:
        if (DENSITY_UNLIKELY((return_state = check_state(out))))
            return exit_process(cheetah_decode_process_check_signature_state, return_state);
        // Try to read the next processing unit
    read_processing_unit:
        read_memory_location = in->read_reserved(DENSITY_CHEETAH_MAXIMUM_COMPRESSED_UNIT_SIZE,
                                                 end_data_overhead);
        if (DENSITY_UNLIKELY(!read_memory_location))
            return exit_process(cheetah_decode_process_read_processing_unit,
                                kernel_decode_state_stall_on_input);
        uint8_t *read_memory_location_pointer_before = read_memory_location->pointer;
        // Decode the signature (endian processing)
        read_signature(read_memory_location);
        // Process body
        process_data(read_memory_location, out);
        read_memory_location->available_bytes -=
            (read_memory_location->pointer - read_memory_location_pointer_before);
        out->available_bytes -= DENSITY_CHEETAH_DECOMPRESSED_UNIT_SIZE;
        // New loop
        goto check_signature_state;
    }
    kernel_decode_state_t
    cheetah_decode_t::finish(teleport_t *RESTRICT in, location_t *RESTRICT out)
    {
        kernel_decode_state_t return_state;
        location_t *read_memory_location;
        uint_fast64_t available_bytes_reserved;
        uint8_t *read_memory_location_pointer_before;
        // Dispatch
        switch (process) {
        case cheetah_decode_process_check_signature_state: goto check_signature_state;
        case cheetah_decode_process_read_processing_unit: goto read_processing_unit;
        default: return kernel_decode_state_error;
        }
    check_signature_state:
        if (DENSITY_UNLIKELY((return_state = check_state(out))))
            return exit_process(cheetah_decode_process_check_signature_state, return_state);
        // Try to read the next processing unit
    read_processing_unit:
        read_memory_location = in->read_reserved(DENSITY_CHEETAH_MAXIMUM_COMPRESSED_UNIT_SIZE,
                                                 end_data_overhead);
        if (DENSITY_UNLIKELY(!read_memory_location))
            goto step_by_step;
        read_memory_location_pointer_before = read_memory_location->pointer;
        // Decode the signature (endian processing)
        read_signature(read_memory_location);
        // Process body
        process_data(read_memory_location, out);
        read_memory_location->available_bytes -=
            (read_memory_location->pointer - read_memory_location_pointer_before);
        out->available_bytes -= DENSITY_CHEETAH_DECOMPRESSED_UNIT_SIZE;
        // New loop
        goto check_signature_state;
        // Try to read and process signature and body, step by step
    step_by_step:
        read_memory_location = in->read_reserved(sizeof(cheetah_signature_t),
                                                 end_data_overhead);
        if (!read_memory_location) goto finish;
        read_signature(read_memory_location);
        read_memory_location->available_bytes -= sizeof(cheetah_signature_t);
        uint16_t hash;
        uint32_t chunk;
        while (shift != DENSITY_BITSIZEOF(cheetah_signature_t)) {
            switch ((uint8_t const) ((signature >> shift) & 0x3)) {
            case cheetah_signature_flag_predicted:
                if (out->available_bytes < sizeof(uint32_t))
                    return kernel_decode_state_error;
                process_predicted(out);
                break;
            case cheetah_signature_flag_map_a:
                read_memory_location = in->read_reserved(sizeof(uint16_t), end_data_overhead);
                if (!read_memory_location) return kernel_decode_state_error;
                if (out->available_bytes < sizeof(uint32_t)) return kernel_decode_state_error;
                DENSITY_MEMCPY(&hash, read_memory_location->pointer, sizeof(hash));
                process_compressed_a(hash, out);
                read_memory_location->pointer += sizeof(hash);
                read_memory_location->available_bytes -= sizeof(hash);
                break;
            case cheetah_signature_flag_map_b:
                read_memory_location = in->read_reserved(sizeof(uint16_t), end_data_overhead);
                if (!read_memory_location) return kernel_decode_state_error;
                if (out->available_bytes < sizeof(uint32_t)) return kernel_decode_state_error;
                DENSITY_MEMCPY(&hash, read_memory_location->pointer, sizeof(hash));
                process_compressed_b(hash, out);
                read_memory_location->pointer += sizeof(hash);
                read_memory_location->available_bytes -= sizeof(hash);
                break;
            case cheetah_signature_flag_chunk:
                read_memory_location = in->read_reserved(sizeof(uint32_t), end_data_overhead);
                if (!read_memory_location) goto finish;
                if (out->available_bytes < sizeof(chunk)) return kernel_decode_state_error;
                DENSITY_MEMCPY(&chunk, read_memory_location->pointer, sizeof(chunk));
                process_uncompressed(chunk, out);
                read_memory_location->pointer += sizeof(chunk);
                read_memory_location->available_bytes -= sizeof(chunk);
                break;
            }
            out->pointer += sizeof(uint32_t);
            out->available_bytes -= sizeof(uint32_t);
            shift += 2;
        }
        // New loop
        goto check_signature_state;
    finish:
        available_bytes_reserved = in->available_bytes_reserved(end_data_overhead);
        if (out->available_bytes < available_bytes_reserved)
            return kernel_decode_state_error;
        in->copy(out, available_bytes_reserved);
        return kernel_decode_state_ready;
    }
}
