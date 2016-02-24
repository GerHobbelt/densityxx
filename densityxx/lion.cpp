// see LICENSE.md for license.
#include "densityxx/lion.hpp"
#include "densityxx/mathmacros.hpp"

namespace density {
    const unsigned lion_preferred_block_chunks_shift = 17;
    const uint_fast64_t lion_preferred_block_chunks =
        1 << lion_preferred_block_chunks_shift;

    const unsigned lion_preferred_efficiency_check_chunks_shift = 13;
    const uint_fast64_t lion_preferred_efficiency_check_chunks =
             1 << lion_preferred_efficiency_check_chunks_shift;

    //--- form ---
    // Unary codes (reversed) except the last one
    static const lion_entropy_code_t lion_form_entropy_codes[lion_number_of_forms] = {
        {DENSITY_BINARY_TO_UINT(1), 1},
        {DENSITY_BINARY_TO_UINT(10), 2},
        {DENSITY_BINARY_TO_UINT(100), 3},
        {DENSITY_BINARY_TO_UINT(1000), 4},
        {DENSITY_BINARY_TO_UINT(10000), 5},
        {DENSITY_BINARY_TO_UINT(100000), 6},
        {DENSITY_BINARY_TO_UINT(1000000), 7},
        {DENSITY_BINARY_TO_UINT(0000000), 7} };

    static const lion_form_t init_forms[] = {
        lion_form_plain, lion_form_dictionary_a, lion_form_dictionary_b,
        lion_form_predictions_a, lion_form_predictions_b,
        lion_form_dictionary_c, lion_form_predictions_c, lion_form_dictionary_d };
    static const uint8_t sz_init_forms = sizeof(init_forms) / sizeof(init_forms[0]);

    inline void
    lion_form_data_t::init(void)
    {
        lion_form_node_t *cur, *prev = NULL;
        for (uint8_t idx = 0; idx < sz_init_forms; ++idx) {
            cur = &forms_pool[idx];
            cur->form = init_forms[idx];
            cur->rank = idx;
            cur->previous_form = prev;
            forms_index[cur->form] = cur;
            prev = cur;
        }
        usages = 0;
    }

    inline void
    lion_form_data_t::update(lion_form_node_t *RESTRICT const form, const uint8_t usage,
                             lion_form_node_t *RESTRICT const previous_form,
                             const uint8_t previous_usage)
    {
        if (DENSITY_UNLIKELY(previous_usage < usage)) {    // Relative stability is assumed
            const lion_form_t form_value = form->form;
            const lion_form_t previous_form_value = previous_form->form;
            previous_form->form = form_value;
            form->form = previous_form_value;
            forms_index[form_value] = previous_form;
            forms_index[previous_form_value] = form;
        }
    }

    inline void
    lion_form_data_t::flatten(const uint8_t usage)
    {
        if (DENSITY_UNLIKELY(usage & 0x80)) // Flatten usage values
            usages = (usages >> 1) & 0x7f7f7f7f7f7f7f7fllu;
    }

    const lion_form_t
    lion_form_data_t::increment_usage(lion_form_node_t *RESTRICT const form)
    {
        const lion_form_t form_value = form->form;
        uint8_t *u8usages = (uint8_t *)&usages;
        const uint8_t usage = ++u8usages[form_value];
        lion_form_node_t *const previous_form = form->previous_form;
        if (previous_form) update(form, usage, previous_form, u8usages[previous_form->form]);
        else flatten(usage);
        return form_value;
    }

    inline lion_entropy_code_t
    lion_form_data_t::get_encoding(const lion_form_t form)
    {
        uint8_t *u8usages = (uint8_t *)&usages;
        const uint8_t usage = ++u8usages[form];
        lion_form_node_t *const form_found = forms_index[form];
        lion_form_node_t *const previous_form = form_found->previous_form;
        if (previous_form) {
            update(form_found, usage, previous_form, u8usages[previous_form->form]);
            return lion_form_entropy_codes[form_found->rank];
        } else {
            flatten(usage);
            return lion_form_entropy_codes[0];
        }
    }

    //--- encode ---
    inline void
    lion_encode_t::prepare_new_signature(location_t *RESTRICT out)
    {
        signature = (lion_signature_t *) (out->pointer);
        proximity_signature = 0;
        out->pointer += sizeof(lion_signature_t);
    }

