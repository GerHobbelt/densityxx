#include "densityxx/memory.hpp"

namespace density {
    location_t *
    teleport_t::read(const uint_fast64_t bytes)
    {
        uint_fast64_t addon_bytes;
        if (DENSITY_UNLIKELY(staging.available_bytes)) {
            if (staging.available_bytes >= bytes) {
                return &staging;
            } else if (DENSITY_UNLIKELY((addon_bytes = (bytes - staging.available_bytes)) <= direct.available_bytes)) {
                if (staging.available_bytes <= direct.initial_available_bytes - direct.available_bytes) { // Revert to direct buffer reading
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
            if (DENSITY_LIKELY(direct.available_bytes >= bytes)) {
                return &direct;
            } else { // Copy what we have in our staging buffer
                rewind_staging_pointers();
                copy_from_direct_buffer_to_staging_buffer();
                return NULL;
            }
        }
    }

    void
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
}
