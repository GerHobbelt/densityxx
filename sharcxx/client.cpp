// see LICENSE.md for license.
#include <stdarg.h>
#include <chrono>
#include "sharcxx/client.hpp"
#include "densityxx/file_buffer.hpp"
#include "densityxx/block.hpp"
#include "densityxx/context.hpp"
#include "densityxx/copy.hpp"
#include "densityxx/chameleon.hpp"
#include "densityxx/cheetah.hpp"
#include "densityxx/lion.hpp"

namespace density {
#ifdef SHARC_ALLOW_ANSI_ESCAPE_SEQUENCES
    static const char *sharc_esc_bold_start = "\33[1m";
    static const char *sharc_esc_red_start = "\33[1;31m";
    static const char *sharc_esc_end = "\33[0m";
#else
    static const char *sharc_esc_bold_start = "";
    static const char *sharc_esc_red_start = "";
    static const char *sharc_esc_end = "";
#endif

    static void version(void)
    {
        printf("%sDensityXX %u.%u.%u%s\n", sharc_esc_bold_start,
               (unsigned)density::major_version, (unsigned)density::minor_version,
               (unsigned)density::revision, sharc_esc_end);
        printf("Copyright (C) 2016 Charles Wang\n");
        printf("Copyright (C) 2013 Guillaume Voirin\n");
        printf("Built for %s (%s endian system, %u bits) using GCC %d.%d.%d, %s %s\n",
               sharc_platform_string, sharc_endian_string, (unsigned int)(8 * sizeof(void *)),
               __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__, __DATE__, __TIME__);
    }

    static void usage(const char *arg0)
    {
        version();
        printf("\nSuperfast compression\n\n");
        printf("%sUsage :%s\n", sharc_esc_bold_start, sharc_esc_end);
        printf("  %s [OPTIONS]... [FILES]...\n\n", arg0);
        printf("%sAvailable options :%s\n", sharc_esc_bold_start, sharc_esc_end);
        printf("  -c[LEVEL]   Compress files using LEVEL if specified (default)\n");
        printf("              LEVEL can have the following values (as values become higher,\n");
        printf("              compression ratio increases and speed diminishes) :\n");
        printf("              0 = No compression\n");
        printf("              1 = Chameleon algorithm (default)\n");
        printf("              2 = Cheetah algorithm\n");
        printf("              3 = Lion algorithm\n");
        printf("  -d          Decompress files\n");
        printf("  -p[PATH]    Set output path\n");
        printf("  -x          Add integrity check hashsum (use when compressing)\n");
        printf("  -f          Overwrite without prompting\n");
        printf("  -i          Read from stdin\n");
        printf("  -o          Write to stdout\n");
        printf("  -v          Display version information\n");
        printf("  -h          Display this help\n");
        exit(0);
    }

    typedef file_buffer_t<sharc_preferred_buffer_size,
                          sharc_preferred_buffer_size> sharc_file_buffer_t;

    static void
    exit_error(const char *message_format, ...)
    {
        va_list ap;
        va_start(ap, message_format);
        fprintf(stderr, "%sSharc error:%s ", sharc_esc_red_start, sharc_esc_end);
        vfprintf(stderr, message_format, ap);
        va_end(ap);
        exit(-1);
    }
    static void
    exit_error(const buffer_state_t buffer_state)
    {
        exit_error("%s\n", buffer_state_render(buffer_state).c_str());
    }
    static std::string
    format_decimal(uint64_t number)
    {
        char buf[128], *cur = buf, *last = buf + sizeof(buf);
        uint64_t mod = 1;
        while (number / mod > 1000) mod *= 1000;
        cur += snprintf(cur, last - buf, "%u", (unsigned)(number / mod));
        while (mod > 1) {
            mod /= 1000;
            cur += snprintf(cur, last - buf,
                            ",%03u", (unsigned)(number / mod % 1000));
        }
        return std::string(buf);
    }
    static FILE *
    check_open_file(const char *file_name, const char *options, const bool check_overwrite)
    {
        if (check_overwrite && access(file_name, F_OK) != -1) {
            printf("File %s already exists. Do you want to overwrite it (y/N) ? ", file_name);
            switch (getchar()) {
            case 'y': break;
            default:  exit(0);
            }
        }
        FILE *file = fopen(file_name, options);
        if (file == NULL) exit_error("Unable to open file %s.\n", file_name);
        return file;
    }