    inline kernel_encode_state_t
    lion_encode_t::check_block_state(void)
    {
        if (DENSITY_LIKELY((chunks_count & (lion_chunks_per_process_unit_big - 1))))
            return kernel_encode_state_ready;
        if (DENSITY_UNLIKELY((chunks_count >= lion_preferred_efficiency_check_chunks)
                             && !efficiency_checked)) {
            efficiency_checked = true;
            return kernel_encode_state_info_efficiency_check;
        }
        if (DENSITY_UNLIKELY(chunks_count >= lion_preferred_block_chunks)) {
            chunks_count = 0;
            efficiency_checked = false;
#if DENSITY_ENABLE_PARALLELIZABLE_DECOMPRESSIBLE_OUTPUT == DENSITY_YES
            if (reset_cycle) --reset_cycle;
            else {
                dictionary.reset();
                reset_cycle = dictionary_preferred_reset_cycle - 1;
            }
#endif
            return kernel_encode_state_info_new_block;
        }
        return kernel_encode_state_ready;
    }

    inline void
    lion_encode_t::push_to_proximity_signature(const uint64_t content, const uint_fast8_t bits)
    {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        proximity_signature |= (content << shift);
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        proximity_signature |= (content << ((56 - (shift & ~0x7)) + (shift & 0x7)));
#else
#error Unknow endianness
#endif
        shift += bits;
    }

    inline void
    lion_encode_t::push_to_signature(location_t *RESTRICT out, const uint64_t content,
                                     const uint_fast8_t bits)
    {
        if (DENSITY_LIKELY(shift)) {
            push_to_proximity_signature(content, bits);
            if (DENSITY_UNLIKELY(shift >= DENSITY_BITSIZEOF(lion_signature_t))) {
                DENSITY_MEMCPY(signature, &proximity_signature, sizeof(proximity_signature));
                const uint_fast8_t remainder = (uint_fast8_t)(shift & 0x3f);
                shift = 0;
                if (remainder) {
                    prepare_new_signature(out);
                    push_to_proximity_signature(content >> (bits - remainder), remainder);
                }
            }
        } else {
            prepare_new_signature(out);
            push_to_proximity_signature(content, bits);
        }
    }
#if 0
    void
    lion_encode_t::push_zero_to_signature(location_t *RESTRICT out, const uint_fast8_t bits)
    {
        if (DENSITY_LIKELY(shift)) {
            shift += bits;
            if (DENSITY_UNLIKELY(shift >= DENSITY_BITSIZEOF(proximity_signature))) {
                DENSITY_MEMCPY(signature, &proximity_signature, sizeof(proximity_signature));
                const uint_fast8_t remainder = (uint_fast8_t)(shift & 0x3f);
                if (remainder) {
                    prepare_new_signature(out);
                    shift = remainder;
                } else
                    shift = 0;
            }
        } else {
            prepare_new_signature(out);
            shift = bits;
        }
    }
#endif
#define DENSITY_LION_KERNEL_PUSH_SAVE(LION_FORM, VAR)                   \
        push_code_to_signature(out, form_data.get_encoding(LION_FORM)); \
        DENSITY_MEMCPY(out->pointer, &VAR, sizeof(VAR));                \
        out->pointer += sizeof(VAR)
    inline void
    lion_encode_t::kernel(location_t *RESTRICT out, const uint16_t hash, const uint32_t chunk)
    {
        lion_dictionary_t *const dictionary = &this->dictionary;
        lion_dictionary_chunk_prediction_entry_t *const predictions =
            &dictionary->predictions[last_hash];
        __builtin_prefetch(&dictionary->predictions[hash]);
        if (*(uint32_t *) predictions != chunk) {
            if (*((uint32_t *) predictions + 1) != chunk) {
                if (*((uint32_t *) predictions + 2) != chunk) {
                    lion_dictionary_chunk_entry_t *const in_dictionary =
                        &dictionary->chunks[hash];
                    if (*(uint32_t *) in_dictionary != chunk) {
                        if (*((uint32_t *) in_dictionary + 1) != chunk) {
                            if (*((uint32_t *) in_dictionary + 2) != chunk) {
                                if (*((uint32_t *) in_dictionary + 3) != chunk) {
                                    DENSITY_LION_KERNEL_PUSH_SAVE(lion_form_plain, chunk);
                                } else {
                                    DENSITY_LION_KERNEL_PUSH_SAVE(lion_form_dictionary_d, hash);
                                }
                            } else {
                                DENSITY_LION_KERNEL_PUSH_SAVE(lion_form_dictionary_c, hash);
                            }
                        } else {
                            DENSITY_LION_KERNEL_PUSH_SAVE(lion_form_dictionary_b, hash);
                        }
                        DENSITY_MEMMOVE((uint32_t*)in_dictionary + 1, in_dictionary,
                                        3 * sizeof(uint32_t));
                        *(uint32_t *)in_dictionary = chunk;
                    } else {
                        DENSITY_LION_KERNEL_PUSH_SAVE(lion_form_dictionary_a, hash);
                    }
                } else {
                    push_code_to_signature(out, form_data.get_encoding(lion_form_predictions_c));
                }
            } else {
                push_code_to_signature(out, form_data.get_encoding(lion_form_predictions_b));
            }
            DENSITY_MEMMOVE((uint32_t*)predictions + 1, predictions, 2 * sizeof(uint32_t));
            *(uint32_t *)predictions = chunk;
        } else
            push_code_to_signature(out, form_data.get_encoding(lion_form_predictions_a));
        last_hash = hash;
    }

