// see LICENSE.md for license.
#include "densityxx/api.def.hpp"
#include "densityxx/context.hpp"
#include "densityxx/block.hpp"

namespace density {
    // buffer.
    static DENSITY_INLINE processing_result_t
    return_processing_result(context_t &context, state_t state)
    {
        processing_result_t result;
        result.state = state;
        result.bytes_read = context.get_total_read();
        result.bytes_written = context.get_total_written();
        return result;
    }

#define RETURN_RESULT(suffix) \
    return return_processing_result(context, state_##suffix)

    template<class KERNEL_ENCODE_T>static DENSITY_INLINE encode_state_t
    do_compress(uint32_t *relative_position, context_t &context)
    {
        encode_state_t encode_state;
        block_encode_t<KERNEL_ENCODE_T> *block_encode = new block_encode_t<KERNEL_ENCODE_T>();
        if ((encode_state = block_encode->init(context))) goto quit;
        if ((encode_state = context.after(block_encode->continue_(context.before()))))
            goto quit;
        if ((encode_state = context.after(block_encode->finish(context.before())))) goto quit;
        *relative_position = block_encode->read_bytes();
    quit:
        delete block_encode;
        return encode_state;
    }
    processing_result_t
    compress(const uint8_t *in, const uint_fast64_t szin,
             uint8_t *out, const uint_fast64_t szout,
             const compression_mode_t compression_mode,
             const block_type_t block_type)
    {
        context_t context;
        uint32_t relative_position;

        context.init(compression_mode, block_type, in, szin, out, szout);
        switch (context.write_header()) {
        case encode_state_ready: break;
        case encode_state_stall_on_output: RETURN_RESULT(error_output_buffer_too_small);
        default: RETURN_RESULT(error_during_processing);
        }
        switch (compression_mode) {
        case compression_mode_copy:
            switch (do_compress<copy_encode_t>(&relative_position, context)) {
            case encode_state_ready: break;
            case encode_state_stall_on_output: RETURN_RESULT(error_output_buffer_too_small);
            default: RETURN_RESULT(error_during_processing);
            }
            break;
        case compression_mode_chameleon_algorithm:
            switch (do_compress<chameleon_encode_t>(&relative_position, context)) {
            case encode_state_ready: break;
            case encode_state_stall_on_output: RETURN_RESULT(error_output_buffer_too_small);
            default: RETURN_RESULT(error_during_processing);
            }
            break;
        case compression_mode_cheetah_algorithm:
            switch (do_compress<cheetah_encode_t>(&relative_position, context)) {
            case encode_state_ready: break;
            case encode_state_stall_on_output: RETURN_RESULT(error_output_buffer_too_small);
            default: RETURN_RESULT(error_during_processing);
            }
            break;
        case compression_mode_lion_algorithm:
            switch (do_compress<lion_encode_t>(&relative_position, context)) {
            case encode_state_ready: break;
            case encode_state_stall_on_output: RETURN_RESULT(error_output_buffer_too_small);
            default: RETURN_RESULT(error_during_processing);
            }
            break;
        }
        switch (context.write_footer(relative_position)) {
        case encode_state_ready: break;
        case encode_state_stall_on_output: RETURN_RESULT(error_output_buffer_too_small);
        default: RETURN_RESULT(error_during_processing);
        }
        RETURN_RESULT(ok);
    }

    template<class KERNEL_DECODE_T>static DENSITY_INLINE decode_state_t
    do_decompress(context_t &context)
    {
        decode_state_t decode_state;
        block_decode_t<KERNEL_DECODE_T> *block_decode = new block_decode_t<KERNEL_DECODE_T>();
        if ((decode_state = block_decode->init(context))) goto quit;
        if ((decode_state = context.after(block_decode->continue_(context.before()))))
            goto quit;
        if ((decode_state = context.after(block_decode->finish(context.before())))) goto quit;
    quit:
        delete block_decode;
        return decode_state;
    }
    processing_result_t
    decompress(const uint8_t *in, const uint_fast64_t szin,
               uint8_t *out, const uint_fast64_t szout)
    {
        context_t context;

        context.init(compression_mode_copy, block_type_default, in, szin, out, szout);
        switch (context.read_header()) {
        case decode_state_ready: break;
        case decode_state_stall_on_output: RETURN_RESULT(error_output_buffer_too_small);
        default: RETURN_RESULT(error_during_processing);
        }
        switch (context.header.compression_mode()) {
        case compression_mode_copy:
            switch (do_decompress<copy_decode_t>(context)) {
            case decode_state_ready: break;
            case decode_state_stall_on_output: RETURN_RESULT(error_output_buffer_too_small);
            default: RETURN_RESULT(error_during_processing);
            }
            break;
        case compression_mode_chameleon_algorithm:
            switch (do_decompress<chameleon_decode_t>(context)) {
            case decode_state_ready: break;
            case decode_state_stall_on_output: RETURN_RESULT(error_output_buffer_too_small);
            default: RETURN_RESULT(error_during_processing);
            }
            break;
        case compression_mode_cheetah_algorithm:
            switch (do_decompress<cheetah_decode_t>(context)) {
            case decode_state_ready: break;
            case decode_state_stall_on_output: RETURN_RESULT(error_output_buffer_too_small);
            default: RETURN_RESULT(error_during_processing);
            }
            break;
        case compression_mode_lion_algorithm:
            switch (do_decompress<lion_decode_t>(context)) {
            case decode_state_ready: break;
            case decode_state_stall_on_output: RETURN_RESULT(error_output_buffer_too_small);
            default: RETURN_RESULT(error_during_processing);
            }
            break;
        }
        switch (context.read_footer()) {
        case decode_state_ready: break;
        case decode_state_stall_on_output: RETURN_RESULT(error_output_buffer_too_small);
        default: RETURN_RESULT(error_during_processing);
        }
        RETURN_RESULT(ok);
    }
}
