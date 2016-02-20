// see LICENSE.md for license.
#pragma once

#include "densityxx/kernel.hpp"

namespace density {
#define DENSITY_LION_CHUNK_HASH_BITS    16
#define DENSITY_LION_HASH32_MULTIPLIER  (uint32_t)0x9D6EF916lu
#define DENSITY_LION_HASH_ALGORITHM(value32) \
    (uint16_t)(value32 * DENSITY_LION_HASH32_MULTIPLIER >> (32 - DENSITY_LION_CHUNK_HASH_BITS))

#define DENSITY_LION_NUMBER_OF_FORMS    8

    typedef uint64_t lion_signature_t;
#define DENSITY_LION_MAXIMUM_COMPRESSED_BODY_SIZE_PER_SIGNATURE         \
    (DENSITY_BITSIZEOF(lion_signature_t) * sizeof(uint32_t))   // Plain writes
#define DENSITY_LION_MAXIMUM_COMPRESSED_UNIT_SIZE                       \
    (sizeof(lion_signature_t) + DENSITY_LION_MAXIMUM_COMPRESSED_BODY_SIZE_PER_SIGNATURE)

    // Cannot be higher than DENSITY_BITSIZEOF(lion_signature_t) / (max form length) or
    // signature interception won't work and has to be > 4 for unrolling
#define DENSITY_LION_CHUNKS_PER_PROCESS_UNIT_SMALL                      8
#define DENSITY_LION_CHUNKS_PER_PROCESS_UNIT_BIG                        64
#define DENSITY_LION_PROCESS_UNIT_SIZE_SMALL                            \
    (DENSITY_LION_CHUNKS_PER_PROCESS_UNIT_SMALL * sizeof(uint32_t))
#define DENSITY_LION_PROCESS_UNIT_SIZE_BIG                              \
    (DENSITY_LION_CHUNKS_PER_PROCESS_UNIT_BIG * sizeof(uint32_t))

#pragma pack(push)
#pragma pack(4)
    //--- form ---
    typedef enum {
        lion_form_predictions_a = 0,
        lion_form_predictions_b,
        lion_form_predictions_c,
        lion_form_dictionary_a,
        lion_form_dictionary_b,
        lion_form_dictionary_c,
        lion_form_dictionary_d,
        lion_form_plain,
    }  lion_form_t;
    typedef struct lion_form_node_s lion_form_node_t;
    struct lion_form_node_s {
        lion_form_node_t *previous_form;
        lion_form_t form;
        uint8_t rank;
    };
    typedef struct {
        uint_fast8_t value;
        uint_fast8_t bit_length;
    } lion_entropy_code_t;
    class lion_form_data_t {
    public:
        void init(void);
        void update(lion_form_node_t *RESTRICT const, const uint8_t,
                    lion_form_node_t *RESTRICT const, const uint8_t);
        void flatten(const uint8_t);
        const lion_form_t increment_usage(lion_form_node_t *RESTRICT const);
        lion_entropy_code_t get_encoding(const lion_form_t);

        // data members.
        uint8_t usages[DENSITY_LION_NUMBER_OF_FORMS];
        void (*attachments[DENSITY_LION_NUMBER_OF_FORMS])
        (location_t *, location_t *, uint16_t *const, uint32_t *const);
        lion_form_node_t forms_pool[DENSITY_LION_NUMBER_OF_FORMS];
        lion_form_node_t *forms_index[DENSITY_LION_NUMBER_OF_FORMS];
        uint8_t next_available_form;
    };

    typedef enum {
        lion_bigram_signature_flag_dictionary = 0x0,
        lion_bigram_signature_flag_plain = 0x1,
    } lion_bigram_signature_flag_t;
    typedef enum {
        lion_predictions_signature_flag_a = 0x0,
        lion_predictions_signature_flag_b = 0x1,
    } lion_predictions_signature_flag_t;

    //-- dictionary ---
    typedef struct {
        uint16_t bigram;
    } lion_dictionary_bigram_entry_t;
    typedef struct {
        uint32_t chunk_a;
        uint32_t chunk_b;
        uint32_t chunk_c;
        uint32_t chunk_d;
        uint32_t chunk_e;
    } lion_dictionary_chunk_entry_t;
    typedef struct {
        uint32_t next_chunk_a;
        uint32_t next_chunk_b;
        uint32_t next_chunk_c;
    } lion_dictionary_chunk_prediction_entry_t;
    class lion_dictionary_t {
    public:
        lion_dictionary_bigram_entry_t bigrams[1 << DENSITY_BITSIZEOF(uint8_t)];
        lion_dictionary_chunk_entry_t chunks[1 << DENSITY_LION_CHUNK_HASH_BITS];
        lion_dictionary_chunk_prediction_entry_t predictions[1 << DENSITY_LION_CHUNK_HASH_BITS];
        inline void reset(void) { memset(this, 0, sizeof(*this)); }
    };