    template<class KERNEL_ENCODE_T>static DENSITY_INLINE uint32_t
    do_compress(context_t &context, sharc_file_buffer_t *buffer)
    {
        encode_state_t encode_state;
        buffer_state_t buffer_state;
        block_encode_t<KERNEL_ENCODE_T> *block_encode = new block_encode_t<KERNEL_ENCODE_T>();
        block_encode->init(context);
        while ((encode_state = context.after(block_encode->continue_(context.before()))))
            if ((buffer_state = buffer->action(encode_state, context)))
                exit_error(buffer_state);
            else if (buffer->get_last_read()) break;
        while ((encode_state = context.after(block_encode->finish(context.before()))))
            if ((buffer_state = buffer->action(encode_state, context)))
                exit_error(buffer_state);
        uint32_t relative_position = block_encode->read_bytes();
        delete block_encode;
        return relative_position;
    }
    void
    client_io_t::compress(client_io_t *const io_out,
                          const compression_mode_t attempt_mode,
                          const bool prompting, const bool integrity_checks,
                          const std::string &in_path, const std::string &out_path)
    {
        // determine in_file_path, out_file_path.
        struct stat attributes;
        std::string in_file_path, out_file_path;
        switch (origin_type) {
        case header_origin_type_stream:
            name = sharc_stdio;
            in_file_path = in_path + name;
            this->stream = stdin;
            break;
        case header_origin_type_file:
            in_file_path = in_path + name;
            this->stream = check_open_file(in_file_path.c_str(), "rb", false);
            stat(in_file_path.c_str(), &attributes);
            break;
        }
        switch (io_out->origin_type) {
        case header_origin_type_stream:
            io_out->name = sharc_stdio_compressed;
            out_file_path = out_path + io_out->name;
            io_out->stream = stdout;
            break;
        case header_origin_type_file:
            io_out->name = name + ".sharc";
            out_file_path = out_path + io_out->name;
            io_out->stream = check_open_file(out_file_path.c_str(), "wb", prompting);
            break;
        }

        std::chrono::system_clock::time_point tpstart = std::chrono::system_clock::now();
        /*
         * The following code is an example of
         * how to use the Density stream API to compress a file.
         */
        uint32_t relative_position;
        uint64_t total_written = header_t::write(io_out->stream, origin_type, &attributes);
        context_t context;
        encode_state_t encode_state;
        buffer_state_t buffer_state;
        sharc_file_buffer_t *buffer = new sharc_file_buffer_t(this->stream, io_out->stream);
        block_type_t block_type =
            integrity_checks ? block_type_with_hashsum_integrity_check: block_type_default;

        buffer->init(attempt_mode, block_type, context);
        if ((buffer_state = buffer->action(encode_state_stall_on_input, context)))
            exit_error(buffer_state);
        while ((encode_state = context.write_header()))
            if ((buffer_state = buffer->action(encode_state, context)))
                exit_error(buffer_state);
        switch (attempt_mode) {
        case compression_mode_copy:
            relative_position = do_compress<copy_encode_t>(context, buffer);
            break;
        case compression_mode_chameleon_algorithm:
            relative_position = do_compress<chameleon_encode_t>(context, buffer);
            break;
        case compression_mode_cheetah_algorithm:
            relative_position = do_compress<cheetah_encode_t>(context, buffer);
            break;
        case compression_mode_lion_algorithm:
            relative_position = do_compress<lion_encode_t>(context, buffer);
            break;
        }
        while ((encode_state = context.write_footer(relative_position)))
            if ((buffer_state = buffer->action(encode_state, context)))
                exit_error(buffer_state);
        if ((buffer_state = buffer->action(encode_state_stall_on_output, context)))
            exit_error(buffer_state);
        delete buffer;
        /*
         * That's it !
         */
        std::chrono::system_clock::time_point tpend = std::chrono::system_clock::now();
        if (io_out->origin_type == header_origin_type_file) {
            std::chrono::duration<double> duration = tpend - tpstart;
            const double elapsed = duration.count();
            total_written += context.get_total_written();
            fclose(io_out->stream);
            if (origin_type == header_origin_type_file) {
                uint64_t total_read = context.get_total_read();
                fclose(this->stream);
                double ratio = (100.0 * total_written) / total_read;
                double speed = (1.0 * total_read) / (elapsed * 1000.0 * 1000.0);
                printf("Compressed %s%s%s(%s bytes) to %s%s%s(%s bytes)",
                       sharc_esc_bold_start, in_file_path.c_str(), sharc_esc_end,
                       format_decimal(total_read).c_str(),
                       sharc_esc_bold_start, out_file_path.c_str(), sharc_esc_end,
                       format_decimal(total_written).c_str());
                printf(" %s %.1lf%% (User time %.3lfs %s %.0lf MB/s)\n",
                       sharc_arrow, ratio, elapsed, sharc_arrow, speed);
            } else {
                printf("Compressed %s%s%s to %s%s%s, %s bytes written.\n",
                       sharc_esc_bold_start, name.c_str(), sharc_esc_end,
                       sharc_esc_bold_start, out_file_path.c_str(), sharc_esc_end,
                       format_decimal(total_written).c_str());
            }
        }
    }