    inline void
    lion_encode_t::process_unit_generic(const uint_fast8_t chunks_per_process_unit,
                                        const uint_fast16_t process_unit_size,
                                        location_t *RESTRICT in, location_t *RESTRICT out)
    {
        uint32_t chunk;
#ifdef __clang__
        for (uint_fast8_t count = 0; count < (chunks_per_process_unit >> 2); count++) {
            DENSITY_UNROLL_4(DENSITY_MEMCPY(&chunk, in->pointer, sizeof(uint32_t)); \
                             kernel(out, hash_algorithm(chunk), chunk); \
                             in->pointer += sizeof(uint32_t));
        }
#else
        for (uint_fast8_t count = 0; count < (chunks_per_process_unit >> 1); count++) {
            DENSITY_UNROLL_2(DENSITY_MEMCPY(&chunk, in->pointer, sizeof(uint32_t)); \
                             kernel(out, hash_algorithm(chunk), chunk); \
                             in->pointer += sizeof(uint32_t));
        }
#endif
        chunks_count += chunks_per_process_unit;
        in->available_bytes -= process_unit_size;
    }

    inline void
    lion_encode_t::process_step_unit(location_t *RESTRICT in, location_t *RESTRICT out)
    {
        uint32_t chunk;
        DENSITY_MEMCPY(&chunk, in->pointer, sizeof(chunk));
        kernel(out, hash_algorithm(LITTLE_ENDIAN_32(chunk)), chunk);
        chunks_count++;
        in->pointer += sizeof(chunk);
        in->available_bytes -= sizeof(chunk);
    }

    kernel_encode_state_t
    lion_encode_t::init(void)
    {
        chunks_count = 0;
        efficiency_checked = false;
        signature = NULL;
        shift = 0;
        dictionary.reset();
#if DENSITY_ENABLE_PARALLELIZABLE_DECOMPRESSIBLE_OUTPUT == DENSITY_YES
        reset_cycle = dictionary_preferred_reset_cycle - 1;
#endif
        form_data.init();
        last_hash = 0;
        last_chunk = 0;
        signature_intercept_mode = false;
        end_marker = false;
        return exit_process(lion_encode_process_check_block_state, kernel_encode_state_ready);
    }

#ifndef DENSITY_LION_ENCODE_MANAGE_INTERCEPT
#define DENSITY_LION_ENCODE_MANAGE_INTERCEPT                            \
    if(start_shift > shift) {                                           \
        if(DENSITY_LIKELY(shift)) {                                     \
            const uint8_t *content_start = (uint8_t *)signature + sizeof(lion_signature_t); \
            this->transient_content.size = (uint8_t)(out->pointer - content_start); \
            DENSITY_MEMCPY(transient_content.content, content_start, transient_content.size); \
            out->pointer = (uint8_t *)signature;                        \
            out->available_bytes -= (out->pointer - pointer_out_before); \
            return exit_process(lion_encode_process_check_output_size,  \
                                kernel_encode_state_stall_on_output);   \
        } else {                                                        \
            out->available_bytes -= (out->pointer - pointer_out_before); \
            return exit_process(lion_encode_process_check_block_state,  \
                                kernel_encode_state_stall_on_output);   \
        }                                                               \
    }
#endif

