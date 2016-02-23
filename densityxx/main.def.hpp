// see LICENSE.md for license.
#pragma once

#include "densityxx/block.hpp"

namespace density {
    // encode.
    typedef enum {
        encode_state_ready = 0,
        encode_state_stall_on_input,
        encode_state_stall_on_output,
        encode_state_error
    } encode_state_t;
    DENSITY_ENUM_RENDER4(encode_state, ready, stall_on_input, stall_on_output, error);

    typedef enum {
        encode_process_write_header,
        encode_process_write_blocks,
        encode_process_write_footer,
    } encode_process_t;
    DENSITY_ENUM_RENDER3(encode_process, write_header, write_blocks, write_footer);

#pragma pack(push)
#pragma pack(4)
    class encode_t {
    public:
        encode_process_t process;
        compression_mode_t compression_mode;
        block_type_t block_type;
        const struct stat *file_attributes;

        uint_fast64_t total_read, total_written;

        block_encode_t block_encode;

        encode_state_t
        init(location_t *RESTRICT out, const compression_mode_t mode,
             const block_type_t block_type);
        encode_state_t continue_(teleport_t *RESTRICT in, location_t *RESTRICT out);
        encode_state_t finish(teleport_t *RESTRICT in, location_t *RESTRICT out);
    private:
        inline encode_state_t exit_process(encode_process_t process, encode_state_t state)
        {   this->process = process; return state; }

        encode_state_t write_header(location_t *RESTRICT out,
                                    const compression_mode_t compression_mode,
                                    const block_type_t block_type);
        encode_state_t write_footer(location_t *RESTRICT out);

        inline void
        update_totals(teleport_t *RESTRICT in, location_t *RESTRICT out,
                      const uint_fast64_t available_in_before,
                      const uint_fast64_t available_out_before)
        {
            total_read += available_in_before - in->available_bytes();
            total_written += available_out_before - out->available_bytes;
        }
    };
#pragma pack(pop)

    // decode.
#if DENSITY_WRITE_MAIN_FOOTER == DENSITY_YES && DENSITY_ENABLE_PARALLELIZABLE_DECOMPRESSIBLE_OUTPUT == DENSITY_YES
    const size_t decode_end_data_overhead = sizeof(density::main_footer_t);
#else
    const size_t decode_end_data_overhead = 0;
#endif

    typedef enum {
        decode_state_ready = 0,
        decode_state_stall_on_input,
        decode_state_stall_on_output,
        decode_state_integrity_check_fail,
        decode_state_error
    } decode_state_t;
    DENSITY_ENUM_RENDER5(decode_state, ready, stall_on_input, stall_on_output,
                         integrity_check_fail, error);

    typedef enum {
        decode_process_read_header,
        decode_process_read_blocks,
        decode_process_read_footer,
    } decode_process_t;
    DENSITY_ENUM_RENDER3(decode_process, read_header, read_blocks, read_footer);

#pragma pack(push)
#pragma pack(4)
    class decode_t {
    public:
        decode_process_t process;

        uint_fast64_t total_read, total_written;

        main_header_t header;
        main_footer_t footer;

        block_decode_t block_decode;

        decode_state_t init(teleport_t *in);
        decode_state_t continue_(teleport_t *in, location_t *out);
        decode_state_t finish(teleport_t *in, location_t *out);
    private:
        inline decode_state_t
        exit_process(decode_process_t process, decode_state_t decode_state)
        {   this->process = process; return decode_state; }
        decode_state_t read_header(teleport_t *RESTRICT in);
        decode_state_t read_footer(teleport_t *RESTRICT in);
        inline void
        update_totals(teleport_t *RESTRICT in, location_t *RESTRICT out,
                      const uint_fast64_t available_in_before,
                      const uint_fast64_t available_out_before)
        {
            total_read += available_in_before - in->available_bytes_reserved(decode_end_data_overhead);
            total_written += available_out_before - out->available_bytes;
        }
    };
#pragma pack(pop)
}
