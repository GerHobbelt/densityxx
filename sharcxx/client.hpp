// see LICENSE.md for license.
#pragma once

#include "densityxx/api.hpp"
#include "sharcxx/header.hpp"

#define SHARC_ACTION_COMPRESS         0
#define SHARC_ACTION_DECOMPRESS       1

#define SHARC_NO_PROMPTING            false
#define SHARC_PROMPTING               true

#define SHARC_INTEGRITY_CHECKS        true
#define SHARC_NO_INTEGRITY_CHECK      false

#define SHARC_STDIN                   "stdin"
#define SHARC_STDOUT                  "stdout"
#define SHARC_STDIN_COMPRESSED        "stdin.sharc"

#define SHARC_OUTPUT_PATH_MAX_SIZE    256
#define SHARC_FILE_OUTPUT_PATH        false
#define SHARC_FIXED_OUTPUT_PATH       true

#define SHARC_PREFERRED_BUFFER_SIZE   (1 << 19)

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define SHARC_ENDIAN_STRING           "Little"
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define SHARC_ENDIAN_STRING           "Big"
#endif

#if defined(_WIN64) || defined(_WIN32)
#define SHARC_PATH_SEPARATOR          '\\'
#else
#define SHARC_PATH_SEPARATOR          '/'
#define SHARC_ALLOW_ANSI_ESCAPE_SEQUENCES
#endif

#if defined(_WIN64) || defined(_WIN32)
#define SHARC_ARROW          "->"
#else
#define SHARC_ARROW          "➔"
#endif

#if defined(_WIN64) || defined(_WIN32)
#define SHARC_PLATFORM_STRING         "Microsoft Windows"
#elif defined(__APPLE__)
#include "TargetConditionals.h"
#include "globals.h"
#if TARGET_IPHONE_SIMULATOR
#define SHARC_PLATFORM_STRING         "iOS Simulator"
#elif TARGET_OS_IPHONE
#define SHARC_PLATFORM_STRING         "iOS"
#elif TARGET_OS_MAC
#define SHARC_PLATFORM_STRING         "Mac OS/X"
#else
#define SHARC_PLATFORM_STRING         "an unknown Apple platform"
#endif
#elif defined(__FreeBSD__)
#define SHARC_PLATFORM_STRING         "FreeBSD"
#elif defined(__linux__)
#define SHARC_PLATFORM_STRING         "GNU/Linux"
#elif defined(__unix__)
#define SHARC_PLATFORM_STRING         "Unix"
#elif defined(__posix__)
#define SHARC_PLATFORM_STRING         "Posix"
#else
#define SHARC_PLATFORM_STRING         "an unknown platform"
#endif

namespace density {
    class client_io_t {
    public:
        char *name;
        FILE *stream;
        header_origin_type_t origin_type;

        FILE* check_open_file(const char*, const char*, const bool);
        static void version(void);
        static void usage(void);
        void compress(client_io_t * const, const compression_mode_t,
                      const bool, const bool, const char*, const char*);
        void decompress(client_io_t * const, const bool, const char*, const char*);
    private:
        void exit_error(const char *message);
        void format_decimal(uint64_t number);
        uint_fast64_t reload_input_buffer(const client_io_t *RESTRICT io_in);
    };
}
