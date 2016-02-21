// see LICENSE.md for license.
#pragma once
#include "densityxx/memory.def.hpp"

namespace density {
    // location_t.
    inline void
    location_t::consume(uint_fast64_t sz)
    {
        pointer += sz;
        available_bytes -= sz;
    }
    inline void
    location_t::read(void *data, uint_fast64_t szdata)
    {
        DENSITY_MEMCPY(data, pointer, szdata);
        consume(szdata);
    }
    inline void
    location_t::write(const void *data, uint_fast64_t szdata)
    {
        DENSITY_MEMCPY(pointer, data, szdata);
        consume(szdata);
    }
    inline void
    location_t::encapsulate(uint8_t *RESTRICT pointer, const uint_fast64_t bytes)
    {
        this->pointer = pointer;
        available_bytes = bytes;
        initial_available_bytes = bytes;
    }
    inline uint_fast64_t
    location_t::used(void) const
    {
        return initial_available_bytes - available_bytes;
    }

    // teleport_t.
    inline teleport_t::teleport_t(const uint_fast64_t size)
    {
        staging.pointer = (uint8_t *)malloc(size);
        staging.available_bytes = 0;
        write_pointer = original_pointer = staging.pointer;
        direct.available_bytes = 0;
    }
    inline teleport_t::~teleport_t()
    {
        free(original_pointer);
    }
    inline void
    teleport_t::change_input_buffer(const uint8_t *RESTRICT in,
                                    const uint_fast64_t available_in)
    {
        direct.encapsulate((uint8_t *)in, available_in);
    }
    inline void
    teleport_t::copy_from_direct_buffer_to_staging_buffer(void)
    {
        DENSITY_MEMCPY(write_pointer, direct.pointer, direct.available_bytes);
        write_pointer += direct.available_bytes;
        staging.available_bytes += direct.available_bytes;
        direct.pointer += direct.available_bytes;
        direct.available_bytes = 0;
    }
    inline void
    teleport_t::reset_staging_buffer(void)
    {
        rewind_staging_pointers();
        staging.available_bytes = 0;
    }
    inline location_t *
    teleport_t::read(const uint_fast64_t bytes)
    {
        uint_fast64_t addon_bytes, direct_bytes;
        if (DENSITY_UNLIKELY(staging.available_bytes)) {
            if (staging.available_bytes >= bytes) return &staging;
            addon_bytes = bytes - staging.available_bytes;
            if (DENSITY_UNLIKELY(addon_bytes <= direct.available_bytes)) {
                direct_bytes = direct.initial_available_bytes - direct.available_bytes;
                if (staging.available_bytes <= direct_bytes) {
                    // Revert to direct buffer reading
                    reset_staging_buffer();
                    direct.pointer -= staging.available_bytes;
                    direct.available_bytes += staging.available_bytes;
                    return &direct;
                } else { // Copy missing bytes from direct input buffer
                    DENSITY_MEMCPY(write_pointer, direct.pointer, addon_bytes);
                    write_pointer += addon_bytes;
                    staging.available_bytes += addon_bytes;
                    direct.pointer += addon_bytes;
                    direct.available_bytes -= addon_bytes;
                    return &staging;
                }
            } else { // Copy as mush as we can from direct input buffer
                copy_from_direct_buffer_to_staging_buffer();
                return NULL;
            }
        } else {
            if (DENSITY_LIKELY(direct.available_bytes >= bytes)) return &direct;
            // Copy what we have in our staging buffer
            rewind_staging_pointers();
            copy_from_direct_buffer_to_staging_buffer();
            return NULL;
        }
    }
    inline location_t *
    teleport_t::read_reserved(const uint_fast64_t bytes, const uint_fast64_t reserved)
    {
        return read(bytes + reserved);
    }
    inline location_t *
    teleport_t::read_remaining_reserved(const uint_fast64_t reserved)
    {
        return read_reserved(available_bytes_reserved(reserved), reserved);
    }
    inline uint_fast64_t
    teleport_t::available_bytes(void) const
    {
        return staging.available_bytes + direct.available_bytes;
    }
    inline uint_fast64_t
    teleport_t::available_bytes_reserved(const uint_fast64_t reserved) const
    {
        const uint_fast64_t contained = available_bytes();
        return DENSITY_UNLIKELY(reserved >= contained) ? 0: contained - reserved;
    }
    inline void
    teleport_t::copy(location_t *RESTRICT out, const uint_fast64_t bytes)
    {
        uint_fast64_t from_staging = 0;
        uint_fast64_t from_direct = 0;

        if (staging.available_bytes) {
            if (bytes <= staging.available_bytes) {
                from_staging = bytes;
            } else if (bytes <= staging.available_bytes + direct.available_bytes) {
                from_staging = staging.available_bytes;
                from_direct = bytes - staging.available_bytes;
            } else {
                from_staging = staging.available_bytes;
                from_direct = direct.available_bytes;
            }
        } else {
            if (bytes <= direct.available_bytes) {
                from_direct = bytes;
            } else {
                from_direct = direct.available_bytes;
            }
        }

        DENSITY_MEMCPY(out->pointer, staging.pointer, from_staging);
        if (from_staging == staging.available_bytes) reset_staging_buffer();
        else {
            staging.pointer += from_staging;
            staging.available_bytes -= from_staging;
        }
        out->pointer += from_staging;
        out->available_bytes -= from_staging;

        DENSITY_MEMCPY(out->pointer, direct.pointer, from_direct);
        direct.pointer += from_direct;
        direct.available_bytes -= from_direct;
        out->pointer += from_direct;
        out->available_bytes -= from_direct;
    }
    inline void
    teleport_t::copy_remaining(location_t *out)
    {
        return copy(out, available_bytes());
    }
    inline void
    teleport_t::rewind_staging_pointers(void)
    {
        staging.pointer = write_pointer = original_pointer;
    }
}
