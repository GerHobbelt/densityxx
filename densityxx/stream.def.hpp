// see LICENSE.md for license.
#pragma once
#include "densityxx/main.hpp"

namespace density {
    const size_t minimum_output_buffer_size = 1 << 10;
    // stream.
    class stream_t {
    public:
        typedef enum {
            state_ready = 0,                      // Awaiting further instructions (new action or adding data to the input buffer)
            state_stall_on_input,                 // There is not enough space left in the input buffer to continue
            state_stall_on_output,                // There is not enough space left in the output buffer to continue
            state_error_output_buffer_too_small,  // Output buffer size is too small
            state_error_invalid_internal_state,   // Error during processing
            state_error_integrity_check_fail      // Integrity check has failed
        } state_t;
        DENSITY_ENUM_RENDER6(state, ready, stall_on_input, stall_on_output,
                             error_output_buffer_too_small,
                             error_invalid_internal_state,
                             error_integrity_check_fail);

        struct header_information_t {
            uint8_t major_version, minor_version, revision;
            compression_mode_t compression_mode;
            block_type_t block_type;
        };

        inline stream_t(void): in(memory_teleport_buffer_size), out() {}
        ~stream_t() {}

        state_t prepare(const uint8_t *RESTRICT in, const uint_fast64_t sz,
                        uint8_t *RESTRICT out, const uint_fast64_t szout);
        inline state_t update_input(const uint8_t *RESTRICT in, const uint_fast64_t szin)
        {   this->in.change_input_buffer(in, szin); return state_ready; }
        inline state_t update_output(uint8_t *RESTRICT out, const uint_fast64_t szout)
        {   this->out.encapsulate(out, szout); return state_ready; }
        inline uint_fast64_t output_available_for_use(void) const { return out.used(); }
        inline state_t check_conformity(void) const
        {   if (out.initial_available_bytes < minimum_output_buffer_size)
                return state_error_output_buffer_too_small;
            return state_ready; }
    protected:
        typedef enum {
            process_prepared = 0,
            process_inited,
            process_started,
            process_finished,
        } process_t;
        DENSITY_ENUM_RENDER4(process, prepared, inited, started, finished);
        process_t process;
        teleport_t in;
        location_t out;
    private:
        static const size_t memory_teleport_buffer_size = 1 << 16;
    };

    template<class KERNEL_ENCODE_T>class stream_encode_t: public stream_t {
    public:
        state_t init(const block_type_t block_type);
        state_t continue_(void);
        state_t finish(void);
        inline const uint_fast64_t get_total_read(void) const
        {   return encode.get_total_read(); }
        inline const uint_fast64_t get_total_written(void) const
        {   return encode.get_total_written(); }
    private:
        encode_t<KERNEL_ENCODE_T> encode;
    };

    template<class KERNEL_DECODE_T>class stream_decode_t: public stream_t {
    public:
        state_t init(header_information_t *RESTRICT header_information);
        state_t continue_(void);
        state_t finish(void);
        inline const uint_fast64_t get_total_read(void) const
        {   return decode.get_total_read(); }
        inline const uint_fast64_t get_total_written(void) const
        {   return decode.get_total_written(); }
    private:
        decode_t<KERNEL_DECODE_T> decode;
    };
}
