// see LICENSE.md for license.
#pragma once

#include "densityxx/block.hpp"

namespace density {
    // encode.
#pragma pack(push)
#pragma pack(4)
    class encode_t {
    public:
        typedef enum {
            state_ready = 0,
            state_stall_on_input,
            state_stall_on_output,
            state_error
        } state_t;
        DENSITY_ENUM_RENDER4(state, ready, stall_on_input, stall_on_output, error);

        state_t init(location_t *RESTRICT out, const compression_mode_t mode,
                     const block_type_t block_type);
        state_t continue_(teleport_t *RESTRICT in, location_t *RESTRICT out);
        state_t finish(teleport_t *RESTRICT in, location_t *RESTRICT out);

        inline const uint_fast64_t get_total_read(void) const { return total_read; }
        inline const uint_fast64_t get_total_written(void) const { return total_written; }
    private:
        typedef enum {
            process_write_header,
            process_write_blocks,
            process_write_footer,
        } process_t;
        DENSITY_ENUM_RENDER3(process, write_header, write_blocks, write_footer);

        process_t process;
        compression_mode_t compression_mode;
        block_type_t block_type;
        const struct stat *file_attributes;

        uint_fast64_t total_read, total_written;

        block_encode_t block_encode;

        inline state_t exit_process(process_t process, state_t state)
        {   this->process = process; return state; }

        state_t write_header(location_t *RESTRICT out,
                             const compression_mode_t compression_mode,
                             const block_type_t block_type);
        state_t write_footer(location_t *RESTRICT out);

        inline void update_totals(teleport_t *RESTRICT in, location_t *RESTRICT out,
                                  const uint_fast64_t available_in_before,
                                  const uint_fast64_t available_out_before)
        {
            total_read += available_in_before - in->available_bytes();
            total_written += available_out_before - out->available_bytes;
        }
    };
#pragma pack(pop)

    // decode.
#pragma pack(push)
#pragma pack(4)
    class decode_t {
    public:
        typedef enum {
            state_ready = 0,
            state_stall_on_input,
            state_stall_on_output,
            state_integrity_check_fail,
            state_error
        } state_t;
        DENSITY_ENUM_RENDER5(state, ready, stall_on_input, stall_on_output,
                             integrity_check_fail, error);

        state_t init(teleport_t *in);
        state_t continue_(teleport_t *in, location_t *out);
        state_t finish(teleport_t *in, location_t *out);

        inline const uint_fast64_t get_total_read(void) const { return total_read; }
        inline const uint_fast64_t get_total_written(void) const { return total_written; }
        inline const main_header_t &get_header(void) const { return header; }
    private:
        typedef enum {
            process_read_header,
            process_read_blocks,
            process_read_footer,
        } process_t;
        DENSITY_ENUM_RENDER3(process, read_header, read_blocks, read_footer);

        process_t process;

        uint_fast64_t total_read, total_written;

        main_header_t header;
        main_footer_t footer;

        block_decode_t block_decode;

#if DENSITY_WRITE_MAIN_FOOTER == DENSITY_YES && DENSITY_ENABLE_PARALLELIZABLE_DECOMPRESSIBLE_OUTPUT == DENSITY_YES
        static const size_t end_data_overhead = sizeof(density::main_footer_t);
#else
        static const size_t end_data_overhead = 0;
#endif

        inline state_t exit_process(process_t process, state_t state)
        {   this->process = process; return state; }
        state_t read_header(teleport_t *RESTRICT in);
        state_t read_footer(teleport_t *RESTRICT in);
        inline void update_totals(teleport_t *RESTRICT in, location_t *RESTRICT out,
                                  const uint_fast64_t available_in_before,
                                  const uint_fast64_t available_out_before)
        {
            total_read += available_in_before - in->available_bytes_reserved(end_data_overhead);
            total_written += available_out_before - out->available_bytes;
        }
    };
#pragma pack(pop)
}
