// see LICENSE.md for license.
#include "sharcxx/header.hpp"
#include <utime.h>

namespace density {
    uint_fast32_t
    header_t::read(FILE *RESTRICT rfp)
    {
        uint_fast32_t read = (uint_fast32_t)
            fread(&header_generic.magic_number, sizeof(uint8_t), sizeof(uint32_t), rfp);
        header_generic.magic_number = LITTLE_ENDIAN_32(header_generic.magic_number);
        header_generic.version[0] = (uint8_t)fgetc(rfp);
        header_generic.version[1] = (uint8_t)fgetc(rfp);
        header_generic.version[2] = (uint8_t)fgetc(rfp);
        header_generic.origin_type = (uint8_t)fgetc(rfp);
        read += 4;
        switch (header_generic.origin_type) {
        case header_origin_type_file:
            read += fread(&header_file_information.original_file_size,
                          sizeof(uint8_t), sizeof(uint64_t), rfp);
            header_file_information.original_file_size =
                LITTLE_ENDIAN_64(header_file_information.original_file_size);
            read += fread(&header_file_information.file_mode,
                          sizeof(uint8_t), sizeof(uint32_t), rfp);
            header_file_information.file_mode =
                LITTLE_ENDIAN_32(header_file_information.file_mode);
            read += fread(&header_file_information.file_accessed,
                          sizeof(uint8_t), sizeof(uint64_t), rfp);
            header_file_information.file_accessed =
                LITTLE_ENDIAN_64(header_file_information.file_accessed);
            read += fread(&header_file_information.file_modified,
                          sizeof(uint8_t), sizeof(uint64_t), rfp);
            header_file_information.file_modified =
                LITTLE_ENDIAN_64(header_file_information.file_modified);
            break;
        default:
            break;
        }
        return read;
    }
    uint_fast32_t
    header_t::write(FILE *RESTRICT wfp,
                    const header_origin_type_t header_origin_type,
                    const struct stat *RESTRICT stat)
    {
        uint32_t temp32;
        uint64_t temp64;

        temp32 = LITTLE_ENDIAN_32(SHARC_HEADER_MAGIC_NUMBER);
        uint_fast32_t written = (uint_fast32_t)
            fwrite(&temp32, sizeof(uint8_t), sizeof(uint32_t), wfp);
        fputc(major_version, wfp);
        fputc(minor_version, wfp);
        fputc(revision, wfp);
        fputc(header_origin_type, wfp);
        written += 4;
        switch (header_origin_type) {
        case header_origin_type_file:
            temp64 = LITTLE_ENDIAN_64(stat->st_size);
            written += fwrite(&temp64, sizeof(uint8_t), sizeof(uint64_t), wfp);
            temp32 = LITTLE_ENDIAN_32(stat->st_mode);
            written += fwrite(&temp32, sizeof(uint8_t), sizeof(uint32_t), wfp);
            temp64 = LITTLE_ENDIAN_64(stat->st_atime);
            written += fwrite(&temp64, sizeof(uint8_t), sizeof(uint64_t), wfp);
            temp64 = LITTLE_ENDIAN_64(stat->st_mtime);
            written += fwrite(&temp64, sizeof(uint8_t), sizeof(uint64_t), wfp);
            break;
        default:
            break;
        }
        return written;
    }
    bool
    header_t::restore_file_attributes(const char *RESTRICT file_name)
    {
        if (header_generic.origin_type == header_origin_type_file) {
            struct utimbuf ubuf;
            ubuf.actime = (time_t)header_file_information.file_accessed;
            ubuf.modtime = (time_t)header_file_information.file_modified;
            if (utime(file_name, &ubuf)) return false;
            if (chmod(file_name, (mode_t) header_file_information.file_mode))
                return false;
            return true;
        } else
            return false;
    }
}