    kernel_encode_state_t
    lion_encode_t::continue_(teleport_t *RESTRICT in, location_t *RESTRICT out)
    {
        kernel_encode_state_t return_state;
        uint8_t *pointer_out_before;
        location_t *read_memory_location;
        // Dispatch
        switch (process) {
        case lion_encode_process_check_block_state: goto check_block_state;
        case lion_encode_process_check_output_size: goto check_output_size;
        case lion_encode_process_unit: goto process_unit;
        default: return kernel_encode_state_error;
        }
        // Check block metadata
    check_block_state:
        if (DENSITY_UNLIKELY(!shift)) {
            if(DENSITY_UNLIKELY(out->available_bytes < lion_encode_minimum_lookahead))
                // Direct exit possible, if coming from copy mode
                return exit_process(lion_encode_process_check_block_state,
                                    kernel_encode_state_stall_on_output);
            if (DENSITY_UNLIKELY(return_state = check_block_state()))
                return exit_process(lion_encode_process_check_block_state, return_state);
        }
        // Check output size
    check_output_size:
        if (DENSITY_UNLIKELY(signature_intercept_mode)) {
            if (out->available_bytes >= lion_encode_minimum_lookahead) {
                // New buffer
                if(DENSITY_LIKELY(shift)) {
                    signature = (lion_signature_t *) (out->pointer);
                    out->pointer += sizeof(lion_signature_t);
                    DENSITY_MEMCPY(out->pointer, transient_content.content,
                                   transient_content.size);
                    out->pointer += transient_content.size;
                    out->available_bytes -= sizeof(lion_signature_t) + transient_content.size;
                }
                signature_intercept_mode = false;
            }
        } else {
            if (DENSITY_UNLIKELY(out->available_bytes < lion_encode_minimum_lookahead))
                signature_intercept_mode = true;
        }
        // Try to read a complete process unit
    process_unit:
        pointer_out_before = out->pointer;
        if (!(read_memory_location = in->read(lion_process_unit_size_big)))
            return exit_process(lion_encode_process_unit, kernel_encode_state_stall_on_input);
        // Chunk was read properly, process
        if(DENSITY_UNLIKELY(signature_intercept_mode)) {
            const uint_fast32_t start_shift = shift;
            process_unit_small(read_memory_location, out);
            DENSITY_LION_ENCODE_MANAGE_INTERCEPT;
        } else {
            if(DENSITY_UNLIKELY(chunks_count & (lion_chunks_per_process_unit_big - 1)))
                // Attempt to resync the chunks count with a multiple of
                // lion_chunks_per_process_unit_big
                process_unit_small(read_memory_location, out);
            else
                process_unit_big(read_memory_location, out);
        }
        out->available_bytes -= (out->pointer - pointer_out_before);
        // New loop
        goto check_block_state;
    }
    kernel_encode_state_t
    lion_encode_t::finish(teleport_t *RESTRICT in, location_t *RESTRICT out)
    {
        kernel_encode_state_t return_state;
        uint8_t *pointer_out_before;
        location_t *read_memory_location;
        // Dispatch
        switch (process) {
        case lion_encode_process_check_block_state: goto check_block_state;
        case lion_encode_process_check_output_size: goto check_output_size;
        case lion_encode_process_unit: goto process_unit;
        default: return kernel_encode_state_error;
        }
        // Check block metadata
    check_block_state:
        if (DENSITY_UNLIKELY(!shift)) {
            if(DENSITY_UNLIKELY(out->available_bytes < lion_encode_minimum_lookahead))
                // Direct exit possible, if coming from copy mode
                return exit_process(lion_encode_process_check_block_state,
                                    kernel_encode_state_stall_on_output);
            if (DENSITY_UNLIKELY(return_state = check_block_state()))
                return exit_process(lion_encode_process_check_block_state, return_state);
        }
        // Check output size
    check_output_size:
        if(DENSITY_UNLIKELY(signature_intercept_mode)) {
            if (out->available_bytes >= lion_encode_minimum_lookahead) {
                // New buffer
                if(DENSITY_LIKELY(shift)) {
                    signature = (lion_signature_t *) (out->pointer);
                    out->pointer += sizeof(lion_signature_t);
                    DENSITY_MEMCPY(out->pointer, transient_content.content,
                                   transient_content.size);
                    out->pointer += transient_content.size;
                    out->available_bytes -= sizeof(lion_signature_t) + transient_content.size;
                }
                signature_intercept_mode = false;
            }
        } else {
            if (DENSITY_UNLIKELY(out->available_bytes < lion_encode_minimum_lookahead))
                signature_intercept_mode = true;
        }
        // Try to read a complete process unit
    process_unit:
        pointer_out_before = out->pointer;
        if (!(read_memory_location = in->read(lion_process_unit_size_big)))
            goto step_by_step;
        // Chunk was read properly, process
        if(DENSITY_UNLIKELY(signature_intercept_mode)) {
            const uint_fast32_t start_shift = shift;
            process_unit_small(read_memory_location, out);
            DENSITY_LION_ENCODE_MANAGE_INTERCEPT;
        } else {
            if(DENSITY_UNLIKELY(chunks_count & (lion_chunks_per_process_unit_big - 1)))
                // Attempt to resync the chunks count with a multiple of
                // lion_chunks_per_process_unit_big
                process_unit_small(read_memory_location, out);
            else
                process_unit_big(read_memory_location, out);
        }
        goto exit;
        // Read step by step
    step_by_step:
        while ((read_memory_location = in->read(sizeof(uint32_t)))) {
            if(DENSITY_UNLIKELY(signature_intercept_mode)) {
                const uint_fast32_t start_shift = shift;
                process_step_unit(read_memory_location, out);
                DENSITY_LION_ENCODE_MANAGE_INTERCEPT;
            } else
                process_step_unit(read_memory_location, out);
            if(!shift) goto exit;
        }
    exit:
        out->available_bytes -= (out->pointer - pointer_out_before);
        if (in->available_bytes() > sizeof(uint32_t)) goto check_block_state;
        // Marker for decode loop exit
        if(!end_marker) {
            end_marker = true;
            goto check_block_state;
        }
        pointer_out_before = out->pointer;
        const lion_entropy_code_t code = form_data.get_encoding(lion_form_plain);
        if(DENSITY_UNLIKELY(signature_intercept_mode)) {
            const uint_fast32_t start_shift = shift;
            push_code_to_signature(out, code);
            DENSITY_LION_ENCODE_MANAGE_INTERCEPT;
        } else
            push_code_to_signature(out, code);
        // Copy the remaining bytes
        DENSITY_MEMCPY(signature, &proximity_signature, sizeof(proximity_signature));
        out->available_bytes -= (out->pointer - pointer_out_before);
        in->copy_remaining(out);
        return kernel_encode_state_ready;
    }

