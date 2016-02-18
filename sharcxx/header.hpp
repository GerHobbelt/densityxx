// see LICENSE.md for license.
#pragma once

#include "densityxx/globals.hpp"

#define SHARC_HEADER_MAGIC_NUMBER      1908011803

namespace density {
    typedef enum {
        header_origin_type_stream,
        header_origin_type_file
    } header_origin_type_t;

#pragma pack(push)
#pragma pack(4)
    struct header_generic_t {
        uint32_t magic_number;
        uint8_t version[3];
        uint8_t origin_type;
        uint8_t reserved[4];
    };

    struct header_file_information_t {
        uint64_t original_file_size;
        uint32_t file_mode;
        uint64_t file_accessed;
        uint64_t file_modified;
    };

    class header_t {
        header_generic_t header_generic;
        header_file_information_t header_file_information;
    public:
        inline bool check_validity(void) const
        {   return header_generic.magic_number == SHARC_HEADER_MAGIC_NUMBER; }
        uint_fast32_t read(FILE *RESTRICT rfp);
        uint_fast32_t write(FILE *RESTRICT wfp,
                            const header_origin_type_t header_origin_type,
                            const struct stat *RESTRICT stat);
        bool restore_file_attributes(const char *RESTRICT file_name);
    };
#pragma pack(pop)
}
