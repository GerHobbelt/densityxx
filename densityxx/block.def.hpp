// see LICENSE.md for license.
#pragma once
#include "densityxx/kernel.hpp"
#include "densityxx/spookyhash.hpp"
#include "densityxx/context.hpp"

namespace density {
    // encode.
#pragma pack(push)
#pragma pack(4)
    class block_encode_base_t {
    public:
        inline const compression_mode_t mode(void) const { return target_mode; }
        inline const block_type_t get_block_type(void) const { return block_type; }
    protected:
        typedef enum {
            process_write_block_header,
            process_write_block_mode_marker,
            process_write_block_footer,
            process_write_data,
        } process_t;
        DENSITY_ENUM_RENDER4(process, write_block_header, write_block_mode_marker,
                             write_block_footer, write_data);

        process_t process;
        compression_mode_t current_mode, target_mode;
        block_type_t block_type;

        uint_fast64_t total_read, total_written;

        // current_block_data.
        uint_fast64_t in_start, out_start;

        // integrity_data.
        bool update;
        uint8_t *input_pointer;
        spookyhash_context_t context;

        inline encode_state_t exit_process(process_t process, encode_state_t state)
        {   this->process = process; return state; }

        inline uint32_t read_bytes(void) const
        {   return total_read > in_start ? total_read - in_start: 0; }

        inline void update_integrity_data(teleport_t *RESTRICT in)
        {   input_pointer = in->direct.pointer; update = false; }

        void update_integrity_hash(teleport_t *RESTRICT in, bool pending_exit);

        encode_state_t write_block_header(teleport_t *RESTRICT in, location_t *RESTRICT out);
        encode_state_t write_block_footer(teleport_t *RESTRICT in, location_t *RESTRICT out);
        encode_state_t write_mode_marker(location_t *RESTRICT out);

        inline void
        update_totals(teleport_t *RESTRICT in, location_t *RESTRICT out,
                      const uint_fast64_t available_in_before,
                      const uint_fast64_t available_out_before)
        {
            total_read += available_in_before - in->available_bytes();
            total_written += available_out_before - out->available_bytes;
        }
    };
    template<class KERNEL_ENCODE_T>class block_encode_t: public block_encode_base_t {
    public:
        encode_state_t init(context_t *context);
        encode_state_t continue_(context_t *context);
        encode_state_t finish(context_t *context);
    private:
        KERNEL_ENCODE_T kernel_encode;
    };
#pragma pack(pop)

    // decode.
#pragma pack(push)
#pragma pack(4)
    class block_decode_base_t {
    public:
        inline const compression_mode_t mode(void) const { return target_mode; }
        inline const block_type_t get_block_type(void) const { return block_type; }
    protected:
        typedef enum {
            process_read_block_header,
            process_read_block_mode_marker,
            process_read_block_footer,
            process_read_data,
        } process_t;
        DENSITY_ENUM_RENDER4(process, read_block_header, read_block_mode_marker,
                             read_block_footer, read_data);

        process_t process;
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

        inline decode_state_t exit_process(process_t process, decode_state_t state)
        {   this->process = process; return state; }

        inline void update_integrity_data(location_t *RESTRICT out)
        {   output_pointer = out->pointer; update = false; }

        void update_integrity_hash(location_t *RESTRICT out, bool pending_exit);
        decode_state_t read_block_header(teleport_t *RESTRICT in, location_t *out);
        decode_state_t read_block_mode_marker(teleport_t *RESTRICT in);
        decode_state_t read_block_footer(teleport_t *RESTRICT in, location_t *RESTRICT out);

        inline void
        update_totals(teleport_t *RESTRICT in, location_t *RESTRICT out,
                      const uint_fast64_t available_in_before,
                      const uint_fast64_t available_out_before)
        {
            total_read += available_in_before - in->available_bytes_reserved(end_data_overhead);
            total_written += available_out_before - out->available_bytes;
        }
    };
    template<class KERNEL_DECODE_T>class block_decode_t: public block_decode_base_t {
    public:
        decode_state_t init(context_t *context);
        decode_state_t continue_(context_t *context);
        decode_state_t finish(context_t *context);
    private:
        KERNEL_DECODE_T kernel_decode;
    };
#pragma pack(pop)
}
