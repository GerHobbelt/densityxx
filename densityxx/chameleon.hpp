// see LICENSE.md for license.
#pragma once
#include "densityxx/kernel.hpp"

namespace density {
    typedef enum {
        chameleon_signature_flag_chunk = 0x0,
        chameleon_signature_flag_map = 0x1,
    }  chameleon_signature_flag_t;

#pragma pack(push)
#pragma pack(4)
    //--- dictionary ---
    typedef uint64_t chameleon_signature_t;
    typedef struct {
        uint32_t as_uint32_t;
    } chameleon_dictionary_entry_t;

    class chameleon_dictionary_t {
    public:
        chameleon_dictionary_entry_t entries[1 << hash_bits];
        inline void reset(void) { memset(entries, 0, sizeof(entries)); }
    };

    //--- encode ---
    typedef enum {
        chameleon_encode_process_prepare_new_block,
        chameleon_encode_process_check_signature_state,
        chameleon_encode_process_read_chunk,
    } chameleon_encode_process_t;
    DENSITY_ENUM_RENDER3(chameleon_encode_process,
                         prepare_new_block, check_signature_state, read_chunk);

    class chameleon_encode_t: public kernel_encode_t {
    public:
        virtual compression_mode_t mode(void) const
        {   return compression_mode_chameleon_algorithm; }

        virtual kernel_encode_state_t init(void);
        virtual kernel_encode_state_t
        continue_(teleport_t *RESTRICT in, location_t *RESTRICT out);
        virtual kernel_encode_state_t
        finish(teleport_t *RESTRICT in, location_t *RESTRICT out);

    private:
        // members.
        chameleon_signature_t proximity_signature;
        uint_fast8_t shift;
        chameleon_signature_t *signature;
        uint_fast32_t signatures_count;
        bool efficiency_checked;
        bool signature_copied_to_memory;

        chameleon_encode_process_t process;
        chameleon_dictionary_t dictionary;
#if DENSITY_ENABLE_PARALLELIZABLE_DECOMPRESSIBLE_OUTPUT == DENSITY_YES
        uint_fast64_t reset_cycle;
#endif

        inline kernel_encode_state_t
        exit_process(chameleon_encode_process_t process,
                     kernel_encode_state_t kernel_encode_state)
        {   this->process = process; return kernel_encode_state; }
        void prepare_new_signature(location_t *RESTRICT out);
        kernel_encode_state_t prepare_new_block(location_t *RESTRICT out);
        kernel_encode_state_t check_state(location_t *RESTRICT out);
        void kernel(location_t *RESTRICT out, const uint16_t, const uint32_t,
                    const uint_fast8_t);
        void process_unit(location_t *RESTRICT in, location_t *RESTRICT out);
    };

    //--- decode ---
    typedef enum {
        chameleon_decode_process_check_signature_state,
        chameleon_decode_process_read_processing_unit,
    } chameleon_decode_process_t;
    DENSITY_ENUM_RENDER2(chameleon_decode_process, check_signature_state, read_processing_unit);

    class chameleon_decode_t: public kernel_decode_t {
    public:
        virtual compression_mode_t mode(void) const
        {   return compression_mode_chameleon_algorithm; }

        virtual kernel_decode_state_t
        init(const main_header_parameters_t parameters, const uint_fast8_t end_data_overhead);
        virtual kernel_decode_state_t
        continue_(teleport_t *RESTRICT in, location_t *RESTRICT out);
        virtual kernel_decode_state_t
        finish(teleport_t *RESTRICT in, location_t *RESTRICT out);

    private:
        chameleon_signature_t signature;
        uint_fast8_t shift;
        uint_fast32_t body_length;
        uint_fast32_t signatures_count;
        uint_fast8_t efficiency_checked;
        chameleon_decode_process_t process;
        uint_fast8_t end_data_overhead;
        main_header_parameters_t parameters;
        chameleon_dictionary_t dictionary;
        uint_fast64_t reset_cycle;

        inline kernel_decode_state_t
        exit_process(chameleon_decode_process_t process,
                     kernel_decode_state_t kernel_decode_state)
        {   this->process = process; return kernel_decode_state; }
        kernel_decode_state_t check_state(location_t *RESTRICT out);
        void read_signature(location_t *RESTRICT in);
        inline void process_compressed(const uint16_t hash, location_t *RESTRICT out)
        {   DENSITY_MEMCPY(out->pointer, &dictionary.entries[hash].as_uint32_t,
                           sizeof(uint32_t)); }
        inline void process_uncompressed(const uint32_t chunk, location_t *RESTRICT out)
        {   const uint16_t hash = hash_algorithm(chunk);
            dictionary.entries[hash].as_uint32_t = chunk;
            DENSITY_MEMCPY(out->pointer, &chunk, sizeof(uint32_t)); }
        void kernel(location_t *RESTRICT in, location_t *RESTRICT out, const bool compressed);
        inline const bool test_compressed(const uint_fast8_t shift) const
        {   return (bool)((signature >> shift) & chameleon_signature_flag_map); }
        void process_data(location_t *RESTRICT in, location_t *RESTRICT out);
    };
#pragma pack(pop)
}
