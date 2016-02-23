// see LICENSE.md for license.
#pragma once

#include "densityxx/globals.hpp"

namespace density {
    const uint32_t header_magic_number = 1908011803U;

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
        static uint_fast32_t
        write(FILE *RESTRICT wfp, const header_origin_type_t header_origin_type,
              const struct stat *RESTRICT stat);

        inline bool check_validity(void) const
        {   return header_generic.magic_number == header_magic_number; }
        inline header_origin_type_t origin_type(void) const
        {   return (header_origin_type_t)header_generic.origin_type; }
        inline uint64_t original_file_size(void) const
        {   return header_file_information.original_file_size; }
        uint_fast32_t read(FILE *RESTRICT rfp);
        bool restore_file_attributes(const char *RESTRICT file_name);
    };
#pragma pack(pop)
}
