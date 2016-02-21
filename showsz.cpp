#include "densityxx/api.hpp"

#define SHOWSZ(TYPE) printf("sizeof(%s) = %u\n", #TYPE, (unsigned)sizeof(TYPE))
int
main(void)
{
    SHOWSZ(density::stream_t);
    SHOWSZ(density::stream_header_information_t);
    SHOWSZ(density::encode_t);
    SHOWSZ(density::decode_t);
    return 0;
}
