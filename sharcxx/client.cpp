// see LICENSE.md for license.
#include "sharcxx/client.hpp"
#include <chrono>
#include <stdarg.h>

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

    static uint8_t input_buffer[sharc_preferred_buffer_size];
    static uint8_t output_buffer[sharc_preferred_buffer_size];

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
        if (file == NULL) {
            char message[512];
            sprintf(message, "Unable to open file %s", file_name);
            exit_error(message);
        }
        return file;
    }

    inline uint_fast64_t
    client_io_t::reload_input_buffer(stream_t *RESTRICT stream) const
    {
        uint_fast64_t read = (uint_fast64_t)
            fread(input_buffer, sizeof(uint8_t), sizeof(input_buffer), this->stream);
        stream->update_input(input_buffer, read);
        if (read < sizeof(input_buffer) && ferror(this->stream))
            exit_error("Error reading file");
        return read;
    }

    inline uint_fast64_t
    client_io_t::empty_output_buffer(stream_t *RESTRICT stream) const
    {
        uint_fast64_t available = stream->output_available_for_use();
        uint_fast64_t written = (uint_fast64_t)
            fwrite(output_buffer, sizeof(uint8_t), (size_t)available, this->stream);
        if (written < available && ferror(this->stream))
            exit_error("Error writing file");
        stream->update_output(output_buffer, sizeof(output_buffer));
        return written;
    }

    inline void
    client_io_t::action_required(uint_fast64_t *read, uint_fast64_t *written,
                                 const client_io_t *RESTRICT io_out,
                                 stream_t *RESTRICT stream,
                                 const stream_t::state_t stream_state,
                                 const char *error_message)
    {
        switch (stream_state) {
        case stream_t::state_stall_on_output:
            *written = io_out->empty_output_buffer(stream);
            break;
        case stream_t::state_stall_on_input:
            *read = reload_input_buffer(stream);
            break;
        case stream_t::state_error_integrity_check_fail:
            exit_error("Integrity check failed");
        default:
            exit_error(error_message);
        }
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
        uint64_t total_written = header_t::write(io_out->stream, origin_type, &attributes);
        stream_encode_t *stream = new stream_encode_t();
        stream_t::state_t stream_state;
        uint_fast64_t read = 0, written = 0;
        if (stream->prepare(input_buffer, sizeof(input_buffer),
                            output_buffer, sizeof(output_buffer)))
            exit_error("Unable to prepare compression");
        read = reload_input_buffer(stream);
        block_type_t block_type =
            integrity_checks ? block_type_with_hashsum_integrity_check: block_type_default;
        while ((stream_state = stream->init(attempt_mode, block_type)))
            action_required(&read, &written, io_out, stream, stream_state,
                            "Unable to initialize compression");
        while ((read == sharc_preferred_buffer_size) &&
               (stream_state = stream->continue_()))
            action_required(&read, &written, io_out, stream, stream_state,
                            "An error occured during compression");
        while ((stream_state = stream->finish()))
            action_required(&read, &written, io_out, stream, stream_state,
                            "An error occured while finishing compression");
        io_out->empty_output_buffer(stream);
        /*
         * That's it !
         */
        std::chrono::system_clock::time_point tpend = std::chrono::system_clock::now();
        if (io_out->origin_type == header_origin_type_file) {
            std::chrono::duration<double> duration = tpend - tpstart;
            const double elapsed = duration.count();
            total_written += stream->get_total_written();
            fclose(io_out->stream);
            if (origin_type == header_origin_type_file) {
                uint64_t total_read = stream->get_total_read();
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
        delete stream;
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
                exit_error("filename must terminated with '.sharc'.");
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
        if (!header.check_validity()) exit_error("Invalid file");
        stream_decode_t *stream = new stream_decode_t();
        stream_t::state_t stream_state;
        uint_fast64_t read = 0, written = 0;
        if (stream->prepare(input_buffer, sizeof(input_buffer),
                            output_buffer, sizeof(output_buffer)))
            exit_error("Unable to prepare decompression");
        read = reload_input_buffer(stream);
        while ((stream_state = stream->init(NULL)))
            action_required(&read, &written, io_out, stream, stream_state,
                            "Unable to initialize decompression");
        while ((read == sharc_preferred_buffer_size) &&
               (stream_state = stream->continue_()))
            action_required(&read, &written, io_out, stream, stream_state,
                            "An error occured during decompression");
        while ((stream_state = stream->finish()))
            action_required(&read, &written, io_out, stream, stream_state,
                            "An error occured while finishing decompression");
        io_out->empty_output_buffer(stream);
        /*
         * That's it !
         */
        std::chrono::system_clock::time_point tpend = std::chrono::system_clock::now();
        if (io_out->origin_type == header_origin_type_file) {
            std::chrono::duration<double> duration = tpend - tpstart;
            const double elapsed = duration.count();
            uint64_t total_written = stream->get_total_written();
            fclose(io_out->stream);
            if (header.origin_type() == header_origin_type_file)
                header.restore_file_attributes(out_file_path.c_str());
            if (origin_type == header_origin_type_file) {
                total_read += stream->get_total_read();
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
        delete stream;
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
    bool integrity_checks = true;
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
