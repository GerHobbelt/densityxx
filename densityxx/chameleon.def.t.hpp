// see LICENSE.md for license.
#pragma once
#include "densityxx/kernel.t.hpp"

namespace density {
    typedef uint64_t chameleon_signature_t;

    typedef enum {
        chameleon_signature_flag_chunk = 0x0,
        chameleon_signature_flag_map = 0x1,
    }  chameleon_signature_flag_t;

#pragma pack(push)
#pragma pack(4)
    //--- dictionary ---
    class chameleon_dictionary_t {
    public:
        typedef struct {
            uint32_t as_uint32_t;
        } entry_t;

        entry_t entries[1 << hash_bits];
        inline void reset(void) { memset(entries, 0, sizeof(entries)); }
    };

    //--- encode ---
    class chameleon_encode_t: public kernel_encode_t {
    public:
        inline compression_mode_t mode(void) const
        {   return compression_mode_chameleon_algorithm; }

        state_t init(void);
        state_t continue_(teleport_t *RESTRICT in, location_t *RESTRICT out);
        state_t finish(teleport_t *RESTRICT in, location_t *RESTRICT out);
    private:
        typedef enum {
            process_prepare_new_block,
            process_check_signature_state,
            process_read_chunk,
        } process_t;
        DENSITY_ENUM_RENDER3(process, prepare_new_block, check_signature_state, read_chunk);

        // members.
        chameleon_signature_t proximity_signature;
        uint_fast8_t shift;
        chameleon_signature_t *signature;
        uint_fast32_t signatures_count;
        bool efficiency_checked;
        bool signature_copied_to_memory;

        process_t process;
        chameleon_dictionary_t dictionary;
#if DENSITY_ENABLE_PARALLELIZABLE_DECOMPRESSIBLE_OUTPUT == DENSITY_YES
        uint_fast64_t reset_cycle;
#endif

        inline state_t exit_process(process_t process, state_t kernel_encode_state)
        {   this->process = process; return kernel_encode_state; }
        void prepare_new_signature(location_t *RESTRICT out);
        state_t prepare_new_block(location_t *RESTRICT out);
        state_t check_state(location_t *RESTRICT out);
        void kernel(location_t *RESTRICT out, const uint16_t, const uint32_t,
                    const uint_fast8_t);
        void process_unit(location_t *RESTRICT in, location_t *RESTRICT out);
    };

    //--- decode ---
    class chameleon_decode_t: public kernel_decode_t {
    public:
        inline compression_mode_t mode(void) const
        {   return compression_mode_chameleon_algorithm; }

        state_t init(const main_header_parameters_t parameters,
                     const uint_fast8_t end_data_overhead);
        state_t continue_(teleport_t *RESTRICT in, location_t *RESTRICT out);
        state_t finish(teleport_t *RESTRICT in, location_t *RESTRICT out);
    private:
        typedef enum {
            process_check_signature_state,
            process_read_processing_unit,
        } process_t;
        DENSITY_ENUM_RENDER2(process, check_signature_state, read_processing_unit);

        chameleon_signature_t signature;
        uint_fast8_t shift;
        uint_fast32_t body_length;
        uint_fast32_t signatures_count;
        uint_fast8_t efficiency_checked;
        process_t process;
        uint_fast8_t end_data_overhead;
        main_header_parameters_t parameters;
        chameleon_dictionary_t dictionary;
        uint_fast64_t reset_cycle;

        inline state_t exit_process(process_t process, state_t kernel_decode_state)
        {   this->process = process; return kernel_decode_state; }
        state_t check_state(location_t *RESTRICT out);
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
