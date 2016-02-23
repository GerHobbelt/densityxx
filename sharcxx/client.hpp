// see LICENSE.md for license.
#pragma once

#include "densityxx/api.hpp"
#include "sharcxx/header.hpp"

namespace density {
    typedef enum { sharc_action_compress, sharc_action_decompress } sharc_action_t;
    const size_t sharc_preferred_buffer_size = 1 << 19;

    const char *sharc_stdio = "stdio";
    const char *sharc_stdio_compressed ="stdio.sharc";

    const bool sharc_file_output_path = false;
    const bool sharc_fixed_output_path = true;

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    const char *sharc_endian_string = "Little";
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    const char *sharc_endian_string = "Big";
#endif

#if defined(_WIN64) || defined(_WIN32)
    const char sharc_path_separator = '\\';
#else
    const char sharc_path_separator = '/';
#define SHARC_ALLOW_ANSI_ESCAPE_SEQUENCES
#endif

#if defined(_WIN64) || defined(_WIN32)
    const char *sharc_arrow = "->";
#else
    const char *sharc_arrow = "➔";
#endif

#if defined(_WIN64) || defined(_WIN32)
    const char *sharc_platform_string = "Microsoft Windows";
#elif defined(__APPLE__)
#include "TargetConditionals.h"
#include "globals.h"
#if TARGET_IPHONE_SIMULATOR
    const char *sharc_platform_string = "iOS Simulator";
#elif TARGET_OS_IPHONE
    const char *sharc_platform_string = "iOS";
#elif TARGET_OS_MAC
    const char *sharc_platform_string = "Mac OS/X";
#else
    const char *sharc_platform_string = "an unknown Apple platform";
#endif
#elif defined(__FreeBSD__)
    const char *sharc_platform_string = "FreeBSD";
#elif defined(__linux__)
    const char *sharc_platform_string = "GNU/Linux";
#elif defined(__unix__)
    const char *sharc_platform_string = "Unix";
#elif defined(__posix__)
    const char *sharc_platform_string = "Posix";
#else
    const char *sharc_platform_string = "an unknown platform";
#endif

    class client_io_t {
    public:
        std::string name;
        FILE *stream;
        header_origin_type_t origin_type;

        inline client_io_t(void)
        {   name = ""; stream = NULL; origin_type = header_origin_type_file; }
        void compress(client_io_t * const, const compression_mode_t,
                      const bool, const bool,
                      const std::string &, const std::string &);
        void decompress(client_io_t * const, const bool,
                        const std::string &, const std::string &);
    private:
        uint_fast64_t reload_input_buffer(stream_t *RESTRICT stream) const;
        uint_fast64_t empty_output_buffer(stream_t *RESTRICT stream) const;
        void action_required(uint_fast64_t *read, uint_fast64_t *written,
                             const client_io_t *RESTRICT io_out, stream_t *RESTRICT stream,
                             const stream_state_t stream_state, const char *error_message);
    };
}
