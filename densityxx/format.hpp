// see LICENSE.md for license.
#pragma once

#include "densityxx/memory.hpp"

namespace density {
    // header.
#pragma pack(push)
#pragma pack(4)
    class block_header_t {
    public:
        // Previous block's relative start position (parallelizable decompressible output)
        uint32_t relative_position; // previousBlockRelativeStartPosition
        inline uint_fast32_t read(location_t *in)
        {   in->read(this, sizeof(*this)); return sizeof(*this); }
        inline uint_fast32_t write(location_t *out)
        {   out->write(this, sizeof(*this)); return sizeof(*this); }
        inline uint_fast32_t write(location_t *out, const uint32_t relative_position)
        {   this->relative_position = relative_position; return write(out); }
    };
#pragma pack(pop)

    // mode_marker.
#pragma pack(push)
#pragma pack(4)
    class block_mode_marker_t {
    public:
        uint8_t mode; // activeBlockMode
        uint8_t reserved;
        inline uint_fast32_t read(location_t *in)
        {   in->read(this, sizeof(*this)); return sizeof(*this); }
        inline uint_fast32_t write(location_t *out)
        {   out->write(this, sizeof(*this)); return sizeof(*this); }
        inline uint_fast32_t write(location_t *out, const compression_mode_t mode)
        {   this->mode = mode; reserved = 0; return write(out); }
    };
#pragma pack(pop)

    // footer.
#pragma pack(push)
#pragma pack(4)
    class block_footer_t {
    public:
        uint64_t hashsum1;
        uint64_t hashsum2;
        inline uint_fast32_t read(location_t *in)
        {   in->read(this, sizeof(*this)); return sizeof(*this); }
        inline uint_fast32_t write(location_t *out)
        {   out->write(this, sizeof(*this)); return sizeof(*this); }
        inline uint_fast32_t
        write(location_t *out, uint64_t hashsum1, uint64_t hashsum2)
        {   this->hashsum1 = hashsum1; this->hashsum2 = hashsum2; return write(out); }

        inline bool check(uint64_t hashsum1, uint64_t hashsum2)
        {   return this->hashsum1 == hashsum1 && this->hashsum2 == hashsum2; }
    };
#pragma pack(pop)

    // main header.
#pragma pack(push)
#pragma pack(4)
    struct main_header_parameters_t {
        union {
            uint64_t as_uint64_t;
            uint8_t as_bytes[8];
        };
    };
    class main_header_t {
    private:
        uint8_t version[3];
        uint8_t compression_mode;
        uint8_t block_type;
        uint8_t reserved[3];
        main_header_parameters_t parameters;
    public:
        inline const compression_mode_t get_compression_mode(void) const
        {   return (const compression_mode_t)compression_mode; }
        inline const block_type_t get_block_type(void) const
        {   return (const block_type_t) block_type; }

        inline void
        setup(const compression_mode_t compression_mode, const block_type_t block_type)
        {
            version[0] = major_version;
            version[1] = minor_version;
            version[2] = revision;
            this->compression_mode = compression_mode;
            this->block_type = block_type;
            memset(reserved, 0, sizeof(reserved));
            memset(&parameters, 0, sizeof(parameters));
#if DENSITY_ENABLE_PARALLELIZABLE_DECOMPRESSIBLE_OUTPUT == DENSITY_YES
            parameters.as_bytes[0] = dictionary_preferred_reset_cycle_shift;
#endif
        }
    };
#pragma pack(pop)

    // main footer.
#pragma pack(push)
#pragma pack(4)
    class main_footer_t {
    public:
        // Previous block's relative start position (parallelizable decompressible output)
        uint32_t relative_position; // previousBlockRelativeStartPosition;
    };
#pragma pack(pop)
}
