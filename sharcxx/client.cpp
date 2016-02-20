// see LICENSE.md for license.
#include "sharcxx/client.hpp"
#include "sharcxx/chrono.hpp"

#ifdef SHARC_ALLOW_ANSI_ESCAPE_SEQUENCES
#define SHARC_ESC_BOLD_START    "\33[1m"
#define SHARC_ESC_RED_START     "\33[1;31m"
#define SHARC_ESC_END           "\33[0m"
#else
#define SHARC_ESC_BOLD_START    ""
#define SHARC_ESC_RED_START     ""
#define SHARC_ESC_END           ""
#endif

static void version(void)
{
    printf("%sDensityXX %i.%i.%i%s\n", SHARC_ESC_BOLD_START,
           DENSITYXX_MAJOR_VERSION, DENSITYXX_MINOR_VERSION, DENSITYXX_REVISION,
           SHARC_ESC_END);
    printf("Copyright (C) 2016 Charles Wang\n");
    printf("Copyright (C) 2013 Guillaume Voirin\n");
    printf("Built for %s (%s endian system, %u bits) using GCC %d.%d.%d, %s %s\n",
           SHARC_PLATFORM_STRING, SHARC_ENDIAN_STRING, (unsigned int)(8 * sizeof(void *)),
           __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__, __DATE__, __TIME__);
}