    //--- decode ---
#if 0 // USELESS?
#define DENSITY_LION_DECODE_NUMBER_OF_BITMASK_VALUES 8
static const uint8_t lion_decode_bitmasks[DENSITY_LION_DECODE_NUMBER_OF_BITMASK_VALUES] = {
    DENSITY_BINARY_TO_UINT(0),
    DENSITY_BINARY_TO_UINT(1),
    DENSITY_BINARY_TO_UINT(11),
    DENSITY_BINARY_TO_UINT(111),
    DENSITY_BINARY_TO_UINT(1111),
    DENSITY_BINARY_TO_UINT(11111),
    DENSITY_BINARY_TO_UINT(111111),
    DENSITY_BINARY_TO_UINT(1111111)
};
#endif
    inline kernel_decode_state_t
    lion_decode_t::check_block_state(void)
    {
        if (DENSITY_UNLIKELY((chunks_count >= lion_preferred_efficiency_check_chunks)
                             && (!efficiency_checked))) {
            efficiency_checked = true;
            return kernel_decode_state_info_efficiency_check;
        } else if (DENSITY_UNLIKELY(chunks_count >= lion_preferred_block_chunks)) {
            chunks_count = 0;
            efficiency_checked = false;
#if DENSITY_ENABLE_PARALLELIZABLE_DECOMPRESSIBLE_OUTPUT == DENSITY_YES
            if (reset_cycle) --reset_cycle;
            else {
                dictionary.reset();
                reset_cycle = dictionary_preferred_reset_cycle - 1;
            }
#endif
            return kernel_decode_state_info_new_block;
        }
        return kernel_decode_state_ready;
    }
    inline void
    lion_decode_t::prediction_a(location_t *RESTRICT in, location_t *RESTRICT out,
                                uint16_t *RESTRICT const hash, uint32_t *RESTRICT const chunk)
    {
        *chunk = dictionary.predictions[last_hash].next_chunk_a;
        prediction_generic(out, hash, chunk);
        last_chunk = *chunk;
        last_hash = *hash;
    }
    inline void
    lion_decode_t::prediction_b(location_t *RESTRICT in, location_t *RESTRICT out,
                                uint16_t *RESTRICT const hash, uint32_t *RESTRICT const chunk)
    {
        lion_dictionary_chunk_prediction_entry_t *const p = &dictionary.predictions[last_hash];
        *chunk = p->next_chunk_b;
        update_predictions_model(p, *chunk);
        prediction_generic(out, hash, chunk);
        last_chunk = *chunk;
        last_hash = *hash;
    }
    inline void
    lion_decode_t::prediction_c(location_t *RESTRICT in, location_t *RESTRICT out,
                                uint16_t *RESTRICT const hash, uint32_t *RESTRICT const chunk)
    {
        lion_dictionary_chunk_prediction_entry_t *const p = &dictionary.predictions[last_hash];
        *chunk = p->next_chunk_c;
        update_predictions_model(p, *chunk);
        prediction_generic(out, hash, chunk);
        last_chunk = *chunk;
        last_hash = *hash;
    }
    inline void
    lion_decode_t::dictionary_a(location_t *RESTRICT in, location_t *RESTRICT out,
                                uint16_t *RESTRICT const hash, uint32_t *RESTRICT const chunk)
    {
        read_hash(in, hash);
        __builtin_prefetch(&dictionary.predictions[*hash]);
        *chunk = dictionary.chunks[*hash].chunk_a;
        dictionary_generic(in, out, hash, chunk);
        last_chunk = *chunk;
        last_hash = *hash;
    }
    inline void
    lion_decode_t::dictionary_b(location_t *RESTRICT in, location_t *RESTRICT out,
                                uint16_t *RESTRICT const hash, uint32_t *RESTRICT const chunk)
    {
        read_hash(in, hash);
        __builtin_prefetch(&dictionary.predictions[*hash]);
        lion_dictionary_chunk_entry_t *entry = &dictionary.chunks[*hash];
        *chunk = entry->chunk_b;
        update_dictionary_model(entry, *chunk);
        dictionary_generic(in, out, hash, chunk);
        last_chunk = *chunk;
        last_hash = *hash;
    }
    inline void
    lion_decode_t::dictionary_c(location_t *RESTRICT in, location_t *RESTRICT out,
                                uint16_t *RESTRICT const hash, uint32_t *RESTRICT const chunk)
    {
        read_hash(in, hash);
        __builtin_prefetch(&dictionary.predictions[*hash]);
        lion_dictionary_chunk_entry_t *entry = &dictionary.chunks[*hash];
        *chunk = entry->chunk_c;
        update_dictionary_model(entry, *chunk);
        dictionary_generic(in, out, hash, chunk);
        last_chunk = *chunk;
        last_hash = *hash;
    }
    inline void
    lion_decode_t::dictionary_d(location_t *RESTRICT in, location_t *RESTRICT out,
                                uint16_t *RESTRICT const hash, uint32_t *RESTRICT const chunk)
    {
        read_hash(in, hash);
        __builtin_prefetch(&dictionary.predictions[*hash]);
        lion_dictionary_chunk_entry_t *entry = &dictionary.chunks[*hash];
        *chunk = entry->chunk_d;
        update_dictionary_model(entry, *chunk);
        dictionary_generic(in, out, hash, chunk);
        last_chunk = *chunk;
        last_hash = *hash;
    }
    inline void
    lion_decode_t::plain(location_t *RESTRICT in, location_t *RESTRICT out,
                         uint16_t *RESTRICT const hash, uint32_t *RESTRICT const chunk)
    {
        DENSITY_MEMCPY(chunk, in->pointer, sizeof(*chunk));
        in->pointer += sizeof(*chunk);
        *hash = hash_algorithm(*chunk);
        lion_dictionary_chunk_entry_t *entry = &dictionary.chunks[*hash];
        update_dictionary_model(entry, *chunk);
        DENSITY_MEMCPY(out->pointer, chunk, sizeof(*chunk));
        out->pointer += sizeof(*chunk);
        lion_dictionary_chunk_prediction_entry_t *p = &(dictionary.predictions[last_hash]);
        update_predictions_model(p, *chunk);
        last_chunk = *chunk;
        last_hash = *hash;
    }
    inline void
    lion_decode_t::chunk(location_t *RESTRICT in, location_t *RESTRICT out,
                         const lion_form_t form)
    {
        uint16_t hash; uint32_t chunk;
        switch (form) {
        case lion_form_predictions_a: prediction_a(in, out, &hash, &chunk); break;
        case lion_form_predictions_b: prediction_b(in, out, &hash, &chunk); break;
        case lion_form_predictions_c: prediction_c(in, out, &hash, &chunk); break;
        case lion_form_dictionary_a: dictionary_a(in, out, &hash, &chunk); break;
        case lion_form_dictionary_b: dictionary_b(in, out, &hash, &chunk); break;
        case lion_form_dictionary_c: dictionary_c(in, out, &hash, &chunk); break;
        case lion_form_dictionary_d: dictionary_d(in, out, &hash, &chunk); break;
        case lion_form_plain: plain(in, out, &hash, &chunk); break;
        default: break;
        }
    }
    inline const lion_form_t
    lion_decode_t::read_form(location_t *RESTRICT in)
    {
        const uint_fast8_t shift = this->shift;
        lion_form_node_t *forms_pool = form_data.forms_pool;
        const uint_fast8_t trailing_zeroes = __builtin_ctz(0x80 | (signature >> shift));
        if (DENSITY_LIKELY(!trailing_zeroes)) {
            this->shift = (shift + 1) & 0x3f;
            return form_data.increment_usage(forms_pool);
        } else if (DENSITY_LIKELY(trailing_zeroes <= 6)) {
            this->shift = (shift + (trailing_zeroes + 1)) & 0x3f;
            return form_data.increment_usage(forms_pool + trailing_zeroes);
        } else if (DENSITY_LIKELY(shift <= (DENSITY_BITSIZEOF(lion_signature_t) - 7))) {
            this->shift = (shift + 7) & 0x3f;
            return form_data.increment_usage(forms_pool + 7);
        } else {
            read_signature_from_memory(in);
            const uint_fast8_t primary_trailing_zeroes =
                DENSITY_BITSIZEOF(lion_signature_t) - shift;
            const uint_fast8_t ctz_barrier_shift = 7 - primary_trailing_zeroes;
            const uint_fast8_t secondary_trailing_zeroes =
                __builtin_ctz((1 << ctz_barrier_shift) | this->signature);
            if (DENSITY_LIKELY(secondary_trailing_zeroes != ctz_barrier_shift))
                this->shift = secondary_trailing_zeroes + 1;
            else
                this->shift = secondary_trailing_zeroes;
            return form_data.increment_usage(forms_pool + primary_trailing_zeroes +
                                             secondary_trailing_zeroes);
        }
    }
    inline void
    lion_decode_t::process_form(location_t *RESTRICT in, location_t *RESTRICT out)
    {
        const uint_fast8_t shift = this->shift;
        if (DENSITY_UNLIKELY(!shift))  read_signature_from_memory(in);
        switch ((signature >> shift) & 0x1) {
        case 0: chunk(in, out, read_form(in)); break;
        default:
            chunk(in, out, form_data.increment_usage((lion_form_node_t *)form_data.forms_pool));
            this->shift = (shift + 1) & 0x3f;
            break;
        }
    }
    inline void
    lion_decode_t::process_unit(location_t *RESTRICT in, location_t *RESTRICT out)
    {
#ifdef __clang__
        for (uint_fast8_t count = 0; count < (lion_chunks_per_process_unit_big >> 2); count++) {
            DENSITY_UNROLL_4(process_form(in, out));
        }
#else
        for (uint_fast8_t count = 0; count < (lion_chunks_per_process_unit_big >> 2); count++) {
            DENSITY_UNROLL_4(process_form(in, out));
        }
#endif
        chunks_count += lion_chunks_per_process_unit_big;
    }
    inline lion_decode_step_by_step_status_t
    lion_decode_t::chunk_step_by_step(location_t *RESTRICT read_memory_location,
                                      teleport_t *RESTRICT in, location_t *RESTRICT out)
    {
        uint8_t *start_pointer = read_memory_location->pointer;
        if (DENSITY_UNLIKELY(!shift)) read_signature_from_memory(read_memory_location);
        lion_form_t form = read_form(read_memory_location);
        read_memory_location->available_bytes -=
            (read_memory_location->pointer - start_pointer);
        switch (form) {
        case lion_form_plain:  // Potential end marker
            if (DENSITY_UNLIKELY(in->available_bytes_reserved(end_data_overhead) <= sizeof(uint32_t)))
                return lion_decode_step_by_step_status_end_marker;
            break;
        default: break;
        }
        if (out->available_bytes < sizeof(uint32_t))
            return lion_decode_step_by_step_status_stall_on_output;
        start_pointer = read_memory_location->pointer;
        chunk(read_memory_location, out, form);
        ++chunks_count;
        read_memory_location->available_bytes -=
            (read_memory_location->pointer - start_pointer);
        return lion_decode_step_by_step_status_proceed;
    }

