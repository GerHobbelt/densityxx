#include "densityxx/file_buffer.hpp"
#include "densityxx/block.hpp"
#include "densityxx/copy.hpp"

static void exit_error(density::buffer_state_t buffer_state)
{
    fprintf(stderr, "%s\n", density::buffer_state_render(buffer_state).c_str());
    exit(-1);
}

int
main(void)
{
    density::context_t context;
    density::encode_state_t encode_state;
    density::buffer_state_t buffer_state;
    FILE *rfp = fopen("SConstruct", "rb");
    FILE *wfp = fopen("SConstruct.sharc", "wb");
    density::file_buffer_t<1 << 19, 1 << 19> buffer(rfp, wfp);
    density::block_encode_t<density::copy_encode_t> block_encode;

    buffer.init(density::compression_mode_copy, density::block_type_default, context);
    block_encode.init(context);
    while ((encode_state = context.write_header()))
        if ((buffer_state = buffer.action(encode_state, context))) exit_error(buffer_state);
    while ((encode_state = context.after(block_encode.continue_(context.before()))))
        if ((buffer_state = buffer.action(encode_state, context))) exit_error(buffer_state);
        else if (context.in.available_bytes() < (1 << 19)) break;
    while ((encode_state = context.after(block_encode.finish(context.before()))))
        if ((buffer_state = buffer.action(encode_state, context))) exit_error(buffer_state);
    while ((encode_state = context.write_footer()))
        if ((buffer_state = buffer.action(encode_state, context))) exit_error(buffer_state);
    if ((buffer_state = buffer.action(density::encode_state_stall_on_output, context)))
        exit_error(buffer_state);
    fclose(rfp); fclose(wfp);
    return 0;
}
