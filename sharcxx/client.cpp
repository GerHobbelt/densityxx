// see LICENSE.md for license.
#include "sharcxx/client.hpp"

namespace density {
    static uint8_t input_buffer[SHARC_PREFERRED_BUFFER_SIZE];
    static uint8_t output_buffer[SHARC_PREFERRED_BUFFER_SIZE];

    void
    client_io_t::exit_error(const char *message)
    {
#ifdef SHARC_ALLOW_ANSI_ESCAPE_SEQUENCES
        fprintf(stderr, "%c[1;31m", SHARC_ESCAPE_CHARACTER);
#endif
        fprintf(stderr, "Sharc error");
#ifdef SHARC_ALLOW_ANSI_ESCAPE_SEQUENCES
        fprintf(stderr, "%c[0m", SHARC_ESCAPE_CHARACTER);
#endif
        fprintf(stderr, " : %s\n", message);
        exit(0);
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
            sharc_client_exit_error(message);
        }
        return file;
    }

    void
    client_io_t::version(void)
    {
#ifdef SHARC_ALLOW_ANSI_ESCAPE_SEQUENCES
        printf("%c[1m", SHARC_ESCAPE_CHARACTER);
#endif
        printf("DensityXX %i.%i.%i",
               DENSITYXX_MAJOR_VERSION, DENSITYXX_MINOR_VERSION, DENSITYXX_REVISION);
#ifdef SHARC_ALLOW_ANSI_ESCAPE_SEQUENCES
        printf("%c[0m", SHARC_ESCAPE_CHARACTER);
#endif
        printf("Copyright (C) 2016 Charles Wang\n");
        printf("Copyright (C) 2013 Guillaume Voirin\n");
        printf("Built for %s (%s endian system, %u bits) using GCC %d.%d.%d, %s %s\n",
               SHARC_PLATFORM_STRING, SHARC_ENDIAN_STRING,
               (unsigned int)(8 * sizeof(void *)),
               __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__,
               __DATE__, __TIME__);
    }

    void
    client_io_t::usage(void)
    {
        version();
        printf("\nSuperfast compression\n\n");
#ifdef SHARC_ALLOW_ANSI_ESCAPE_SEQUENCES
        printf("%c[1m", SHARC_ESCAPE_CHARACTER);
#endif
        printf("Usage :\n");
#ifdef SHARC_ALLOW_ANSI_ESCAPE_SEQUENCES
        printf("%c[0m", SHARC_ESCAPE_CHARACTER);
#endif
        printf("  sharc [OPTIONS]... [FILES]...\n\n");
#ifdef SHARC_ALLOW_ANSI_ESCAPE_SEQUENCES
        printf("%c[1m", SHARC_ESCAPE_CHARACTER);
#endif
        printf("Available options :\n");
#ifdef SHARC_ALLOW_ANSI_ESCAPE_SEQUENCES
        printf("%c[0m", SHARC_ESCAPE_CHARACTER);
#endif
        printf("  -c[LEVEL], --compress[=LEVEL]     Compress files using LEVEL if specified (default)\n");
        printf("                                    LEVEL can have the following values (as values become higher,\n");
        printf("                                    compression ratio increases and speed diminishes) :\n");
        printf("                                    0 = No compression\n");
        printf("                                    1 = Chameleon algorithm (default)\n");
        printf("                                    2 = Cheetah algorithm\n");
        printf("                                    3 = Lion algorithm\n");
        printf("  -d, --decompress                  Decompress files\n");
        printf("  -p[PATH], --output-path[=PATH]    Set output path\n");
        printf("  -x, --check-integrity             Add integrity check hashsum (use when compressing)\n");
        printf("  -f, --no-prompt                   Overwrite without prompting\n");
        printf("  -i, --stdin                       Read from stdin\n");
        printf("  -o, --stdout                      Write to stdout\n");
        printf("  -v, --version                     Display version information\n");
        printf("  -h, --help                        Display this help\n");
        exit(0);
    }

    void
    client_io_t::format_decimal(uint64_t number)
    {
        if (number < 1000) {
            printf("%"PRIu64, number);
            return;
        }
        client_io_t::format_decimal(number / 1000);
        printf(",%03"PRIu64, number % 1000);
    }

    uint_fast64_t
    client_io_t::reload_input_buffer(const stream_t *RESTRICT stream)
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
    client_io_t::empty_output_buffer(stream_t *RESTRICT stream)
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
                          const bool prompting,
                          const bool integrity_checks,
                          const char *in_path, const char *out_path)
    {
        struct stat attributes;
        const size_t in_file_name_length = strlen(name);
        const size_t out_file_name_length = in_file_name_length + 6;
        io_out->name = (char *)malloc((out_file_name_length + 1) * sizeof(char));
        sprintf(io_out->name, "%s.sharc", io_in->name);
        char in_file_path[strlen(in_path) + in_file_name_length + 1];
        const size_t out_file_path_length = strlen(out_path) + out_file_name_length;
        char out_file_path[(out_file_name_length > strlen(SHARC_STDIN_COMPRESSED) ?
                            out_file_path_length :
                            strlen(out_path) + strlen(SHARC_STDIN_COMPRESSED)) + 1];
        sprintf(in_file_path, "%s%s", in_path, name);
        if (origin_type == header_origin_type_file) {
            if (io_out->origin_type == header_origin_type_file)
                sprintf(out_file_path, "%s%s", out_path, io_out->name);
            stream = check_open_file(in_file_path, "rb", false);
            stat(in_file_path, &attributes);
        } else {
            if (io_out->origin_type == header_origin_type_file)
                sprintf(out_file_path, "%s%s", out_path, SHARC_STDIN_COMPRESSED);
            stream = stdin;
            name = SHARC_STDIN;
        }
        if (io_out->origin_type == header_origin_type_file)
            io_out->stream = check_open_file(out_file_path, "wb", prompting);
        else {
            io_out->stream = stdout;
            sprintf(out_file_path, "%s%s", out_path, SHARC_STDOUT);
        }

        chrono_t chrono;
        chrono.start();
        /*
         * The following code is an example of how to use the
         * Density stream API to compress a file
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
                printf("Compressed ");
#ifdef SHARC_ALLOW_ANSI_ESCAPE_SEQUENCES
                printf("%c[1m", SHARC_ESCAPE_CHARACTER);
#endif
                printf("%s", in_file_path);
#ifdef SHARC_ALLOW_ANSI_ESCAPE_SEQUENCES
                printf("%c[0m", SHARC_ESCAPE_CHARACTER);
#endif
                printf(" (");
                format_decimal(total_read);
                printf(" bytes) to ");
#ifdef SHARC_ALLOW_ANSI_ESCAPE_SEQUENCES
                printf("%c[1m", SHARC_ESCAPE_CHARACTER);
#endif
                printf("%s", out_file_path);
#ifdef SHARC_ALLOW_ANSI_ESCAPE_SEQUENCES
                printf("%c[0m", SHARC_ESCAPE_CHARACTER);
#endif
                printf(" (");
                format_decimal(total_written);
                printf(" bytes)");
                printf(" %s %.1lf%% (User time %.3lfs %s %.0lf MB/s)\n",
                       SHARC_ARROW, ratio, elapsed, SHARC_ARROW, speed);
            } else {
                printf("Compressed ");
#ifdef SHARC_ALLOW_ANSI_ESCAPE_SEQUENCES
                printf("%c[1m", SHARC_ESCAPE_CHARACTER);
#endif
                printf("%s", name);
#ifdef SHARC_ALLOW_ANSI_ESCAPE_SEQUENCES
                printf("%c[0m", SHARC_ESCAPE_CHARACTER);
#endif
                printf(" to ");
#ifdef SHARC_ALLOW_ANSI_ESCAPE_SEQUENCES
                printf("%c[1m", SHARC_ESCAPE_CHARACTER);
#endif
                printf("%s", out_file_path);
#ifdef SHARC_ALLOW_ANSI_ESCAPE_SEQUENCES
                printf("%c[0m", SHARC_ESCAPE_CHARACTER);
#endif
                printf(", ");
                format_decimal(total_written);
                printf(" bytes written.\n");
            }
        }
        delete stream;
        free(io_out->name);
    }

    void
    client_io_t::decompress(client_io_t *const io_out, const bool prompting,
                            const char *in_path, const char *out_path)
    {
        if (origin_type == SHARC_HEADER_ORIGIN_TYPE_STREAM)
            name = SHARC_STDIN_COMPRESSED;
        const size_t in_file_name_length = strlen(name);
        if (in_file_name_length < 6) exit_error("Invalid file name");
        const size_t out_file_name_length = in_file_name_length - 6;
        io_out->name = (char *)malloc((out_file_name_length + 1) * sizeof(char));
        strncpy(io_out->name, name, out_file_name_length);
        io_out->name[out_file_name_length] = '\0';
        char in_file_path[strlen(in_path) + in_file_name_length + 1];
        const size_t out_file_path_length = strlen(out_path) + out_file_name_length;
        char out_file_path[(out_file_name_length > strlen(SHARC_STDOUT) ?
                            out_file_path_length:
                            strlen(out_path) + strlen(SHARC_STDOUT)) + 1];
        sprintf(in_file_path, "%s%s", in_path, name);

        if (origin_type == header_origin_type_file) {
            if (io_out->origin_type == header_origin_type_file)
                sprintf(out_file_path, "%s%s", out_path, io_out->name);
            this->stream = check_open_file(in_file_path, "rb", false);
        } else {
            if (io_out->origin_type == header_origin_type_file)
                strcpy(out_file_path, SHARC_STDIN);
            this->stream = stdin;
        }
        if (io_out->origin_type == header_origin_type_file)
            io_out->stream = check_open_file(out_file_path, "wb", prompting);
        else {
            io_out->stream = stdout;
            strcpy(out_file_path, SHARC_STDOUT);
        }
        chrono_t chrono;
        chrono.start();

        /*
         * The following code is an example of how to
         * use the Density stream API to decompress a file
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
                header.restore_file_attributes(out_file_path);
            if (origin_type == header_origin_type_file) {
                total_read += *stream->total_bytes_read;
                fclose(this->stream);
                if (header.origin_type() == header_origin_type_file) {
                    if (total_written != header.original_file_size())
                        exit_error("Input file is corrupt !");
                }
                double ratio = (100.0 * total_written) / total_read;
                double speed = (1.0 * total_written) / (elapsed * 1000.0 * 1000.0);
                printf("Decompressed ");
#ifdef SHARC_ALLOW_ANSI_ESCAPE_SEQUENCES
                printf("%c[1m", SHARC_ESCAPE_CHARACTER);
#endif
                printf("%s", in_file_path);
#ifdef SHARC_ALLOW_ANSI_ESCAPE_SEQUENCES
                printf("%c[0m", SHARC_ESCAPE_CHARACTER);
#endif
                printf(" (");
                format_decimal(total_read);
                printf(" bytes) to ");
#ifdef SHARC_ALLOW_ANSI_ESCAPE_SEQUENCES
                printf("%c[1m", SHARC_ESCAPE_CHARACTER);
#endif
                printf("%s", out_file_path);
#ifdef SHARC_ALLOW_ANSI_ESCAPE_SEQUENCES
                printf("%c[0m", SHARC_ESCAPE_CHARACTER);
#endif
                printf(" (");
                format_decimal(total_written);
                printf(" bytes)");
                printf(" %s %.1lf%% (User time %.3lfs %s %.0lf MB/s)\n",
                       SHARC_ARROW, ratio, elapsed, SHARC_ARROW, speed);
            } else {
                printf("Decompressed ");
#ifdef SHARC_ALLOW_ANSI_ESCAPE_SEQUENCES
                printf("%c[1m", SHARC_ESCAPE_CHARACTER);
#endif
                printf("%s", io_in->name);
#ifdef SHARC_ALLOW_ANSI_ESCAPE_SEQUENCES
                printf("%c[0m", SHARC_ESCAPE_CHARACTER);
#endif
                printf(" to ");
#ifdef SHARC_ALLOW_ANSI_ESCAPE_SEQUENCES
                printf("%c[1m", SHARC_ESCAPE_CHARACTER);
#endif
                printf("%s", out_file_path);
#ifdef SHARC_ALLOW_ANSI_ESCAPE_SEQUENCES
                printf("%c[0m", SHARC_ESCAPE_CHARACTER);
#endif
                printf(", ");
                format_decimal(total_written);
                printf(" bytes written.\n");
            }
        }
        delete stream;
        free(io_out->name);
    }

int
main(int argc, char *argv[])
{
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
    _setmode(_fileno(stderr), _O_BINARY);
#endif

    if (argc <= 1)
        density::client_io_t::usage();

    uint8_t action = SHARC_ACTION_COMPRESS;
    compression_mode_t mode = compression_mode_chameleon_algorithm;
    bool prompting = SHARC_PROMPTING;
    bool integrity_checks = SHARC_NO_INTEGRITY_CHECK;
    density::client_io_t in;
    in.origin_type = header_origin_type_file;
    in.name = "";
    density::client_io_t out;
    out.origin_type = header_origin_type_file;
    uint8_t path_mode = SHARC_FILE_OUTPUT_PATH;
    char in_path[SHARC_OUTPUT_PATH_MAX_SIZE] = "";
    char out_path[SHARC_OUTPUT_PATH_MAX_SIZE] = "";

    size_t arg_length;
    for (int idx = 1; idx < argc; idx++) {
        switch (argv[idx][0]) {
        case '-':
            arg_length = strlen(argv[idx]);
            if (arg_length < 2) density::client_io_t::usage();
            switch (argv[idx][1]) {
            case 'c':
                if (arg_length == 2) break;
                if (arg_length != 3) density::client_io_t::usage();
                switch (argv[idx][2] - '0') {
                case 0: mode = compression_mode_copy; break;
                case 1: mode = compression_mode_chameleon_algorithm; break;
                case 2: mode = compression_mode_cheetah_algorithm; break;
                case 3: mode = compression_mode_lion_algorithm; break;
                default: density::client_io_t::usage();
                }
                break;
            case 'd': action = SHARC_ACTION_DECOMPRESS; break;
            case 'p':
                if (arg_length == 2) density::client_io_t::usage();
                char *last_separator = strrchr(argv[idx], SHARC_PATH_SEPARATOR);
                if (argv[idx][2] != '.') {
                    if (last_separator == NULL) density::client_io_t::usage();
                    if (last_separator - argv[idx] + 1 != arg_length)
                        density::client_io_t::usage();
                    strncpy(out_path, argv[idx] + 2, SHARC_OUTPUT_PATH_MAX_SIZE);
                }
                path_mode = SHARC_FIXED_OUTPUT_PATH;
                break;
            case 'f': prompting = SHARC_NO_PROMPTING; break;
            case 'x': integrity_checks = SHARC_INTEGRITY_CHECKS; break;
            case 'i': in.origin_type = SHARC_HEADER_ORIGIN_TYPE_STREAM; break;
            case 'o': out.origin_type = SHARC_HEADER_ORIGIN_TYPE_STREAM; break;
            case 'v': density::client_io_t::version(); exit(0);
            case 'h': density::client_io_t::usage(); break;
            case '-':
                if (arg_length < 3) density::client_io_t::usage();
                switch (argv[idx][2]) {
                case 'c':
                    if (arg_length < 4) density::client_io_t::usage();
                    switch (argv[idx][3]) {
                    case 'o':
                        if (arg_length == 10) break;
                        if (arg_length != 12) density::client_io_t::usage();
                        switch (argv[idx][11] - '0') {
                        case 0: mode = compression_mode_COPY; break;
                        case 1: mode = compression_mode_CHAMELEON_ALGORITHM; break;
                        case 2: mode = compression_mode_CHEETAH_ALGORITHM; break;
                        case 3: mode = compression_mode_LION_ALGORITHM; break;
                        default: density::client_io_t::usage();
                        }
                        break;
                    case 'h':
                        if (arg_length != 17) density::client_io_t::usage();
                        integrity_checks = SHARC_INTEGRITY_CHECKS;
                        break;
                    default: density::client_io_t::usage();
                    }
                    break;
                case 'd':
                    if (arg_length != 12) density::client_io_t::usage();
                    action = SHARC_ACTION_DECOMPRESS;
                    break;
                case 'o':
                    if (arg_length <= 14) density::client_io_t::usage();
                    last_separator = strrchr(argv[idx], SHARC_PATH_SEPARATOR);
                    if (argv[idx][14] != '.') {
                        if (last_separator == NULL) density::client_io_t::usage();
                        if (last_separator - argv[idx] + 1 != arg_length)
                            density::client_io_t::usage();
                        strncpy(out_path, argv[idx] + 14, SHARC_OUTPUT_PATH_MAX_SIZE);
                    }
                    path_mode = SHARC_FIXED_OUTPUT_PATH;
                    break;
                case 'n':
                    if (arg_length != 11) density::client_io_t::usage();
                    prompting = SHARC_NO_PROMPTING;
                    break;
                case 's':
                    if (arg_length == 7) in.origin_type = SHARC_HEADER_ORIGIN_TYPE_STREAM;
                    else if (arg_length == 8) out.origin_type = SHARC_HEADER_ORIGIN_TYPE_STREAM;
                    else density::client_io_t::usage();
                    break;
                case 'v':
                    if (arg_length != 9) density::client_io_t::usage();
                    density::client_io_t::version();
                    exit(0);
                case 'h': density::client_io_t::usage(); break;
                default: density::client_io_t::usage();
                }
                break;
            default: break;
            }
            break;
        default:
            if (in.origin_type == header_origin_type_file) {
                char *last_separator = strrchr(argv[idx], SHARC_PATH_SEPARATOR);
                if (last_separator != NULL) {
                    in.name = last_separator + 1;
                    size_t chars_to_copy = in.name - argv[idx];
                    if (chars_to_copy < SHARC_OUTPUT_PATH_MAX_SIZE) {
                        strncpy(in_path, argv[idx], chars_to_copy);
                        in_path[chars_to_copy] = '\0';
                    } else
                        density::client_io_t::usage();
                    if (path_mode == SHARC_FILE_OUTPUT_PATH) strcpy(out_path, in_path);
                } else  in.name = argv[idx];
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
    if (in.origin_type == SHARC_HEADER_ORIGIN_TYPE_STREAM) {
        switch (action) {
        case SHARC_ACTION_DECOMPRESS:
            in.decompress(&out, prompting, in_path, out_path);
            break;
        default:
            in.compress(&out, mode, prompting, integrity_checks, in_path, out_path);
            break;
        }
    }
    return true;
}