static void usage(const char *arg0)
{
    version();
    printf("\nSuperfast compression\n\n");
    printf("%sUsage :%s\n", SHARC_ESC_BOLD_START, SHARC_ESC_END);
    printf("  %s [OPTIONS]... [FILES]...\n\n", arg0);
    printf("%sAvailable options :%s\n", SHARC_ESC_BOLD_START, SHARC_ESC_END);
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

namespace density {
    static uint8_t input_buffer[SHARC_PREFERRED_BUFFER_SIZE];
    static uint8_t output_buffer[SHARC_PREFERRED_BUFFER_SIZE];

    static void
    exit_error(const char *message)
    {
        fprintf(stderr, "%sSharc error:%s %s\n",
                SHARC_ESC_RED_START, SHARC_ESC_END, message);
        exit(0);
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

    uint_fast64_t
    client_io_t::reload_input_buffer(stream_t *RESTRICT stream) const
    {
        uint_fast64_t read = (uint_fast64_t)
            fread(input_buffer, sizeof(uint8_t), SHARC_PREFERRED_BUFFER_SIZE, this->stream);
        stream->update_input(input_buffer, read);
        if (read < SHARC_PREFERRED_BUFFER_SIZE)
            if (ferror(this->stream))
                exit_error("Error reading file");
        return read;
    }

    uint_fast64_t
    client_io_t::empty_output_buffer(stream_t *RESTRICT stream) const
    {
        uint_fast64_t available = stream->output_available_for_use();
        uint_fast64_t written = (uint_fast64_t)
            fwrite(output_buffer, sizeof(uint8_t), (size_t)available, this->stream);
        if (written < available)
            if (ferror(this->stream))
                exit_error("Error writing file");
        stream->update_output(output_buffer, SHARC_PREFERRED_BUFFER_SIZE);
        return written;
    }

    void
    client_io_t::action_required(uint_fast64_t *read, uint_fast64_t *written,
                                 const client_io_t *RESTRICT io_out,
                                 stream_t *RESTRICT stream,
                                 const stream_state_t stream_state,
                                 const char *error_message)
    {
        switch (stream_state) {
        case stream_state_stall_on_output:
            *written = io_out->empty_output_buffer(stream);
            break;
        case stream_state_stall_on_input:
            *read = reload_input_buffer(stream);
            break;
        case stream_state_error_integrity_check_fail:
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
            name = SHARC_STDIO;
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
            io_out->name = SHARC_STDIO_COMPRESSED;
            out_file_path = out_path + io_out->name;
            io_out->stream = stdout;
            break;
        case header_origin_type_file:
            io_out->name = name + ".sharc";
            out_file_path = out_path + io_out->name;
            io_out->stream = check_open_file(out_file_path.c_str(), "wb", prompting);
            break;
        }

        chrono_t chrono;
        chrono.start();
        /*
         * The following code is an example of
         * how to use the Density stream API to compress a file.
         */
        uint64_t total_written = header_t::write(io_out->stream, origin_type, &attributes);
        stream_t *stream = new stream_t();
        stream_state_t stream_state;
        uint_fast64_t read = 0, written = 0;
        if (stream->prepare(input_buffer, sizeof(input_buffer),
                            output_buffer, sizeof(output_buffer)))
            exit_error("Unable to prepare compression");
        read = reload_input_buffer(stream);
        block_type_t block_type =
            integrity_checks ? block_type_with_hashsum_integrity_check: block_type_default;
        while ((stream_state = stream->compress_init(attempt_mode, block_type)))
            action_required(&read, &written, io_out, stream, stream_state,
                            "Unable to initialize compression");
        while ((read == SHARC_PREFERRED_BUFFER_SIZE) &&
               (stream_state = stream->compress_continue()))
            action_required(&read, &written, io_out, stream, stream_state,
                            "An error occured during compression");
        while ((stream_state = stream->compress_finish()))
            action_required(&read, &written, io_out, stream, stream_state,
                            "An error occured while finishing compression");
        io_out->empty_output_buffer(stream);
        /*
         * That's it !
         */
        chrono.stop();
        if (io_out->origin_type == header_origin_type_file) {
            const double elapsed = chrono.elapsed();
            total_written += *stream->total_bytes_written;
            fclose(io_out->stream);
            if (origin_type == header_origin_type_file) {
                uint64_t total_read = *stream->total_bytes_read;
                fclose(this->stream);
                double ratio = (100.0 * total_written) / total_read;
                double speed = (1.0 * total_read) / (elapsed * 1000.0 * 1000.0);
                printf("Compressed %s%s%s(%s bytes) to %s%s%s(%s bytes)",
                       SHARC_ESC_BOLD_START, in_file_path.c_str(), SHARC_ESC_END,
                       format_decimal(total_read).c_str(),
                       SHARC_ESC_BOLD_START, out_file_path.c_str(), SHARC_ESC_END,
                       format_decimal(total_written).c_str());
                printf(" %s %.1lf%% (User time %.3lfs %s %.0lf MB/s)\n",
                       SHARC_ARROW, ratio, elapsed, SHARC_ARROW, speed);
            } else {
                printf("Compressed %s%s%s to %s%s%s, %s bytes written.\n",
                       SHARC_ESC_BOLD_START, name.c_str(), SHARC_ESC_END,
                       SHARC_ESC_BOLD_START, out_file_path.c_str(), SHARC_ESC_END,
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
            name = SHARC_STDIO_COMPRESSED;
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
            io_out->name = SHARC_STDIO;
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

        chrono_t chrono;
        chrono.start();
        /*
         * The following code is an example of
         * how to use the Density stream API to decompress a file.
         */
        header_t header;
        uint64_t total_read = header.read(this->stream);
        if (!header.check_validity()) exit_error("Invalid file");
        stream_t *stream = new stream_t();
        stream_state_t stream_state;
        uint_fast64_t read = 0, written = 0;
        if (stream->prepare(input_buffer, sizeof(input_buffer),
                            output_buffer, sizeof(output_buffer)))
            exit_error("Unable to prepare decompression");
        read = reload_input_buffer(stream);
        while ((stream_state = stream->decompress_init(NULL)))
            action_required(&read, &written, io_out, stream, stream_state,
                            "Unable to initialize decompression");
        while ((read == SHARC_PREFERRED_BUFFER_SIZE) &&
               (stream_state = stream->decompress_continue()))
            action_required(&read, &written, io_out, stream, stream_state,
                            "An error occured during decompression");
        while ((stream_state = stream->decompress_finish()))
            action_required(&read, &written, io_out, stream, stream_state,
                            "An error occured while finishing decompression");
        io_out->empty_output_buffer(stream);
        /*
         * That's it !
         */
        chrono.stop();
        if (io_out->origin_type == header_origin_type_file) {
            const double elapsed = chrono.elapsed();
            uint64_t total_written = *stream->total_bytes_written;
            fclose(io_out->stream);
            if (header.origin_type() == header_origin_type_file)
                header.restore_file_attributes(out_file_path.c_str());
            if (origin_type == header_origin_type_file) {
                total_read += *stream->total_bytes_read;
                fclose(this->stream);
                if (header.origin_type() == header_origin_type_file) {
                    if (total_written != header.original_file_size())
                        exit_error("Input file is corrupt !");
                }
                double ratio = (100.0 * total_written) / total_read;
                double speed = (1.0 * total_written) / (elapsed * 1000.0 * 1000.0);
                printf("Decompressed %s%s%s(%s bytes) to %s%s%s(%s bytes)",
                       SHARC_ESC_BOLD_START, in_file_path.c_str(), SHARC_ESC_END,
                       format_decimal(total_read).c_str(),
                       SHARC_ESC_BOLD_START, out_file_path.c_str(), SHARC_ESC_END,
                       format_decimal(total_written).c_str());
                printf(" %s %.1lf%% (User time %.3lfs %s %.0lf MB/s)\n",
                       SHARC_ARROW, ratio, elapsed, SHARC_ARROW, speed);
            } else {
                printf("Decompressed %s%s%s to %s%s%s, %s bytes written.\n",
                       SHARC_ESC_BOLD_START, name.c_str(), SHARC_ESC_END,
                       SHARC_ESC_BOLD_START, out_file_path.c_str(), SHARC_ESC_END,
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
    if (argc <= 1) usage(argv[0]);

    uint8_t action = SHARC_ACTION_COMPRESS;
    density::compression_mode_t mode = density::compression_mode_chameleon_algorithm;
    bool prompting = SHARC_PROMPTING;
    bool integrity_checks = SHARC_NO_INTEGRITY_CHECK;
    density::client_io_t in;
    density::client_io_t out;
    uint8_t path_mode = SHARC_FILE_OUTPUT_PATH;
    std::string in_path, out_path;
    size_t arg_length;

    for (int idx = 1; idx < argc; idx++) {
        switch (argv[idx][0]) {
        case '-':
            arg_length = strlen(argv[idx]);
            if (arg_length < 2) usage(argv[0]);
            switch (argv[idx][1]) {
            case 'c':
                if (arg_length == 2) break;
                if (arg_length != 3) usage(argv[0]);
                switch (argv[idx][2] - '0') {
                case 0: mode = density::compression_mode_copy; break;
                case 1: mode = density::compression_mode_chameleon_algorithm; break;
                case 2: mode = density::compression_mode_cheetah_algorithm; break;
                case 3: mode = density::compression_mode_lion_algorithm; break;
                default: usage(argv[0]);
                }
                break;
            case 'd': action = SHARC_ACTION_DECOMPRESS; break;
            case 'p':
                if (arg_length == 2) usage(argv[0]);
                else {
                    const char *lastsep = strrchr(argv[idx], SHARC_PATH_SEPARATOR);
                    if (lastsep == NULL) {
                        out_path = ""; out.name = argv[idx] + 2;
                    } else {
                        out_path = std::string(argv[idx] + 2, lastsep - argv[idx] + 1 - 2);
                        out.name = lastsep + 1;
                    }
                    path_mode = SHARC_FIXED_OUTPUT_PATH;
                }
                break;
            case 'f': prompting = SHARC_NO_PROMPTING; break;
            case 'x': integrity_checks = SHARC_INTEGRITY_CHECKS; break;
            case 'i': in.origin_type = density::header_origin_type_stream; break;
            case 'o': out.origin_type = density::header_origin_type_stream; break;
            case 'v': version(); exit(0);
            case 'h': usage(argv[0]); break;
            default: break;
            }
            break;
        default:
            if (in.origin_type == density::header_origin_type_file) {
                char *lastsep = strrchr(argv[idx], SHARC_PATH_SEPARATOR);
                if (lastsep == NULL) {
                    in_path = "";  in.name = argv[idx];
                } else {
                    in_path = std::string(argv[idx] + 2, lastsep - argv[idx] + 1 - 2);
                    in.name = lastsep + 1;
                    if (path_mode == SHARC_FILE_OUTPUT_PATH) out_path = in_path;
                }
            }
            switch (action) {
            case SHARC_ACTION_DECOMPRESS:
                in.decompress(&out, prompting, in_path, out_path);
                break;
            default:
                in.compress(&out, mode, prompting, integrity_checks, in_path, out_path);
                break;
            }
            break;
        }
    }
    if (in.origin_type == density::header_origin_type_stream) {
        switch (action) {
        case SHARC_ACTION_COMPRESS:
            in.compress(&out, mode, prompting, integrity_checks, in_path, out_path);
            break;
        case SHARC_ACTION_DECOMPRESS:
            in.decompress(&out, prompting, in_path, out_path);
            break;
        }
    }
    return true;
}
