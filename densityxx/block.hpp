// see LICENSE.md for license.
#pragma once

#include "densityxx/kernel.hpp"
#include "densityxx/spookyhash.hpp"

#define DENSITY_PREFERRED_COPY_BLOCK_SIZE  (1 << 19)
#define DENSITY_SPOOKYHASH_SEED_1  (0xabc)
#define DENSITY_SPOOKYHASH_SEED_2  (0xdef)

namespace density {
    // encode.
    typedef enum {
        block_encode_state_ready = 0,
        block_encode_state_stall_on_input,
        block_encode_state_stall_on_output,
        block_encode_state_error
    } block_encode_state_t;
    DENSITY_ENUM_RENDER4(block_encode_state,
                         ready, stall_on_input, stall_on_output, error);

    typedef enum {
        block_encode_process_write_block_header,
        block_encode_process_write_block_mode_marker,
        block_encode_process_write_block_footer,
        block_encode_process_write_data,
    } block_encode_process_t;
    DENSITY_ENUM_RENDER4(block_encode_process, write_block_header,
                         write_block_mode_marker, write_block_footer, write_data);

#pragma pack(push)
#pragma pack(4)
    class block_encode_t {
    public:
        block_encode_process_t process;
        compression_mode_t current_mode, target_mode;
        block_type_t block_type;

        uint_fast64_t total_read, total_written;

        // current_block_data.
        uint_fast64_t in_start, out_start;

        // integrity_data.
        bool update;
        uint8_t *input_pointer;
        spookyhash_context_t context;

        kernel_encode_t *kernel_encode;

        block_encode_state_t
        init(kernel_encode_t *kernel_encode, const block_type_t block_type);
        block_encode_state_t continue_(teleport_t *RESTRICT in, location_t *RESTRICT out);
        block_encode_state_t finish(teleport_t *RESTRICT in, location_t *RESTRICT out);
    private:
        inline block_encode_state_t
        exit_process(block_encode_process_t process, block_encode_state_t block_encode_state)
        {   this->process = process; return block_encode_state; }

        inline void update_integrity_data(teleport_t *RESTRICT in)
        {   input_pointer = in->direct.pointer; update = false; }

        void update_integrity_hash(teleport_t *RESTRICT in, bool pending_exit);

        block_encode_state_t
        write_block_header(teleport_t *RESTRICT in, location_t *RESTRICT out);

        block_encode_state_t
        write_block_footer(teleport_t *RESTRICT in, location_t *RESTRICT out);

        block_encode_state_t write_mode_marker(location_t *RESTRICT out);

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
    typedef enum {
        block_decode_state_ready = 0,
        block_decode_state_stall_on_input,
        block_decode_state_stall_on_output,
        block_decode_state_integrity_check_fail,
        block_decode_state_error
    } block_decode_state_t;
    DENSITY_ENUM_RENDER5(block_decode_state,
                         ready, stall_on_input, stall_on_output,
                         integrity_check_fail, error);

    typedef enum {
        block_decode_process_read_block_header,
        block_decode_process_read_block_mode_marker,
        block_decode_process_read_block_footer,
        block_decode_process_read_data,
    } block_decode_process_t;
    DENSITY_ENUM_RENDER4(block_decode_process, read_block_header,
                         read_block_mode_marker, read_block_footer, read_data);

#pragma pack(push)
#pragma pack(4)
    class block_decode_t {
    public:
        block_decode_process_t process;
        compression_mode_t current_mode, target_mode;
        block_type_t block_type;

        uint_fast64_t total_read;
        uint_fast64_t total_written;
        uint_fast8_t end_data_overhead;

        bool read_block_header_content;
        block_header_t last_block_header;
        block_mode_marker_t last_mode_marker;
        block_footer_t last_block_footer;

        // current_block_data.
        uint_fast64_t in_start;
        uint_fast64_t out_start;

        // integrity_data.
        bool update;
        uint8_t *output_pointer;
        spookyhash_context_t context;

        kernel_decode_t *kernel_decode;

        block_decode_state_t
        init(kernel_decode_t *kernel_decode, const block_type_t block_type,
             const main_header_parameters_t parameters, const uint_fast8_t end_data_overhead);
        block_decode_state_t continue_(teleport_t *RESTRICT in, location_t *RESTRICT out);
        block_decode_state_t finish(teleport_t *RESTRICT in, location_t *RESTRICT out);
    private:
        inline block_decode_state_t
        exit_process(block_decode_process_t process, block_decode_state_t block_decode_state)
        {   this->process = process; return block_decode_state; }

        inline void
        update_integrity_data(location_t *RESTRICT out)
        {   output_pointer = out->pointer; update = false; }

        void update_integrity_hash(location_t *RESTRICT out, bool pending_exit);
        block_decode_state_t read_block_header(teleport_t *RESTRICT in, location_t *out);
        block_decode_state_t read_block_mode_marker(teleport_t *RESTRICT in);
        block_decode_state_t
        read_block_footer(teleport_t *RESTRICT in, location_t *RESTRICT out);

        inline void
        update_totals(teleport_t *RESTRICT in, location_t *RESTRICT out,
                      const uint_fast64_t available_in_before,
                      const uint_fast64_t available_out_before)
        {
            total_read += available_in_before - in->available_bytes_reserved(end_data_overhead);
            total_written += available_out_before - out->available_bytes;
        }
    };
}
