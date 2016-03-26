// see LICENSE.md for license.
#pragma once

#include "densityxx/context.hpp"

namespace density {
    template<unsigned in_size, unsigned out_size>class file_buffer_t {
    private:
        FILE *rfp, *wfp;
        uint8_t in[in_size], out[out_size];

        inline buffer_state_t do_input(context_t &context)
        {   uint_fast64_t read = (uint_fast64_t)fread(in, 1, sizeof(in), rfp);
            context.update_input(in, read);
            if (read < sizeof(in) && ferror(rfp)) return buffer_state_error_on_input;
            return buffer_state_ready; }
        inline buffer_state_t do_output(context_t &context)
        {   uint_fast64_t available = context.output_available_for_use();
            uint_fast64_t written = (uint_fast64_t)fwrite(out, 1, available, wfp);
            if (written < available && ferror(wfp)) return buffer_state_error_on_output;
            context.update_output(out, sizeof(out));
            return buffer_state_ready; }
    public:
        inline file_buffer_t(FILE *rfp, FILE *wfp): rfp(rfp), wfp(wfp) {}
        inline ~file_buffer_t() {}

        inline size_t get_in_size(void) const { return sizeof(in); }
        inline size_t get_out_size(void) const { return sizeof(out); }

        inline void init(const compression_mode_t compression_mode,
                         const block_type_t block_type, context_t &context)
        {   context.init(compression_mode, block_type, in, sizeof(in), out, sizeof(out)); }
        inline buffer_state_t action(encode_state_t encode_state, context_t &context)
        {   switch (encode_state) {
            case encode_state_stall_on_input: return do_input(context);
            case encode_state_stall_on_output: return do_output(context);
            default: return buffer_state_error; } }
        inline buffer_state_t action(decode_state_t decode_state, context_t &context)
        {   switch (decode_state) {
            case decode_state_stall_on_input: return do_input(context);
            case decode_state_stall_on_output: return do_output(context);
            default: return buffer_state_error; } }
    };
}