    kernel_decode_state_t
    lion_decode_t::init(const main_header_parameters_t parameters,
                        const uint_fast8_t end_data_overhead)
    {
        chunks_count = 0;
        efficiency_checked = false;
        shift = 0;
        dictionary.reset();
        this->parameters = parameters;
        uint8_t reset_dictionary_cycle_shift = parameters.as_bytes[0];
        if (reset_dictionary_cycle_shift)
            reset_cycle = ((uint_fast64_t) 1 << reset_dictionary_cycle_shift) - 1;
        this->end_data_overhead = end_data_overhead;
        form_data.init();
        last_hash = 0;
        last_chunk = 0;
        return exit_process(lion_decode_process_check_block_state, kernel_decode_state_ready);
    }

#define DENSITY_LION_DECODE_MAX_BITS_TO_READ_FOR_CHUNK \
    (DENSITY_BITSIZEOF(lion_signature_t) + 3 +         \
     2 * (3 + DENSITY_BITSIZEOF(uint16_t)))
// 8 bytes (new signature) + 3 bits (lowest rank form) + 2 * (3 bit flags (DENSITY_LION_FORM_SECONDARY_ACCESS + DENSITY_LION_BIGRAM_PRIMARY_SIGNATURE_FLAG_SECONDARY_ACCESS + DENSITY_LION_BIGRAM_SECONDARY_SIGNATURE_FLAG_PLAIN) + 2 bytes)
#define DENSITY_LION_DECODE_MAX_BYTES_TO_READ_FOR_PROCESS_UNIT \
    (1 + ((lion_chunks_per_process_unit_big * DENSITY_LION_DECODE_MAX_BITS_TO_READ_FOR_CHUNK) >> 3))
    kernel_decode_state_t
    lion_decode_t::continue_(teleport_t *RESTRICT in, location_t *RESTRICT out)
    {
        kernel_decode_state_t return_state;
        location_t *read_memory_location;
        uint8_t *read_memory_location_pointer_before;
        // Dispatch
        switch (process) {
        case lion_decode_process_check_block_state: goto check_block_state;
        case lion_decode_process_check_output_size: goto check_output_size;
        case lion_decode_process_unit: goto process_unit;
        default: return kernel_decode_state_error;
        }
        // Check block state
    check_block_state:
        if (DENSITY_UNLIKELY(!shift)) {
            if (DENSITY_UNLIKELY(return_state = check_block_state()))
                return exit_process(lion_decode_process_check_block_state, return_state);
        }
        // Check output size
    check_output_size:
        if (DENSITY_UNLIKELY(out->available_bytes < lion_process_unit_size_big))
            return exit_process(lion_decode_process_check_output_size,
                                kernel_decode_state_stall_on_output);
        // Try to read the next processing unit
    process_unit:
        read_memory_location =
            in->read_reserved(DENSITY_LION_DECODE_MAX_BYTES_TO_READ_FOR_PROCESS_UNIT,
                              end_data_overhead);
        if (DENSITY_UNLIKELY(!read_memory_location))
            return exit_process(lion_decode_process_unit, kernel_decode_state_stall_on_input);
        read_memory_location_pointer_before = read_memory_location->pointer;
        process_unit(read_memory_location, out);
        read_memory_location->available_bytes -=
            (read_memory_location->pointer - read_memory_location_pointer_before);
        out->available_bytes -= lion_process_unit_size_big;
        // New loop
        goto check_block_state;
    }
    kernel_decode_state_t
    lion_decode_t::finish(teleport_t *RESTRICT in, location_t *RESTRICT out)
    {
        kernel_decode_state_t return_state;
        location_t *read_memory_location;
        uint_fast64_t available_bytes_reserved;
        uint8_t *read_memory_location_pointer_before;
        // Dispatch
        switch (process) {
        case lion_decode_process_check_block_state: goto check_block_state;
        case lion_decode_process_check_output_size: goto check_output_size;
        case lion_decode_process_unit: goto process_unit;
        default: return kernel_decode_state_error;
        }
        // Check block state
    check_block_state:
        if (DENSITY_UNLIKELY(!shift)) {
            if (DENSITY_UNLIKELY(return_state = check_block_state()))
                return exit_process(lion_decode_process_check_block_state, return_state);
        }
        // Check output size
    check_output_size:
        if (DENSITY_UNLIKELY(out->available_bytes < lion_process_unit_size_big))
            return exit_process(lion_decode_process_check_output_size,
                                kernel_decode_state_stall_on_output);
        // Try to read the next processing unit
    process_unit:
        read_memory_location =
            in->read_reserved(DENSITY_LION_DECODE_MAX_BYTES_TO_READ_FOR_PROCESS_UNIT,
                              end_data_overhead);
        if (DENSITY_UNLIKELY(!read_memory_location))
            goto step_by_step;
        read_memory_location_pointer_before = read_memory_location->pointer;
        process_unit(read_memory_location, out);
        read_memory_location->available_bytes -=
            (read_memory_location->pointer - read_memory_location_pointer_before);
        out->available_bytes -= lion_process_unit_size_big;
        // New loop
        goto check_block_state;
        // Try to read and process units step by step
    step_by_step:
        read_memory_location = in->read_remaining_reserved(end_data_overhead);
        uint_fast8_t iterations = lion_chunks_per_process_unit_big;
        while (iterations --) {
            switch (chunk_step_by_step(read_memory_location, in, out)) {
            case lion_decode_step_by_step_status_proceed: break;
            case lion_decode_step_by_step_status_stall_on_output:
                return kernel_decode_state_error;
            case lion_decode_step_by_step_status_end_marker: goto finish;
            }
            out->available_bytes -= sizeof(uint32_t);
        }
        // New loop
        goto check_block_state;
    finish:
        available_bytes_reserved = in->available_bytes_reserved(end_data_overhead);
        if (out->available_bytes < available_bytes_reserved)
            return kernel_decode_state_error;
        in->copy(out, available_bytes_reserved);
        return kernel_decode_state_ready;
    }
}