    template<class KERNEL_DECODE_T>static DENSITY_INLINE void
    do_decompress(context_t &context, sharc_file_buffer_t *buffer)
    {
        decode_state_t decode_state;
        buffer_state_t buffer_state;
        block_decode_t<KERNEL_DECODE_T> *block_decode = new block_decode_t<KERNEL_DECODE_T>();
        block_decode->init(context);
        while ((decode_state = context.after(block_decode->continue_(context.before()))))
            if ((buffer_state = buffer->action(decode_state, context)))
                exit_error(buffer_state);
            else if (buffer->get_last_read()) break;
        while ((decode_state = context.after(block_decode->finish(context.before()))))
            if ((buffer_state = buffer->action(decode_state, context)))
                exit_error(buffer_state);
        delete block_decode;
    }
    void
    client_io_t::decompress(client_io_t *const io_out, const bool prompting,
                            const std::string &in_path, const std::string &out_path)
    {
        // determine in_file_path, out_file_path.
        std::string in_file_path, out_file_path;
        switch (origin_type) {
        case header_origin_type_stream:
            name = sharc_stdio_compressed;
            in_file_path = in_path + name;
            this->stream = stdin;
            break;
        case header_origin_type_file:
            in_file_path = in_path + name;
            this->stream = check_open_file(in_file_path.c_str(), "rb", false);
            break;
        }
        switch (io_out->origin_type) {
        case header_origin_type_stream:
            io_out->name = sharc_stdio;
            out_file_path = out_path + io_out->name;
            io_out->stream = stdout;
            break;
        case header_origin_type_file:
            if (name.size() <= 6 || name.substr(name.size() - 6, 6) != ".sharc")
                exit_error("filename must terminated with '.sharc'.\n");
            io_out->name = name.substr(0, name.size() - 6);
            out_file_path = out_path + io_out->name;
            io_out->stream = check_open_file(out_file_path.c_str(), "wb", prompting);
            break;
        }

        std::chrono::system_clock::time_point tpstart = std::chrono::system_clock::now();
        /*
         * The following code is an example of
         * how to use the Density stream API to decompress a file.
         */
        header_t header;
        uint64_t total_read = header.read(this->stream);
        if (!header.check_validity()) exit_error("Invalid file.\n");
        context_t context;
        decode_state_t decode_state;
        buffer_state_t buffer_state;
        sharc_file_buffer_t *buffer = new sharc_file_buffer_t(this->stream, io_out->stream);

        buffer->init(compression_mode_copy, block_type_default, context);
        if ((buffer_state = buffer->action(decode_state_stall_on_input, context)))
            exit_error(buffer_state);
        while ((decode_state = context.read_header()))
            if ((buffer_state = buffer->action(decode_state, context)))
                exit_error(buffer_state);
        switch (context.header.compression_mode()) {
        case compression_mode_copy:
            do_decompress<copy_decode_t>(context, buffer);
            break;
        case compression_mode_chameleon_algorithm:
            do_decompress<chameleon_decode_t>(context, buffer);
            break;
        case compression_mode_cheetah_algorithm:
            do_decompress<cheetah_decode_t>(context, buffer);
            break;
        case compression_mode_lion_algorithm:
            do_decompress<lion_decode_t>(context, buffer);
            break;
        }
        while ((decode_state = context.read_footer()))
            if ((buffer_state = buffer->action(decode_state, context)))
                exit_error(buffer_state);
        if ((buffer_state = buffer->action(decode_state_stall_on_output, context)))
            exit_error(buffer_state);
        delete buffer;
        /*
         * That's it !
         */
        std::chrono::system_clock::time_point tpend = std::chrono::system_clock::now();
        if (io_out->origin_type == header_origin_type_file) {
            std::chrono::duration<double> duration = tpend - tpstart;
            const double elapsed = duration.count();
            uint64_t total_written = context.get_total_written();
            fclose(io_out->stream);
            if (header.origin_type() == header_origin_type_file)
                header.restore_file_attributes(out_file_path.c_str());
            if (origin_type == header_origin_type_file) {
                total_read += context.get_total_read();
                fclose(this->stream);
                if (header.origin_type() == header_origin_type_file &&
                    total_written != header.original_file_size())
                    exit_error("Input file is corrupt(%llu != %llu)!\n",
                               (unsigned long long)total_written,
                               (unsigned long long)header.original_file_size());
                double ratio = (100.0 * total_written) / total_read;
                double speed = (1.0 * total_written) / (elapsed * 1000.0 * 1000.0);
                printf("Decompressed %s%s%s(%s bytes) to %s%s%s(%s bytes)",
                       sharc_esc_bold_start, in_file_path.c_str(), sharc_esc_end,
                       format_decimal(total_read).c_str(),
                       sharc_esc_bold_start, out_file_path.c_str(), sharc_esc_end,
                       format_decimal(total_written).c_str());
                printf(" %s %.1lf%% (User time %.3lfs %s %.0lf MB/s)\n",
                       sharc_arrow, ratio, elapsed, sharc_arrow, speed);
            } else {
                printf("Decompressed %s%s%s to %s%s%s, %s bytes written.\n",
                       sharc_esc_bold_start, name.c_str(), sharc_esc_end,
                       sharc_esc_bold_start, out_file_path.c_str(), sharc_esc_end,
                       format_decimal(total_written).c_str());
            }
        }
    }
}

