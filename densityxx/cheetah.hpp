// see LICENSE.md for license.
#pragma once
#include "densityxx/kernel.hpp"

namespace density {
    const unsigned cheetah_hash_bits = 16;
    const uint32_t cheetah_hash_multiplier = 0x9D6EF916U;
    inline uint16_t cheetah_hash_algorithm(uint32_t value32)
    {   return (uint16_t)(value32 * cheetah_hash_multiplier >> (32 - cheetah_hash_bits)); }

    typedef uint64_t cheetah_signature_t;

    typedef enum {
        cheetah_signature_flag_predicted = 0x0,
        cheetah_signature_flag_map_a = 0x1,
        cheetah_signature_flag_map_b = 0x2,
        cheetah_signature_flag_chunk = 0x3,
    } cheetah_signature_flag_t;
    DENSITY_ENUM_RENDER4(cheetah_signature_flag, predicted, map_a, map_b, chunk);

#pragma pack(push)
#pragma pack(4)
    //--- dictionary ---
    typedef struct {
        uint32_t chunk_a, chunk_b;
    } cheetah_dictionary_entry_t;
    typedef struct {
        uint32_t next_chunk_prediction;
    } cheetah_dictionary_prediction_entry_t;
    class cheetah_dictionary_t {
    public:
        cheetah_dictionary_entry_t entries[1 << cheetah_hash_bits];
        cheetah_dictionary_prediction_entry_t
        prediction_entries[1 << cheetah_hash_bits];
        inline void reset(void) { memset(this, 0, sizeof(*this)); }
    };

    //--- encode ---
    typedef enum {
        cheetah_encode_process_prepare_new_block,
        cheetah_encode_process_check_signature_state,
        cheetah_encode_process_read_chunk,
    } cheetah_encode_process_t;
    DENSITY_ENUM_RENDER3(cheetah_encode_process,
                         prepare_new_block, check_signature_state, read_chunk);

    class cheetah_encode_t: public kernel_encode_t {
    public:
        virtual compression_mode_t mode(void) const
        {   return compression_mode_cheetah_algorithm; }

        virtual kernel_encode_state_t init(void);
        virtual kernel_encode_state_t
        continue_(teleport_t *RESTRICT in, location_t *RESTRICT out);
        virtual kernel_encode_state_t
        finish(teleport_t *RESTRICT in, location_t *RESTRICT out);

        cheetah_signature_t proximity_signature;
        uint_fast16_t last_hash;
        uint_fast8_t shift;
        cheetah_signature_t *signature;
        uint_fast32_t signatures_count;
        bool efficiency_checked;
        bool signature_copied_to_memory;
        cheetah_encode_process_t process;
        cheetah_dictionary_t dictionary;
#if DENSITY_ENABLE_PARALLELIZABLE_DECOMPRESSIBLE_OUTPUT == DENSITY_YES
        uint_fast64_t reset_cycle;
#endif
    private:
        inline kernel_encode_state_t
        exit_process(cheetah_encode_process_t process,
                     kernel_encode_state_t kernel_encode_state)
        {   this->process = process; return kernel_encode_state; }
        void prepare_new_signature(location_t *RESTRICT out);
        kernel_encode_state_t prepare_new_block(location_t *RESTRICT out);
        kernel_encode_state_t check_state(location_t *RESTRICT out);
        void kernel(location_t *RESTRICT out, const uint16_t hash,
                    const uint32_t chunk, const uint_fast8_t shift);
        void process_unit(location_t *RESTRICT in, location_t *RESTRICT out);
    };

    //--- decode ---
    typedef enum {
        cheetah_decode_process_check_signature_state,
        cheetah_decode_process_read_processing_unit,
    } cheetah_decode_process_t;
    DENSITY_ENUM_RENDER2(cheetah_decode_process, check_signature_state, read_processing_unit);

    class cheetah_decode_t: public kernel_decode_t {
    public:
        virtual compression_mode_t mode(void) const
        {   return compression_mode_cheetah_algorithm; }

        kernel_decode_state_t
        init(const main_header_parameters_t parameters, const uint_fast8_t end_data_overhead);
        virtual kernel_decode_state_t
        continue_(teleport_t *RESTRICT in, location_t *RESTRICT out);
        virtual kernel_decode_state_t
        finish(teleport_t *RESTRICT in, location_t *RESTRICT out);

        cheetah_signature_t signature;
        uint_fast16_t last_hash;
        uint_fast8_t shift;
        uint_fast32_t signatures_count;
        bool efficiency_checked;
        cheetah_decode_process_t process;
        uint_fast8_t end_data_overhead;
        main_header_parameters_t parameters;
        cheetah_dictionary_t dictionary;
        uint_fast64_t reset_cycle;
    private:
        inline kernel_decode_state_t
        exit_process(cheetah_decode_process_t process,
                     kernel_decode_state_t kernel_decode_state)
        {   this->process = process; return kernel_decode_state; }
        kernel_decode_state_t check_state(location_t *RESTRICT out);
        void read_signature(location_t *RESTRICT in);
        void process_predicted(location_t *RESTRICT out);
        void process_compressed_a(const uint16_t hash, location_t *RESTRICT out);
        void process_compressed_b(const uint16_t hash, location_t *RESTRICT out);
        void process_uncompressed(const uint32_t chunk, location_t *RESTRICT out);
        void kernel(location_t *RESTRICT in, location_t *RESTRICT out, const uint8_t mode);
        void process_data(location_t *RESTRICT in, location_t *RESTRICT out);
    };
#pragma pack(pop)
}
