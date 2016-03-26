// see LICENSE.md for license.
#pragma once

#include <sys/types.h>
#include <sys/stat.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <typeinfo>
#include <string>

#define DENSITY_YES  1
#define DENSITY_NO   0

#define DENSITY_WRITE_MAIN_HEADER  DENSITY_YES
#define DENSITY_WRITE_MAIN_FOOTER  DENSITY_YES

#define DENSITY_ENABLE_PARALLELIZABLE_DECOMPRESSIBLE_OUTPUT  DENSITY_NO

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define LITTLE_ENDIAN_64(b)   ((uint64_t)b)
#define LITTLE_ENDIAN_32(b)   ((uint32_t)b)
#define LITTLE_ENDIAN_16(b)   ((uint16_t)b)
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#if __GNUC__ * 100 + __GNUC_MINOR__ >= 403
#define LITTLE_ENDIAN_64(b)   __builtin_bswap64(b)
#define LITTLE_ENDIAN_32(b)   __builtin_bswap32(b)
#define LITTLE_ENDIAN_16(b)   __builtin_bswap16(b)
#else
#warning Using bulk byte swap routines. Expect performance issues.
#define LITTLE_ENDIAN_64(b)   ((((b) & 0xFF00000000000000ull) >> 56) | (((b) & 0x00FF000000000000ull) >> 40) | (((b) & 0x0000FF0000000000ull) >> 24) | (((b) & 0x000000FF00000000ull) >> 8) | (((b) & 0x00000000FF000000ull) << 8) | (((b) & 0x0000000000FF0000ull) << 24ull) | (((b) & 0x000000000000FF00ull) << 40) | (((b) & 0x00000000000000FFull) << 56))
#define LITTLE_ENDIAN_32(b)   ((((b) & 0xFF000000) >> 24) | (((b) & 0x00FF0000) >> 8) | (((b) & 0x0000FF00) << 8) | (((b) & 0x000000FF) << 24))
#define LITTLE_ENDIAN_16(b)   ((((b) & 0xFF00) >> 8) | (((b) & 0x00FF) << 8))
#endif
#else
#error Unknow endianness
#endif

#define RESTRICT

#define DENSITY_INLINE inline __attribute__((always_inline))
#define DENSITY_MEMCPY  __builtin_memcpy
#define DENSITY_MEMMOVE  __builtin_memmove

#define DENSITY_LIKELY(x)  __builtin_expect(!!(x), 1)
#define DENSITY_UNLIKELY(x)  __builtin_expect(!!(x), 0)

#define DENSITY_BITSIZEOF(x) (8 * sizeof(x))


#ifdef DENSITY_SHOW
#define DENSITY_SHOW_IN(IN, SZ)                                         \
    fprintf(stderr, "%s::%s(%u): in(%p, %u)\n", typeid(*this).name(),   \
            __FUNCTION__, __LINE__, (IN)->pointer, (unsigned)(SZ))
#define DENSITY_SHOW_OUT(OUT, SZ)                                       \
    fprintf(stderr, "%s::%s(%u): out(%p, %u)\n", typeid(*this).name(),  \
            __FUNCTION__, __LINE__, (OUT)->pointer, (unsigned)(SZ))
#else
#define DENSITY_SHOW_IN(IN, SZ)
#define DENSITY_SHOW_OUT(OUT, SZ)
#endif

#define DENSITY_ENUM_RENDER_PROTOTYPE(ENUM_TYPE)                        \
    DENSITY_INLINE std::string ENUM_TYPE##_render(const ENUM_TYPE##_t ENUM_TYPE)
#define DENSITY_ENUM_RENDER_VALUE(ENUM_TYPE, ENUM_VALUE)                \
    case ENUM_TYPE##_##ENUM_VALUE: return #ENUM_TYPE "_" #ENUM_VALUE