    //--- encode ---
    typedef enum {
        lion_encode_process_check_block_state,
        lion_encode_process_check_output_size,
        lion_encode_process_unit,
    } lion_encode_process_t;
    typedef struct {
        uint8_t content[DENSITY_LION_MAXIMUM_COMPRESSED_BODY_SIZE_PER_SIGNATURE];
        uint_fast8_t size;
    } lion_encode_content_t;

#define DENSITY_LION_ENCODE_MINIMUM_LOOKAHEAD           \
        (sizeof(block_footer_t) +                       \
         sizeof(block_header_t) +                       \
         sizeof(block_mode_marker_t) +                  \
     (DENSITY_LION_MAXIMUM_COMPRESSED_UNIT_SIZE << 1))
    // On a normal cycle, DENSITY_LION_CHUNKS_PER_PROCESS_UNIT = 64 chunks = 256 bytes can be
    // compressed at once, before being in intercept mode where another 256 input bytes could be
    // processed before ending the signature

    class lion_encode_t: public kernel_encode_t {
    public:
        virtual compression_mode_t mode(void) const
        {   return compression_mode_lion_algorithm; }

        virtual kernel_encode_state_t init(void);
        virtual kernel_encode_state_t
        continue_(teleport_t *RESTRICT in, location_t *RESTRICT out);
        virtual kernel_encode_state_t
        finish(teleport_t *RESTRICT in, location_t *RESTRICT out);

        lion_signature_t proximity_signature;
        lion_form_data_t form_data;
        uint_fast16_t last_hash;
        uint32_t last_chunk;
        uint_fast8_t shift;
        lion_signature_t *signature;
        uint_fast64_t chunks_count;
        bool efficiency_checked;
        lion_encode_process_t process;
        lion_encode_content_t transient_content;
        bool signature_intercept_mode;
        bool end_marker;
        lion_dictionary_t dictionary;
#if DENSITY_ENABLE_PARALLELIZABLE_DECOMPRESSIBLE_OUTPUT == DENSITY_YES
        uint_fast64_t reset_cycle;
#endif
    private:
        inline kernel_encode_state_t
        exit_process(lion_encode_process_t process, kernel_encode_state_t kernel_encode_state)
        {   this->process = process; return kernel_encode_state; }
        void prepare_new_signature(location_t *RESTRICT out);
        kernel_encode_state_t check_block_state(void);
        void push_to_proximity_signature(const uint64_t content, const uint_fast8_t bits);
        void push_to_signature(location_t *RESTRICT out, const uint64_t content,
                               const uint_fast8_t bits);
        void push_zero_to_signature(location_t *RESTRICT out, const uint_fast8_t bits);
        inline void
        push_code_to_signature(location_t *RESTRICT out, const lion_entropy_code_t code)
        {   push_to_signature(out, code.value, code.bit_length); }
        void kernel(location_t *RESTRICT out, const uint16_t hash, const uint32_t chunk);
        void process_unit_generic(const uint_fast8_t chunks_per_process_unit,
                                  const uint_fast16_t process_unit_size,
                                  location_t *RESTRICT in, location_t *RESTRICT out);
        inline void process_unit_small(location_t *RESTRICT in, location_t *RESTRICT out)
        {   process_unit_generic(DENSITY_LION_CHUNKS_PER_PROCESS_UNIT_SMALL,
                                 DENSITY_LION_PROCESS_UNIT_SIZE_SMALL, in, out); }
        inline void
        process_unit_big(location_t *RESTRICT in, location_t *RESTRICT out)
        {   process_unit_generic(DENSITY_LION_CHUNKS_PER_PROCESS_UNIT_BIG,
                                 DENSITY_LION_PROCESS_UNIT_SIZE_BIG, in, out); }
        void process_step_unit(location_t *RESTRICT in, location_t *RESTRICT out);
    };

    //--- decode ---
    typedef enum {
        lion_decode_process_check_block_state,
        lion_decode_process_check_output_size,
        lion_decode_process_unit,
    } lion_decode_process_t;
    typedef enum {
        lion_decode_step_by_step_status_proceed = 0,
        lion_decode_step_by_step_status_stall_on_output,
        lion_decode_step_by_step_status_end_marker
    } lion_decode_step_by_step_status_t;
    class lion_decode_t: public kernel_decode_t {
    public:
        virtual compression_mode_t mode(void) const
        {   return compression_mode_lion_algorithm; }

        kernel_decode_state_t
        init(const main_header_parameters_t parameters, const uint_fast8_t end_data_overhead);
        virtual kernel_decode_state_t
        continue_(teleport_t *RESTRICT in, location_t *RESTRICT out);
        virtual kernel_decode_state_t
        finish(teleport_t *RESTRICT in, location_t *RESTRICT out);

