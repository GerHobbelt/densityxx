// see LICENSE.md for license.
#include <typeinfo>
#include "densityxx/chameleon.hpp"
#include "densityxx/mathmacros.hpp"

#ifdef DENSITY_SHOW
#define DENSITY_SHOW_ENCODE(LABEL)                                      \
    fprintf(stderr, "%s::%s(%u/%s):\n",                                 \
            typeid(*this).name(), __FUNCTION__, __LINE__, #LABEL);      \
    fprintf(stderr, "    proximity_signature(%llx)\n", (unsigned long long)proximity_signature); \
    fprintf(stderr, "    shift(%u)\n", (unsigned)shift);                \
    fprintf(stderr, "    signatures_count(%u)\n", (unsigned)signatures_count); \
    fprintf(stderr, "    efficiency_checked(%s)\n", efficiency_checked ? "true": "false"); \
    fprintf(stderr, "    signature_copied_to_memory(%s)\n", signature_copied_to_memory ? "true": "false"); \
    fprintf(stderr, "    process(%s)\n", chameleon_encode_process_render(process).c_str())
#else
#define DENSITY_SHOW_ENCODE(LABEL)
#endif

namespace density {
#define DENSITY_CHAMELEON_PREFERRED_BLOCK_SIGNATURES_SHIFT      11
#define DENSITY_CHAMELEON_PREFERRED_BLOCK_SIGNATURES            \
    (1 << DENSITY_CHAMELEON_PREFERRED_BLOCK_SIGNATURES_SHIFT)

#define DENSITY_CHAMELEON_PREFERRED_EFFICIENCY_CHECK_SIGNATURES_SHIFT   7
#define DENSITY_CHAMELEON_PREFERRED_EFFICIENCY_CHECK_SIGNATURES         \
    (1 << DENSITY_CHAMELEON_PREFERRED_EFFICIENCY_CHECK_SIGNATURES_SHIFT)


    // Uncompressed chunks
#define DENSITY_CHAMELEON_MAXIMUM_COMPRESSED_BODY_SIZE_PER_SIGNATURE    \
    (DENSITY_BITSIZEOF(chameleon_signature_t) * sizeof(uint32_t))
#define DENSITY_CHAMELEON_DECOMPRESSED_BODY_SIZE_PER_SIGNATURE          \
    (DENSITY_BITSIZEOF(chameleon_signature_t) * sizeof(uint32_t))

#define DENSITY_CHAMELEON_MAXIMUM_COMPRESSED_UNIT_SIZE                  \
    (sizeof(chameleon_signature_t) +                                    \
     DENSITY_CHAMELEON_MAXIMUM_COMPRESSED_BODY_SIZE_PER_SIGNATURE)
#define DENSITY_CHAMELEON_DECOMPRESSED_UNIT_SIZE                \
    (DENSITY_CHAMELEON_DECOMPRESSED_BODY_SIZE_PER_SIGNATURE)

    //--- encode ---
#define DENSITY_CHAMELEON_ENCODE_PROCESS_UNIT_SIZE                      \
    (DENSITY_BITSIZEOF(chameleon_signature_t) * sizeof(uint32_t))

    inline void
    chameleon_encode_t::prepare_new_signature(location_t *RESTRICT out)
    {
        signatures_count++;
        shift = 0;
        signature = (chameleon_signature_t *)(out->pointer);
        proximity_signature = 0;
        signature_copied_to_memory = false;
        //DENSITY_SHOW_OUT(out, sizeof(chameleon_signature_t));
        out->pointer += sizeof(chameleon_signature_t);
        out->available_bytes -= sizeof(chameleon_signature_t);
    }

    inline kernel_encode_state_t
    chameleon_encode_t::prepare_new_block(location_t *RESTRICT out)
    {
        if (DENSITY_CHAMELEON_MAXIMUM_COMPRESSED_UNIT_SIZE > out->available_bytes)
            return kernel_encode_state_stall_on_output;
        switch (signatures_count) {
        case DENSITY_CHAMELEON_PREFERRED_EFFICIENCY_CHECK_SIGNATURES:
            if (!efficiency_checked) {
                efficiency_checked = true;
                return kernel_encode_state_info_efficiency_check;
            }
            break;
        case DENSITY_CHAMELEON_PREFERRED_BLOCK_SIGNATURES:
            signatures_count = 0;
            efficiency_checked = false;
#if DENSITY_ENABLE_PARALLELIZABLE_DECOMPRESSIBLE_OUTPUT == DENSITY_YES
            if (reset_cycle) --reset_cycle;
            else {
                dictionary.reset();
                reset_cycle = dictionary_preferred_reset_cycle - 1;
            }
#endif
            return kernel_encode_state_info_new_block;
        default: break;
        }
        chameleon_encode_t::prepare_new_signature(out);
        return kernel_encode_state_ready;
    }

    inline kernel_encode_state_t
    chameleon_encode_t::check_state(location_t *RESTRICT out)
    {
        kernel_encode_state_t kernel_encode_state;
        switch (shift) {
        case DENSITY_BITSIZEOF(chameleon_signature_t):
            if (DENSITY_LIKELY(!signature_copied_to_memory)) {
                // Avoid dual copying in case of mode reversion
                DENSITY_MEMCPY(signature, &proximity_signature, sizeof(proximity_signature));
                signature_copied_to_memory = true;
            }
            if ((kernel_encode_state = prepare_new_block(out))) return kernel_encode_state;
            break;
        default: break;
        }
        return kernel_encode_state_ready;
    }

    inline void
    chameleon_encode_t::kernel(location_t *RESTRICT out, const uint16_t hash,
                               const uint32_t chunk, const uint_fast8_t shift)
    {
        chameleon_dictionary_entry_t *const found = &dictionary.entries[hash];
        if (chunk != found->as_uint32_t) {
            found->as_uint32_t = chunk;
            //DENSITY_SHOW_OUT(out, sizeof(chunk));
            DENSITY_MEMCPY(out->pointer, &chunk, sizeof(chunk));
            out->pointer += sizeof(chunk);
        } else {
            proximity_signature |= ((uint64_t)chameleon_signature_flag_map << shift);
            //DENSITY_SHOW_OUT(out, sizeof(hash));
            DENSITY_MEMCPY(out->pointer, &hash, sizeof(hash));
            out->pointer += sizeof(hash);
        }
    }

    inline void
    chameleon_encode_t::process_unit(location_t *RESTRICT in, location_t *RESTRICT out)
    {
        uint32_t chunk;
        uint_fast8_t count = 0;
        //DENSITY_SHOW_IN(in, 64 * sizeof(uint32_t));
#ifdef __clang__
        for (uint_fast8_t count_b = 0; count_b < 32; count_b++) {
            DENSITY_UNROLL_2(DENSITY_MEMCPY(&chunk, in->pointer, sizeof(uint32_t)); \
                             kernel(out, DENSITY_CHAMELEON_HASH_ALGORITHM(chunk), \
                                    chunk, count++);                    \
                             in->pointer += sizeof(uint32_t);           \
                             );
        }
#else
        for (uint_fast8_t count_b = 0; count_b < 16; count_b++) {
            DENSITY_UNROLL_4(DENSITY_MEMCPY(&chunk, in->pointer, sizeof(uint32_t)); \
                             kernel(out, DENSITY_CHAMELEON_HASH_ALGORITHM(chunk), \
                                    chunk, count++);                    \
                             in->pointer += sizeof(uint32_t);           \
                             );
        }
#endif
        shift = DENSITY_BITSIZEOF(chameleon_signature_t);
    }

    kernel_encode_state_t
    chameleon_encode_t::init(void)
    {
        signatures_count = 0;
        efficiency_checked = 0;
        dictionary.reset();
#if DENSITY_ENABLE_PARALLELIZABLE_DECOMPRESSIBLE_OUTPUT == DENSITY_YES
        reset_cycle = DENSITY_DICTIONARY_PREFERRED_RESET_CYCLE - 1;
#endif
        DENSITY_SHOW_ENCODE(init);
        return exit_process(chameleon_encode_process_prepare_new_block,
                            kernel_encode_state_ready);
    }
    kernel_encode_state_t
    chameleon_encode_t::continue_(teleport_t *RESTRICT in, location_t *RESTRICT out)
    {
        kernel_encode_state_t return_state;
        uint8_t *pointer_out_before;
        location_t *read_memory_location;
        // Dispatch
        switch (process) {
        case chameleon_encode_process_prepare_new_block: goto prepare_new_block;
        case chameleon_encode_process_check_signature_state: goto check_signature_state;
        case chameleon_encode_process_read_chunk: goto read_chunk;
        default: return kernel_encode_state_error;
        }
        // Prepare new block
    prepare_new_block:
        DENSITY_SHOW_ENCODE(prepare_new_block);
        if ((return_state = prepare_new_block(out)))
            return exit_process(chameleon_encode_process_prepare_new_block, return_state);
        // Check signature state
    check_signature_state:
        DENSITY_SHOW_ENCODE(check_signature_state);
        if ((return_state = check_state(out)))
            return exit_process(chameleon_encode_process_check_signature_state, return_state);
        // Try to read a complete chunk unit
    read_chunk:
        DENSITY_SHOW_ENCODE(read_chunk);
        pointer_out_before = out->pointer;
        if (!(read_memory_location = in->read(DENSITY_CHAMELEON_ENCODE_PROCESS_UNIT_SIZE)))
            return exit_process(chameleon_encode_process_read_chunk,
                                kernel_encode_state_stall_on_input);
        // Chunk was read properly, process
        process_unit(read_memory_location, out);
        read_memory_location->available_bytes -= DENSITY_CHAMELEON_ENCODE_PROCESS_UNIT_SIZE;
        out->available_bytes -= (out->pointer - pointer_out_before);
        // New loop
        goto check_signature_state;
    }
    kernel_encode_state_t
    chameleon_encode_t::finish(teleport_t *RESTRICT in, location_t *RESTRICT out)
    {
        kernel_encode_state_t return_state;
        uint8_t *pointer_out_before;
        location_t *read_memory_location;
        // Dispatch
        switch (process) {
        case chameleon_encode_process_prepare_new_block: goto prepare_new_block;
        case chameleon_encode_process_check_signature_state: goto check_signature_state;
        case chameleon_encode_process_read_chunk: goto read_chunk;
        default: return kernel_encode_state_error;
        }
        // Prepare new block
    prepare_new_block:
        DENSITY_SHOW_ENCODE(prepare_new_block);
        if ((return_state = prepare_new_block(out)))
            return exit_process(chameleon_encode_process_prepare_new_block, return_state);
        // Check signature state
    check_signature_state:
        DENSITY_SHOW_ENCODE(check_signature_state);
        if ((return_state = check_state(out)))
            return exit_process(chameleon_encode_process_check_signature_state, return_state);
        // Try to read a complete chunk unit
    read_chunk:
        DENSITY_SHOW_ENCODE(read_chunk);
        pointer_out_before = out->pointer;
        if (!(read_memory_location = in->read(DENSITY_CHAMELEON_ENCODE_PROCESS_UNIT_SIZE)))
            goto step_by_step;
        // Chunk was read properly, process
        process_unit(read_memory_location, out);
        read_memory_location->available_bytes -= DENSITY_CHAMELEON_ENCODE_PROCESS_UNIT_SIZE;
        goto exit;
        // Read step by step
    step_by_step:
        DENSITY_SHOW_ENCODE(step_by_step);
        while (shift != DENSITY_BITSIZEOF(chameleon_signature_t) &&
               (read_memory_location = in->read(sizeof(uint32_t)))) {
            uint32_t chunk;
            DENSITY_MEMCPY(&chunk, read_memory_location->pointer, sizeof(chunk));
            kernel(out, DENSITY_CHAMELEON_HASH_ALGORITHM(chunk), chunk, shift);
            ++shift;
            read_memory_location->pointer += sizeof(chunk);
            read_memory_location->available_bytes -= sizeof(chunk);
        }
    exit:
        DENSITY_SHOW_ENCODE(exit);
        out->available_bytes -= (out->pointer - pointer_out_before);
        if (in->available_bytes() >= sizeof(uint32_t)) goto check_signature_state;
        // Copy the remaining bytes
        DENSITY_MEMCPY(signature, &proximity_signature, sizeof(proximity_signature));
        in->copy_remaining(out);
        return kernel_encode_state_ready;
    }

    //--- decode ---
    inline kernel_decode_state_t
    chameleon_decode_t::check_state(location_t *RESTRICT out)
    {
        if (out->available_bytes < DENSITY_CHAMELEON_DECOMPRESSED_UNIT_SIZE)
            return kernel_decode_state_stall_on_output;
        switch (signatures_count) {
        case DENSITY_CHAMELEON_PREFERRED_EFFICIENCY_CHECK_SIGNATURES:
            if (!efficiency_checked) {
                efficiency_checked = true;
                return kernel_decode_state_info_efficiency_check;
            }
            break;
        case DENSITY_CHAMELEON_PREFERRED_BLOCK_SIGNATURES:
            signatures_count = 0;
            efficiency_checked = false;
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

    inline void
    chameleon_decode_t::read_signature(location_t *RESTRICT in)
    {
        //DENSITY_SHOW_IN(in, sizeof(signature));
        DENSITY_MEMCPY(&signature, in->pointer, sizeof(signature));
        in->pointer += sizeof(signature);
        shift = 0;
        signatures_count++;
    }

    inline void
    chameleon_decode_t::kernel(location_t *RESTRICT in, location_t *RESTRICT out,
                               const bool compressed)
    {
        if (compressed) {
            uint16_t hash;
            //DENSITY_SHOW_IN(in, sizeof(hash));
            DENSITY_MEMCPY(&hash, in->pointer, sizeof(hash));
            process_compressed(hash, out);
            in->pointer += sizeof(hash);
        } else {
            uint32_t chunk;
            //DENSITY_SHOW_IN(in, sizeof(chunk));
            DENSITY_MEMCPY(&chunk, in->pointer, sizeof(chunk));
            process_uncompressed(chunk, out);
            in->pointer += sizeof(chunk);
        }
        //DENSITY_SHOW_OUT(out, sizeof(uint32_t));
        out->pointer += sizeof(uint32_t);
    }

    inline void
    chameleon_decode_t::process_data(location_t *RESTRICT in, location_t *RESTRICT out)
    {
        uint_fast8_t count = 0;
#ifdef __clang__
        for(uint_fast8_t count_b = 0; count_b < 8; count_b ++) {
            DENSITY_UNROLL_8(kernel(in, out, test_compressed(count++)));
        }
#else
        for(uint_fast8_t count_b = 0; count_b < 16; count_b ++) {
            DENSITY_UNROLL_4(kernel(in, out, test_compressed(count++)));
        }
#endif
        shift = DENSITY_BITSIZEOF(chameleon_signature_t);
    }

    kernel_decode_state_t
    chameleon_decode_t::init(const main_header_parameters_t parameters,
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
        return exit_process(chameleon_decode_process_check_signature_state,
                            kernel_decode_state_ready);
    }
    kernel_decode_state_t
    chameleon_decode_t::continue_(teleport_t *RESTRICT in, location_t *RESTRICT out)
    {
        kernel_decode_state_t return_state;
        location_t *read_memory_location;
        // Dispatch
        switch (process) {
        case chameleon_decode_process_check_signature_state: goto check_signature_state;
        case chameleon_decode_process_read_processing_unit: goto read_processing_unit;
        default: return kernel_decode_state_error;
        }
    check_signature_state:
        if ((return_state = check_state(out)))
            return exit_process(chameleon_decode_process_check_signature_state, return_state);
        // Try to read the next processing unit
    read_processing_unit:
        if (!(read_memory_location =
              in->read_reserved(DENSITY_CHAMELEON_MAXIMUM_COMPRESSED_UNIT_SIZE,
                                end_data_overhead)))
            return exit_process(chameleon_decode_process_read_processing_unit,
                                kernel_decode_state_stall_on_input);
        uint8_t *read_memory_location_pointer_before = read_memory_location->pointer;
        // Decode the signature (endian processing)
        read_signature(read_memory_location);
        // Process body
        process_data(read_memory_location, out);
        read_memory_location->available_bytes -=
            (read_memory_location->pointer - read_memory_location_pointer_before);
        out->available_bytes -= DENSITY_CHAMELEON_DECOMPRESSED_UNIT_SIZE;
        // New loop
        goto check_signature_state;
    }
    kernel_decode_state_t
    chameleon_decode_t::finish(teleport_t *RESTRICT in, location_t *RESTRICT out)
    {
        kernel_decode_state_t return_state;
        location_t *read_memory_location;
        uint_fast64_t available_bytes_reserved;
        uint8_t *read_memory_location_pointer_before;
        // Dispatch
        switch (this->process) {
        case chameleon_decode_process_check_signature_state: goto check_signature_state;
        case chameleon_decode_process_read_processing_unit: goto read_processing_unit;
        default: return kernel_decode_state_error;
        }
    check_signature_state:
        if ((return_state = check_state(out)))
            return exit_process(chameleon_decode_process_check_signature_state, return_state);
        // Try to read the next processing unit
    read_processing_unit:
        if (!(read_memory_location =
              in->read_reserved(DENSITY_CHAMELEON_MAXIMUM_COMPRESSED_UNIT_SIZE,
                                end_data_overhead)))
            goto step_by_step;
        read_memory_location_pointer_before = read_memory_location->pointer;
        // Decode the signature (endian processing)
        read_signature(read_memory_location);
        // Process body
        process_data(read_memory_location, out);
        read_memory_location->available_bytes -=
            (read_memory_location->pointer - read_memory_location_pointer_before);
        out->available_bytes -= DENSITY_CHAMELEON_DECOMPRESSED_UNIT_SIZE;
        // New loop
        goto check_signature_state;
        // Try to read and process signature and body, step by step
    step_by_step:
        if (!(read_memory_location =
              in->read_reserved(sizeof(chameleon_signature_t), end_data_overhead)))
            goto finish;
        read_signature(read_memory_location);
        read_memory_location->available_bytes -= sizeof(chameleon_signature_t);
        while (shift != DENSITY_BITSIZEOF(chameleon_signature_t)) {
            if (test_compressed(shift)) {
                if (!(read_memory_location =
                      in->read_reserved(sizeof(uint16_t), end_data_overhead)))
                    return kernel_decode_state_error;
                if(out->available_bytes < sizeof(uint32_t))
                    return kernel_decode_state_error;
                uint16_t hash;
                DENSITY_MEMCPY(&hash, read_memory_location->pointer, sizeof(hash));
                process_compressed(hash, out);
                read_memory_location->pointer += sizeof(hash);
                read_memory_location->available_bytes -= sizeof(hash);
            } else {
                if (!(read_memory_location =
                      in->read_reserved(sizeof(uint32_t), end_data_overhead)))
                    goto finish;
                if(out->available_bytes < sizeof(uint32_t)) return kernel_decode_state_error;
                uint32_t chunk;
                DENSITY_MEMCPY(&chunk, read_memory_location->pointer, sizeof(chunk));
                process_uncompressed(chunk, out);
                read_memory_location->pointer += sizeof(chunk);
                read_memory_location->available_bytes -= sizeof(chunk);
            }
            out->pointer += sizeof(uint32_t);
            out->available_bytes -= sizeof(uint32_t);
            ++shift;
        }
        // New loop
        goto check_signature_state;
    finish:
        available_bytes_reserved = in->available_bytes_reserved(end_data_overhead);
        if(out->available_bytes < available_bytes_reserved) return kernel_decode_state_error;
        in->copy(out, available_bytes_reserved);
        return kernel_decode_state_ready;
    }
}