#define DENSITY_ENUM_RENDER2(ENUM_TYPE, ENUM_VALUE1, ENUM_VALUE2)       \
    DENSITY_ENUM_RENDER_PROTOTYPE(ENUM_TYPE) {                          \
        switch (ENUM_TYPE) {                                            \
            DENSITY_ENUM_RENDER_VALUE(ENUM_TYPE, ENUM_VALUE1);          \
            DENSITY_ENUM_RENDER_VALUE(ENUM_TYPE, ENUM_VALUE2);          \
        default: return #ENUM_TYPE "_???"; } }

#define DENSITY_ENUM_RENDER3(ENUM_TYPE, ENUM_VALUE1, ENUM_VALUE2, ENUM_VALUE3) \
    DENSITY_ENUM_RENDER_PROTOTYPE(ENUM_TYPE) {                          \
        switch (ENUM_TYPE) {                                            \
            DENSITY_ENUM_RENDER_VALUE(ENUM_TYPE, ENUM_VALUE1);          \
            DENSITY_ENUM_RENDER_VALUE(ENUM_TYPE, ENUM_VALUE2);          \
            DENSITY_ENUM_RENDER_VALUE(ENUM_TYPE, ENUM_VALUE3);          \
        default: return #ENUM_TYPE "_???"; } }

#define DENSITY_ENUM_RENDER4(ENUM_TYPE, ENUM_VALUE1, ENUM_VALUE2, ENUM_VALUE3, ENUM_VALUE4) \
    DENSITY_ENUM_RENDER_PROTOTYPE(ENUM_TYPE) {                          \
        switch (ENUM_TYPE) {                                            \
            DENSITY_ENUM_RENDER_VALUE(ENUM_TYPE, ENUM_VALUE1);          \
            DENSITY_ENUM_RENDER_VALUE(ENUM_TYPE, ENUM_VALUE2);          \
            DENSITY_ENUM_RENDER_VALUE(ENUM_TYPE, ENUM_VALUE3);          \
            DENSITY_ENUM_RENDER_VALUE(ENUM_TYPE, ENUM_VALUE4);          \
    default: return #ENUM_TYPE "_???"; } }

#define DENSITY_ENUM_RENDER5(ENUM_TYPE, ENUM_VALUE1, ENUM_VALUE2, ENUM_VALUE3, ENUM_VALUE4, ENUM_VALUE5) \
    DENSITY_ENUM_RENDER_PROTOTYPE(ENUM_TYPE) {                          \
        switch (ENUM_TYPE) {                                            \
            DENSITY_ENUM_RENDER_VALUE(ENUM_TYPE, ENUM_VALUE1);          \
            DENSITY_ENUM_RENDER_VALUE(ENUM_TYPE, ENUM_VALUE2);          \
            DENSITY_ENUM_RENDER_VALUE(ENUM_TYPE, ENUM_VALUE3);          \
            DENSITY_ENUM_RENDER_VALUE(ENUM_TYPE, ENUM_VALUE4);          \
            DENSITY_ENUM_RENDER_VALUE(ENUM_TYPE, ENUM_VALUE5);          \
        default: return #ENUM_TYPE "_???"; } }

#define DENSITY_ENUM_RENDER6(ENUM_TYPE, ENUM_VALUE1, ENUM_VALUE2, ENUM_VALUE3, ENUM_VALUE4, ENUM_VALUE5, ENUM_VALUE6) \
    DENSITY_ENUM_RENDER_PROTOTYPE(ENUM_TYPE) {                          \
        switch (ENUM_TYPE) {                                            \
            DENSITY_ENUM_RENDER_VALUE(ENUM_TYPE, ENUM_VALUE1);          \
            DENSITY_ENUM_RENDER_VALUE(ENUM_TYPE, ENUM_VALUE2);          \
            DENSITY_ENUM_RENDER_VALUE(ENUM_TYPE, ENUM_VALUE3);          \
            DENSITY_ENUM_RENDER_VALUE(ENUM_TYPE, ENUM_VALUE4);          \
            DENSITY_ENUM_RENDER_VALUE(ENUM_TYPE, ENUM_VALUE5);          \
            DENSITY_ENUM_RENDER_VALUE(ENUM_TYPE, ENUM_VALUE6);          \
        default: return #ENUM_TYPE "_???"; } }

#define DENSITY_ENUM_RENDER7(ENUM_TYPE, ENUM_VALUE1, ENUM_VALUE2, ENUM_VALUE3, ENUM_VALUE4, ENUM_VALUE5, ENUM_VALUE6, ENUM_VALUE7) \
    DENSITY_ENUM_RENDER_PROTOTYPE(ENUM_TYPE) {                          \
        switch (ENUM_TYPE) {                                            \
            DENSITY_ENUM_RENDER_VALUE(ENUM_TYPE, ENUM_VALUE1);          \
            DENSITY_ENUM_RENDER_VALUE(ENUM_TYPE, ENUM_VALUE2);          \
            DENSITY_ENUM_RENDER_VALUE(ENUM_TYPE, ENUM_VALUE3);          \
            DENSITY_ENUM_RENDER_VALUE(ENUM_TYPE, ENUM_VALUE4);          \
            DENSITY_ENUM_RENDER_VALUE(ENUM_TYPE, ENUM_VALUE5);          \
            DENSITY_ENUM_RENDER_VALUE(ENUM_TYPE, ENUM_VALUE6);          \
            DENSITY_ENUM_RENDER_VALUE(ENUM_TYPE, ENUM_VALUE7);          \
        default: return #ENUM_TYPE "_???"; } }

namespace density {
    const uint8_t major_version = 0;
    const uint8_t minor_version = 12;
    const uint8_t revision = 5;

    const uint_fast8_t dictionary_preferred_reset_cycle_shift = 6;
    const uint_fast64_t dictionary_preferred_reset_cycle =
        1 << dictionary_preferred_reset_cycle_shift;

    typedef enum {
        compression_mode_copy = 0,
        compression_mode_chameleon_algorithm = 1,
        compression_mode_cheetah_algorithm = 2,
        compression_mode_lion_algorithm = 3,
    } compression_mode_t;
    DENSITY_ENUM_RENDER4(compression_mode, copy, chameleon_algorithm,
                         cheetah_algorithm, lion_algorithm);
    typedef enum {
        block_type_default = 0,                      // Standard, no integrity check
        block_type_with_hashsum_integrity_check = 1  // Add data integrity check to the stream
    } block_type_t;
    DENSITY_ENUM_RENDER2(block_type, default, with_hashsum_integrity_check);

    typedef enum {
        buffer_state_ready = 0,
        buffer_state_error,
        buffer_state_error_on_input,
        buffer_state_error_on_output
    } buffer_state_t;
    DENSITY_ENUM_RENDER4(buffer_state, ready, error, error_on_input, error_on_output);

    typedef enum {
        encode_state_ready = 0,
        encode_state_error,
        encode_state_stall_on_input,
        encode_state_stall_on_output
    } encode_state_t;
    DENSITY_ENUM_RENDER4(encode_state, ready, error, stall_on_input, stall_on_output);

    typedef enum {
        decode_state_ready = 0,
        decode_state_error,
        decode_state_stall_on_input,
        decode_state_stall_on_output,
        decode_state_integrity_check_fail
    } decode_state_t;
    DENSITY_ENUM_RENDER5(decode_state, ready, error, stall_on_input, stall_on_output,
                         integrity_check_fail);
}