        lion_signature_t signature;
        lion_form_data_t form_data;
        uint_fast16_t last_hash;
        uint32_t last_chunk;
        uint_fast8_t shift;
        uint_fast64_t chunks_count;
        bool efficiency_checked;
        lion_decode_process_t process;
        uint_fast8_t end_data_overhead;
        main_header_parameters_t parameters;
        lion_dictionary_t dictionary;
        uint_fast64_t reset_cycle;
    private:
        inline kernel_decode_state_t
        exit_process(lion_decode_process_t process, kernel_decode_state_t kernel_decode_state)
        {   this->process = process; return kernel_decode_state; }
        kernel_decode_state_t check_block_state(void);
        inline void read_signature_from_memory(location_t *RESTRICT in)
        {   DENSITY_MEMCPY(&signature, in->pointer, sizeof(signature));
            in->pointer += sizeof(signature); }
        inline void
        update_predictions_model(lion_dictionary_chunk_prediction_entry_t *const RESTRICT predictions,
                                 const uint32_t chunk)
        {   DENSITY_MEMMOVE((uint32_t *) predictions + 1, predictions, 2 * sizeof(uint32_t));
            // Move chunk to the top of the predictions list
            *(uint32_t *) predictions = chunk; }
        inline void
        update_dictionary_model(lion_dictionary_chunk_entry_t *const RESTRICT entry,
                                const uint32_t chunk)
        {   DENSITY_MEMMOVE((uint32_t *) entry + 1, entry, 3 * sizeof(uint32_t));
            *(uint32_t *) entry = chunk; }
        inline void
        read_hash(location_t *RESTRICT in, uint16_t *RESTRICT const hash)
        {   DENSITY_MEMCPY(hash, in->pointer, sizeof(uint16_t));
            in->pointer += sizeof(uint16_t); }
        inline void
        prediction_generic(location_t *RESTRICT out, uint16_t *RESTRICT const hash,
                           uint32_t *RESTRICT const chunk)
        {   *hash = DENSITY_LION_HASH_ALGORITHM(*chunk);
            DENSITY_MEMCPY(out->pointer, chunk, sizeof(*chunk));
            out->pointer += sizeof(*chunk); }
        inline void
        dictionary_generic(location_t *RESTRICT in, location_t *RESTRICT out,
                           uint16_t *RESTRICT const hash, uint32_t *RESTRICT const chunk)
        {   DENSITY_MEMCPY(out->pointer, chunk, sizeof(*chunk));
            out->pointer += sizeof(*chunk);
            lion_dictionary_chunk_prediction_entry_t *p = &(dictionary.predictions[last_hash]);
            update_predictions_model(p, *chunk); }
        void prediction_a(location_t *RESTRICT in, location_t *RESTRICT out,
                          uint16_t *RESTRICT const hash, uint32_t *RESTRICT const chunk);
        void prediction_b(location_t *RESTRICT in, location_t *RESTRICT out,
                          uint16_t *RESTRICT const hash, uint32_t *RESTRICT const chunk);
        void prediction_c(location_t *RESTRICT in, location_t *RESTRICT out,
                          uint16_t *RESTRICT const hash, uint32_t *RESTRICT const chunk);
        void dictionary_a(location_t *RESTRICT in, location_t *RESTRICT out,
                          uint16_t *RESTRICT const hash, uint32_t *RESTRICT const chunk);
        void dictionary_b(location_t *RESTRICT in, location_t *RESTRICT out,
                          uint16_t *RESTRICT const hash, uint32_t *RESTRICT const chunk);
        void dictionary_c(location_t *RESTRICT in, location_t *RESTRICT out,
                          uint16_t *RESTRICT const hash, uint32_t *RESTRICT const chunk);
        void dictionary_d(location_t *RESTRICT in, location_t *RESTRICT out,
                          uint16_t *RESTRICT const hash, uint32_t *RESTRICT const chunk);
        void plain(location_t *, location_t *, uint16_t *const, uint32_t *const);
        inline void
        chunk(location_t *RESTRICT in, location_t *RESTRICT out, const lion_form_t form)
        {   uint16_t hash; uint32_t chunk;
            form_data.attachments[form](in, out, &hash, &chunk); }
        const lion_form_t read_form(location_t *RESTRICT in);
        void process_form(location_t *RESTRICT in, location_t *RESTRICT out);
        void process_unit(location_t *RESTRICT in, location_t *RESTRICT out);
        lion_decode_step_by_step_status_t
        chunk_step_by_step(location_t *RESTRICT read_memory_location,
                           teleport_t *RESTRICT in, location_t *RESTRICT out);
    };
#pragma pack(pop)
}
