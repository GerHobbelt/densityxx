// see LICENSE.md for license.
#pragma once

#include "globals.hpp"

namespace density {
    class location_t {
    public:
        uint8_t *pointer;
        uint_fast64_t available_bytes;
        uint_fast64_t initial_available_bytes;

        void consume(uint_fast64_t sz);
        void read(void *data, uint_fast64_t szdata);
        void write(const void *data, uint_fast64_t szdata);
        void encapsulate(uint8_t *pointer, const uint_fast64_t bytes);
        uint_fast64_t used(void) const;
    };
    class teleport_t {
    public:
        uint8_t *original_pointer, *write_pointer;
        location_t staging, direct;

        teleport_t(const uint_fast64_t size);
        ~teleport_t();
        void change_input_buffer(const uint8_t *in, const uint_fast64_t available_in);
        void copy_from_direct_buffer_to_staging_buffer(void);
        void reset_staging_buffer(void);
        location_t *read(const uint_fast64_t bytes);
        location_t *read_reserved(const uint_fast64_t bytes, const uint_fast64_t reserved);
        location_t *read_remaining_reserved(const uint_fast64_t reserved);
        uint_fast64_t available_bytes(void) const;
        uint_fast64_t available_bytes_reserved(const uint_fast64_t reserved) const;
        void copy(location_t *out, const uint_fast64_t bytes);
        void copy_remaining(location_t *out);
    private:
        void rewind_staging_pointers(void);
    };
}
