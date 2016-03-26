#include "densityxx/context.hpp"
#include "densityxx/chameleon.hpp"
#include "densityxx/cheetah.hpp"
#include "densityxx/lion.hpp"

#define SHOWSZ(TYPE) printf("sizeof(%s) = %u\n", #TYPE, (unsigned)sizeof(TYPE))
int
main(void)
{
    SHOWSZ(density::context_t);
    SHOWSZ(density::main_header_t);
    SHOWSZ(density::main_footer_t);
    SHOWSZ(density::block_header_t);
    SHOWSZ(density::block_mode_marker_t);
    SHOWSZ(density::block_footer_t);
    SHOWSZ(density::location_t);
    SHOWSZ(density::teleport_t);
    SHOWSZ(density::chameleon_dictionary_t);
    SHOWSZ(density::chameleon_encode_t);
    SHOWSZ(density::chameleon_decode_t);
    SHOWSZ(density::cheetah_dictionary_t);
    SHOWSZ(density::cheetah_encode_t);
    SHOWSZ(density::cheetah_decode_t);
    SHOWSZ(density::lion_dictionary_t);
    SHOWSZ(density::lion_encode_t);
    SHOWSZ(density::lion_decode_t);
    return 0;
}
