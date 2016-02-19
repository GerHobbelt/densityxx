// see LICENSE.md for license.
#pragma once

#include "densityxx/kernel.hpp"

namespace density {
#define DENSITY_CHAMELEON_HASH_BITS         16
#pragma pack(push)
#pragma pack(4)
    typedef uint64_t chameleon_signature_t;
    typedef struct {
        uint32_t as_uint32_t;
    } chameleon_dictionary_entry_t;

    class chameleon_dictionary_t {
    public:
        chameleon_dictionary_entry_t entries[1 << DENSITY_CHAMELEON_HASH_BITS];
        inline void reset(void) { memset(entries, 0, sizeof(entries)); }
    };

    //--- encode ---
    typedef enum {
        chameleon_encode_process_prepare_new_block,
        chameleon_encode_process_check_signature_state,
        chameleon_encode_process_read_chunk,
    } chameleon_encode_process_t;

    class chameleon_encode_t: public kernel_encode_t {
    public:
        chameleon_encode_t(void);
        virtual ~chameleon_encode_t();

        virtual compression_mode_t mode(void) const
        {   return compression_mode_chameleon_algorithm; }

        virtual kernel_encode_state_t init(void);
        virtual kernel_encode_state_t
        continue_(teleport_t *RESTRICT in, location_t *RESTRICT out);
        virtual kernel_encode_state_t
        finish(teleport_t *RESTRICT in, location_t *RESTRICT out);

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
    private:
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

    class chameleon_decode_t: public kernel_decode_t {
    public:
        chameleon_decode_t(const main_header_parameters_t parameters,
                           const uint_fast8_t end_data_overhead);
        virtual ~chameleon_decode_t();

        virtual compression_mode_t mode(void) const
        {   return compression_mode_chameleon_algorithm; }
        virtual kernel_decode_state_t
        continue_(teleport_t *RESTRICT in, location_t *RESTRICT out);
        virtual kernel_decode_state_t
        finish(teleport_t *RESTRICT in, location_t *RESTRICT out);

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
    private:
        inline kernel_decode_state_t
        exit_process(chameleon_decode_process_t process,
                     kernel_decode_state_t kernel_decode_state)
        {   this->process = process; return kernel_decode_state; }

        kernel_decode_state_t check_state(location_t *RESTRICT out);
        void read_signature(location_t *RESTRICT in);
        void process_compressed(const uint16_t hash, location_t *RESTRICT out);
        void process_uncompressed(const uint32_t chunk, location_t *RESTRICT out);
        void decode_kernel(location_t *RESTRICT in, location_t *RESTRICT out,
                           const bool compressed);
        const bool test_compressed(const uint_fast8_t shift);
        void process_data(location_t *RESTRICT in, location_t *RESTRICT out);
    };
#pragma pack(pop)
}