int
main(int argc, char *argv[])
{
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
    _setmode(_fileno(stderr), _O_BINARY);
#endif
    if (argc <= 1) density::usage(argv[0]);

    density::sharc_action_t action = density::sharc_action_compress;
    density::compression_mode_t mode = density::compression_mode_chameleon_algorithm;
    bool prompting = true;
    bool integrity_checks = false;
    density::client_io_t in;
    density::client_io_t out;
    bool path_mode = density::sharc_file_output_path;
    std::string in_path, out_path;
    size_t arg_length;

    for (int idx = 1; idx < argc; idx++) {
        switch (argv[idx][0]) {
        case '-':
            arg_length = strlen(argv[idx]);
            if (arg_length < 2) density::usage(argv[0]);
            switch (argv[idx][1]) {
            case 'c':
                if (arg_length == 2) break;
                if (arg_length != 3) density::usage(argv[0]);
                switch (argv[idx][2] - '0') {
                case 0: mode = density::compression_mode_copy; break;
                case 1: mode = density::compression_mode_chameleon_algorithm; break;
                case 2: mode = density::compression_mode_cheetah_algorithm; break;
                case 3: mode = density::compression_mode_lion_algorithm; break;
                default: density::usage(argv[0]);
                }
                break;
            case 'd': action = density::sharc_action_decompress; break;
            case 'p':
                if (arg_length == 2) density::usage(argv[0]);
                else {
                    const char *lastsep = strrchr(argv[idx], density::sharc_path_separator);
                    if (lastsep == NULL) {
                        out_path = ""; out.name = argv[idx] + 2;
                    } else {
                        out_path = std::string(argv[idx] + 2, lastsep - argv[idx] + 1 - 2);
                        out.name = lastsep + 1;
                    }
                    path_mode = density::sharc_fixed_output_path;
                }
                break;
            case 'f': prompting = false; break;
            case 'x': integrity_checks = true; break;
            case 'i': in.origin_type = density::header_origin_type_stream; break;
            case 'o': out.origin_type = density::header_origin_type_stream; break;
            case 'v': density::version(); exit(0);
            case 'h': density::usage(argv[0]); break;
            default: break;
            }
            break;
        default:
            if (in.origin_type == density::header_origin_type_file) {
                char *lastsep = strrchr(argv[idx], density::sharc_path_separator);
                if (lastsep == NULL) {
                    in_path = "";  in.name = argv[idx];
                } else {
                    in_path = std::string(argv[idx] + 2, lastsep - argv[idx] + 1 - 2);
                    in.name = lastsep + 1;
                    if (path_mode == density::sharc_file_output_path) out_path = in_path;
                }
            }
            switch (action) {
            case density::sharc_action_compress:
                in.compress(&out, mode, prompting, integrity_checks, in_path, out_path);
                break;
            case density::sharc_action_decompress:
                in.decompress(&out, prompting, in_path, out_path);
                break;
            }
            break;
        }
    }
    if (in.origin_type == density::header_origin_type_stream) {
        switch (action) {
        case density::sharc_action_compress:
            in.compress(&out, mode, prompting, integrity_checks, in_path, out_path);
            break;
        case density::sharc_action_decompress:
            in.decompress(&out, prompting, in_path, out_path);
            break;
        }
    }
    return 0;
}
