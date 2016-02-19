#include "densityxx/chameleon.hpp"
#include "densityxx/mathmacros.hpp"

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

    void chameleon_encode_t::prepare_new_signature(location_t *RESTRICT out)
    {
        signatures_count++;
        shift = 0;
        signature = (chameleon_signature_t *)(out->pointer);
        proximity_signature = 0;
        signature_copied_to_memory = false;
        out->pointer += sizeof(chameleon_signature_t);
        out->available_bytes -= sizeof(chameleon_signature_t);
    }

    kernel_encode_state_t
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

    kernel_encode_state_t
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

    void
    chameleon_encode_t::kernel(location_t *RESTRICT out, const uint16_t hash,
                               const uint32_t chunk, const uint_fast8_t shift)
    {
        chameleon_dictionary_entry_t *const found = &dictionary.entries[hash];
        if (chunk != found->as_uint32_t) {
            found->as_uint32_t = chunk;
            DENSITY_MEMCPY(out->pointer, &chunk, sizeof(chunk));
            out->pointer += sizeof(chunk);
        } else {
            proximity_signature |= ((uint64_t)chameleon_signature_flag_map << shift);
            DENSITY_MEMCPY(out->pointer, &hash, sizeof(hash));
            out->pointer += sizeof(hash);
        }
    }

    void
    chameleon_encode_t::process_unit(location_t *RESTRICT in, location_t *RESTRICT out)
    {
        uint32_t chunk;
        uint_fast8_t count = 0;
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
        return exit_process(chameleon_encode_process_prepare_new_block,
                            kernel_encode_state_ready);
    }
    kernel_encode_state_t
    chameleon_encode_t::continue_(teleport_t *RESTRICT in, location_t *RESTRICT out)
    {
        // FIXME: NOT IMPLEMENTED YET.
        return kernel_encode_state_ready;
    }
    kernel_encode_state_t
    chameleon_encode_t::finish(teleport_t *RESTRICT in, location_t *RESTRICT out)
    {
        // FIXME: NOT IMPLEMENTED YET.
        return kernel_encode_state_ready;
    }

    //--- decode ---
    kernel_decode_state_t
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

    void
    chameleon_decode_t::read_signature(location_t *RESTRICT in)
    {
        DENSITY_MEMCPY(&signature, in->pointer, sizeof(signature));
        in->pointer += sizeof(signature);
        shift = 0;
        signatures_count++;
    }

    void
    chameleon_decode_t::kernel(location_t *RESTRICT in, location_t *RESTRICT out,
                               const bool compressed)
    {
        if (compressed) {
            uint16_t hash;
            DENSITY_MEMCPY(&hash, in->pointer, sizeof(hash));
            process_compressed(hash, out);
            in->pointer += sizeof(hash);
        } else {
            uint32_t chunk;
            DENSITY_MEMCPY(&chunk, in->pointer, sizeof(chunk));
            process_uncompressed(chunk, out);
            in->pointer += sizeof(chunk);
        }
        out->pointer += sizeof(uint32_t);
    }

    void
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
        // FIXME: NOT IMPLEMENTED YET.
        return kernel_decode_state_ready;
    }
    kernel_decode_state_t
    chameleon_decode_t::finish(teleport_t *RESTRICT in, location_t *RESTRICT out)
    {
        // FIXME: NOT IMPLEMENTED YET.
        return kernel_decode_state_ready;
    }
}
