// see LICENSE.md for license.
#pragma once

#include "densityxx/format.hpp"
#include "densityxx/spookyhash.hpp"

#define DENSITY_PREFERRED_COPY_BLOCK_SIZE  (1 << 19)
#define DENSITY_SPOOKYHASH_SEED_1  (0xabc)
#define DENSITY_SPOOKYHASH_SEED_2  (0xdef)

namespace density {
    // encode.
    typedef enum {
        BLOCK_ENCODE_STATE_READY = 0,
        BLOCK_ENCODE_STATE_STALL_ON_INPUT,
        BLOCK_ENCODE_STATE_STALL_ON_OUTPUT,
        BLOCK_ENCODE_STATE_ERROR
    } BLOCK_ENCODE_STATE;

    typedef enum {
        BLOCK_ENCODE_PROCESS_WRITE_BLOCK_HEADER,
        BLOCK_ENCODE_PROCESS_WRITE_BLOCK_MODE_MARKER,
        BLOCK_ENCODE_PROCESS_WRITE_BLOCK_FOOTER,
        BLOCK_ENCODE_PROCESS_WRITE_DATA,
    } BLOCK_ENCODE_PROCESS;
    
    typedef enum {
        KERNEL_ENCODE_STATE_READY = 0,
        KERNEL_ENCODE_STATE_INFO_NEW_BLOCK,
        KERNEL_ENCODE_STATE_INFO_EFFICIENCY_CHECK,
        KERNEL_ENCODE_STATE_STALL_ON_INPUT,
        KERNEL_ENCODE_STATE_STALL_ON_OUTPUT,
        KERNEL_ENCODE_STATE_ERROR
    } KERNEL_ENCODE_STATE;

#pragma pack(push)
#pragma pack(4)
    class block_encode_state_t {
    public:
        BLOCK_ENCODE_PROCESS process;
        COMPRESSION_MODE targetMode;
        COMPRESSION_MODE currentMode;
        BLOCK_TYPE blockType;

        uint_fast64_t totalRead;
        uint_fast64_t totalWritten;

        // current_block_data.
        uint_fast64_t inStart;
        uint_fast64_t outStart;

        // integrity_data.
        bool update;
        uint8_t *inputPointer;
        spookyhash_context_t context;

        void *kernelEncodeState;

        inline BLOCK_ENCODE_STATE
        init(const COMPRESSION_MODE mode, const BLOCK_TYPE blockType, void *kernelState)
        {
            this->blockType = blockType;
            targetMode = currentMode = mode;
            totalRead = totalWritten = 0;
            if (blockType == BLOCK_TYPE_WITH_HASHSUM_INTEGRITY_CHECK) update = true;
            switch (currentMode) {
            case COMPRESSION_MODE_COPY: break;
            default: kernelEncodeState = kernelState; break;
            }
            return exit_process(BLOCK_ENCODE_PROCESS_WRITE_BLOCK_HEADER,
                                BLOCK_ENCODE_STATE_READY);
        }

        inline BLOCK_ENCODE_STATE
        continue_(memory_teleport_t *RESTRICT in, memory_location_t *RESTRICT out)
        {
            BLOCK_ENCODE_STATE blockEncodeState;
            KERNEL_ENCODE_STATE kernelEncodeState;
            uint_fast64_t availableInBefore;
            uint_fast64_t availableOutBefore;
            uint_fast64_t blockRemaining;
            uint_fast64_t inRemaining;
            uint_fast64_t outRemaining;

            // Add to the integrity check hashsum
            if (blockType == BLOCK_TYPE_WITH_HASHSUM_INTEGRITY_CHECK && update)
                update_integrity_data(in);

            // Dispatch
            switch (process) {
            case BLOCK_ENCODE_PROCESS_WRITE_BLOCK_HEADER: goto write_block_header;
            case BLOCK_ENCODE_PROCESS_WRITE_BLOCK_MODE_MARKER: goto write_mode_marker;
            case BLOCK_ENCODE_PROCESS_WRITE_DATA: goto write_data;
            case BLOCK_ENCODE_PROCESS_WRITE_BLOCK_FOOTER: goto write_block_footer;
            default: return BLOCK_ENCODE_STATE_ERROR;
            }

        write_mode_marker:
            if ((blockEncodeState = write_mode_marker(out)))
                return exit_process(BLOCK_ENCODE_PROCESS_WRITE_BLOCK_MODE_MARKER,
                                    blockEncodeState);
            goto write_data;

        write_block_header:
            if ((blockEncodeState = write_block_header(in, out)))
                return exit_process(BLOCK_ENCODE_PROCESS_WRITE_BLOCK_HEADER, blockEncodeState);

        write_data:
            availableInBefore = in->available_bytes();
            availableOutBefore = out->available_bytes;

            switch (currentMode) {
            case COMPRESSION_MODE_COPY:
                blockRemaining = (uint_fast64_t)DENSITY_PREFERRED_COPY_BLOCK_SIZE - (totalRead - inStart);
                inRemaining = in->available_bytes();
                outRemaining = out->available_bytes;

                if (inRemaining <= outRemaining) {
                    if (blockRemaining <= inRemaining) goto copy_until_end_of_block;
                    else {
                        in->copy(out, inRemaining);
                        update_totals(in, out, availableInBefore, availableOutBefore);
                    if (blockType == BLOCK_TYPE_WITH_HASHSUM_INTEGRITY_CHECK)
                        update_integrity_hash(in, true);
                    in->copy_from_direct_buffer_to_staging_buffer();
                    return exit_process(BLOCK_ENCODE_PROCESS_WRITE_DATA,
                                        BLOCK_ENCODE_STATE_STALL_ON_INPUT);
                    }
                } else {
                    if (blockRemaining <= outRemaining)
                        goto copy_until_end_of_block;
                    else {
                        in->copy(out, outRemaining);
                        update_totals(in, out, availableInBefore, availableOutBefore);
                        return exit_process(BLOCK_ENCODE_PROCESS_WRITE_DATA,
                                            BLOCK_ENCODE_STATE_STALL_ON_OUTPUT);
                    }
                }

            copy_until_end_of_block:
                in->copy(out, blockRemaining);
                update_totals(in, out, availableInBefore, availableOutBefore);
                goto write_block_footer;

            default:
            kernelEncodeState = _process(in, out, this->kernelEncodeState);
            update_totals(in, out, availableInBefore, availableOutBefore);

            switch (kernelEncodeState) {
                case KERNEL_ENCODE_STATE_STALL_ON_INPUT:
                    if (blockType == BLOCK_TYPE_WITH_HASHSUM_INTEGRITY_CHECK)
                        update_integrity_hash(in, true);
                    return exit_process(BLOCK_ENCODE_PROCESS_WRITE_DATA,
                                        BLOCK_ENCODE_STATE_STALL_ON_INPUT);
                case KERNEL_ENCODE_STATE_STALL_ON_OUTPUT:
                    return exit_process(BLOCK_ENCODE_PROCESS_WRITE_DATA,
                                        BLOCK_ENCODE_STATE_STALL_ON_OUTPUT);
                case KERNEL_ENCODE_STATE_INFO_NEW_BLOCK: goto write_block_footer;
                case KERNEL_ENCODE_STATE_INFO_EFFICIENCY_CHECK: goto write_mode_marker;
                default:
                    return BLOCK_ENCODE_STATE_ERROR;
            }
            }

        write_block_footer:
            if (blockType == BLOCK_TYPE_WITH_HASHSUM_INTEGRITY_CHECK)
                if ((blockEncodeState = write_block_footer(in, out)))
                    return exit_process(BLOCK_ENCODE_PROCESS_WRITE_BLOCK_FOOTER,
                                        blockEncodeState);
            goto write_block_header;
        }

        inline BLOCK_ENCODE_STATE
        finish(memory_teleport_t *RESTRICT in, memory_location_t *RESTRICT out)
        {
            BLOCK_ENCODE_STATE blockEncodeState;
            KERNEL_ENCODE_STATE kernelEncodeState;
            uint_fast64_t availableInBefore;
            uint_fast64_t availableOutBefore;
            uint_fast64_t blockRemaining;
            uint_fast64_t inRemaining;
            uint_fast64_t outRemaining;

            // Add to the integrity check hashsum
            if (blockType == BLOCK_TYPE_WITH_HASHSUM_INTEGRITY_CHECK && update)
                update_integrity_data(in);

            // Dispatch
            switch (process) {
            case BLOCK_ENCODE_PROCESS_WRITE_BLOCK_HEADER: goto write_block_header;
            case BLOCK_ENCODE_PROCESS_WRITE_BLOCK_MODE_MARKER: goto write_mode_marker;
            case BLOCK_ENCODE_PROCESS_WRITE_DATA: goto write_data;
            case BLOCK_ENCODE_PROCESS_WRITE_BLOCK_FOOTER: goto write_block_footer;
            default: return BLOCK_ENCODE_STATE_ERROR;
            }

        write_mode_marker:
            if ((blockEncodeState = write_mode_marker(out)))
                return exit_process(BLOCK_ENCODE_PROCESS_WRITE_BLOCK_MODE_MARKER,
                                    blockEncodeState);
            goto write_data;

        write_block_header:
            if ((blockEncodeState = write_block_header(in, out)))
                return exit_process(BLOCK_ENCODE_PROCESS_WRITE_BLOCK_HEADER, blockEncodeState);

        write_data:
            availableInBefore = in->available_bytes();
            availableOutBefore = out->available_bytes;

            switch (currentMode) {
            case COMPRESSION_MODE_COPY:
                blockRemaining = (uint_fast64_t)DENSITY_PREFERRED_COPY_BLOCK_SIZE - (totalRead - inStart);
                inRemaining = in->available_bytes();
                outRemaining = out->available_bytes;

                if (inRemaining <= outRemaining) {
                    if (blockRemaining <= inRemaining) goto copy_until_end_of_block;
                    else {
                        in->copy(out, inRemaining);
                        update_totals(in, out, availableInBefore, availableOutBefore);
                        goto write_block_footer;
                    }
                } else {
                    if (blockRemaining <= outRemaining) goto copy_until_end_of_block;
                    else {
                        in->copy(out, outRemaining);
                        update_totals(in, out, availableInBefore, availableOutBefore);
                        return exit_process(BLOCK_ENCODE_PROCESS_WRITE_DATA,
                                            BLOCK_ENCODE_STATE_STALL_ON_OUTPUT);
                    }
                }

            copy_until_end_of_block:
                in->copy(out, blockRemaining);
                update_totals(in, out, availableInBefore, availableOutBefore);
                goto write_block_footer;

            default:
                kernelEncodeState = _finish(in, out, this->kernelEncodeState);
                update_totals(in, out, availableInBefore, availableOutBefore);

                switch (kernelEncodeState) {
                case KERNEL_ENCODE_STATE_STALL_ON_INPUT:
                    return BLOCK_ENCODE_STATE_ERROR;
                case KERNEL_ENCODE_STATE_STALL_ON_OUTPUT:
                    return exit_process(BLOCK_ENCODE_PROCESS_WRITE_DATA,
                                        BLOCK_ENCODE_STATE_STALL_ON_OUTPUT);
                case KERNEL_ENCODE_STATE_READY:
                case KERNEL_ENCODE_STATE_INFO_NEW_BLOCK: goto write_block_footer;
                case KERNEL_ENCODE_STATE_INFO_EFFICIENCY_CHECK: goto write_mode_marker;
                default: return BLOCK_ENCODE_STATE_ERROR;
                }
            }

        write_block_footer:
            if (blockType == BLOCK_TYPE_WITH_HASHSUM_INTEGRITY_CHECK)
                if ((blockEncodeState = write_block_footer(in, out)))
                    return exit_process(BLOCK_ENCODE_PROCESS_WRITE_BLOCK_FOOTER,
                                        blockEncodeState);
            if (in->available_bytes()) goto write_block_header;
            //if (blockType == BLOCK_TYPE_WITH_HASHSUM_INTEGRITY_CHECK) delete context;
            return BLOCK_ENCODE_STATE_READY;
        }
    protected:
        virtual KERNEL_ENCODE_STATE _init(void *) = 0;
        virtual KERNEL_ENCODE_STATE
        _process(memory_teleport_t *, memory_location_t *, void *) = 0;
        virtual KERNEL_ENCODE_STATE
        _finish(memory_teleport_t *, memory_location_t *, void *) = 0;
    private:
        inline BLOCK_ENCODE_STATE
        exit_process(BLOCK_ENCODE_PROCESS process, BLOCK_ENCODE_STATE blockEncodeState)
        {   this->process = process; return blockEncodeState; }
        inline void
        update_integrity_data(memory_teleport_t *RESTRICT in)
        {   inputPointer = in->direct.pointer; update = false; }
        inline void
        update_integrity_hash(memory_teleport_t *RESTRICT in, bool pendingExit)
        {   const uint8_t *const pointerBefore = inputPointer;
            const uint8_t *const pointerAfter = in->direct.pointer;
            const uint_fast64_t processed = pointerAfter - pointerBefore;
            if (pendingExit) {
                context.update(inputPointer, processed);
                update = true;
            } else {
                context.update(inputPointer, processed - in->staging.available_bytes);
                update_integrity_data(in);
            }
        }

        inline BLOCK_ENCODE_STATE
        write_block_header(memory_teleport_t *RESTRICT in, memory_location_t *RESTRICT out)
        {
            if (sizeof(block_header_t) > out->available_bytes)
                return BLOCK_ENCODE_STATE_STALL_ON_OUTPUT;
            currentMode = targetMode;
#if DENSITY_ENABLE_PARALLELIZABLE_DECOMPRESSIBLE_OUTPUT == DENSITY_YES
            block_header_t block_header;
            totalWritten += block_header.write(out, totalRead > 0 ? (uint32_t)totalRead - inStart: 0);
#endif
            inStart = totalRead;
            outStart = totalWritten;

            if (blockType == BLOCK_TYPE_WITH_HASHSUM_INTEGRITY_CHECK) {
                context.init(DENSITY_SPOOKYHASH_SEED_1, DENSITY_SPOOKYHASH_SEED_2);
                context.update(in->staging.pointer, in->staging.available_bytes);
                update_integrity_data(in);
            }
            return BLOCK_ENCODE_STATE_READY;
        }

        inline BLOCK_ENCODE_STATE
        write_block_footer(memory_teleport_t *RESTRICT in, memory_location_t *RESTRICT out)
        {
            block_footer_t block_footer;
            if (sizeof(block_footer) > out->available_bytes)
                return BLOCK_ENCODE_STATE_STALL_ON_OUTPUT;
            update_integrity_hash(in, false);
            uint64_t hashsum1, hashsum2;
            context.final(&hashsum1, &hashsum2);
            totalWritten += block_footer.write(out, hashsum1, hashsum2);
            return BLOCK_ENCODE_STATE_READY;
        }

        inline BLOCK_ENCODE_STATE
        write_mode_marker(memory_location_t *RESTRICT out)
        {
            block_mode_marker_t block_mode_marker;
            if (sizeof(block_mode_marker) > out->available_bytes)
                return BLOCK_ENCODE_STATE_STALL_ON_OUTPUT;
            switch (currentMode) {
            case COMPRESSION_MODE_COPY: break;
            default:
                if (totalWritten > totalRead) currentMode = COMPRESSION_MODE_COPY;
                break;
            }
            totalWritten += block_mode_marker.write(out, currentMode);
            return BLOCK_ENCODE_STATE_READY;
        }

        inline void
        update_totals(memory_teleport_t *RESTRICT in, memory_location_t *RESTRICT out,
                      const uint_fast64_t availableInBefore,
                      const uint_fast64_t availableOutBefore)
        {
            totalRead += availableInBefore - in->available_bytes();
            totalWritten += availableOutBefore - out->available_bytes;
        }
    };
#pragma pack(pop)

    // decode.
    typedef enum {
        BLOCK_DECODE_STATE_READY = 0,
        BLOCK_DECODE_STATE_STALL_ON_INPUT,
        BLOCK_DECODE_STATE_STALL_ON_OUTPUT,
        BLOCK_DECODE_STATE_INTEGRITY_CHECK_FAIL,
        BLOCK_DECODE_STATE_ERROR
    } BLOCK_DECODE_STATE;

    typedef enum {
        BLOCK_DECODE_PROCESS_READ_BLOCK_HEADER,
        BLOCK_DECODE_PROCESS_READ_BLOCK_MODE_MARKER,
        BLOCK_DECODE_PROCESS_READ_BLOCK_FOOTER,
        BLOCK_DECODE_PROCESS_READ_DATA,
    } BLOCK_DECODE_PROCESS;

    typedef enum {
        KERNEL_DECODE_STATE_READY = 0,
        KERNEL_DECODE_STATE_INFO_NEW_BLOCK,
        KERNEL_DECODE_STATE_INFO_EFFICIENCY_CHECK,
        KERNEL_DECODE_STATE_STALL_ON_INPUT,
        KERNEL_DECODE_STATE_STALL_ON_OUTPUT,
        KERNEL_DECODE_STATE_ERROR
    } KERNEL_DECODE_STATE;

#pragma pack(push)
#pragma pack(4)
    class block_decode_state_t {
    public:
        BLOCK_DECODE_PROCESS process;
        COMPRESSION_MODE targetMode;
        COMPRESSION_MODE currentMode;
        BLOCK_TYPE blockType;

        uint_fast64_t totalRead;
        uint_fast64_t totalWritten;
        uint_fast8_t endDataOverhead;

        bool readBlockHeaderContent;
        block_header_t lastBlockHeader;
        block_mode_marker_t lastModeMarker;
        block_footer_t lastBlockFooter;

        // current_block_data.
        uint_fast64_t inStart;
        uint_fast64_t outStart;

        // integrity_data.
        bool update;
        uint8_t *outputPointer;
        spookyhash_context_t context;

        void *kernelDecodeState;

        inline BLOCK_DECODE_STATE
        init(const COMPRESSION_MODE mode, const BLOCK_TYPE blockType,
             const main_header_parameters_t parameters, const uint_fast8_t endDataOverhead,
             void *kernelState)
        {
            targetMode = currentMode = mode;
            this->blockType = blockType;
            readBlockHeaderContent = parameters.as_bytes[0] ? true: false;
            totalRead = totalWritten = 0;
            this->endDataOverhead = endDataOverhead;

            if (blockType == BLOCK_TYPE_WITH_HASHSUM_INTEGRITY_CHECK) {
                update = true;
                this->endDataOverhead += sizeof(block_footer_t);
            }

            switch (currentMode) {
            case COMPRESSION_MODE_COPY: break;
            default:
                this->kernelDecodeState = kernelState;
                _init(this->kernelDecodeState, parameters, this->endDataOverhead);
                break;
            }
            return exit_process(BLOCK_DECODE_PROCESS_READ_BLOCK_HEADER,
                                BLOCK_DECODE_STATE_READY);
        }

        inline BLOCK_DECODE_STATE
        continue_(memory_teleport_t *RESTRICT in, memory_location_t *RESTRICT out)
        {
            KERNEL_DECODE_STATE kernelDecodeState;
            BLOCK_DECODE_STATE blockDecodeState;
            uint_fast64_t inAvailableBefore;
            uint_fast64_t outAvailableBefore;
            uint_fast64_t blockRemaining;
            uint_fast64_t inRemaining;
            uint_fast64_t outRemaining;

            // Update integrity pointers
            if (blockType == BLOCK_TYPE_WITH_HASHSUM_INTEGRITY_CHECK && update)
                update_integrity_data(out);

            // Dispatch
            switch (process) {
            case BLOCK_DECODE_PROCESS_READ_BLOCK_HEADER: goto read_block_header;
            case BLOCK_DECODE_PROCESS_READ_BLOCK_MODE_MARKER: goto read_mode_marker;
            case BLOCK_DECODE_PROCESS_READ_DATA: goto read_data;
            case BLOCK_DECODE_PROCESS_READ_BLOCK_FOOTER: goto read_block_footer;
            default: return BLOCK_DECODE_STATE_ERROR;
            }

        read_mode_marker:
            if ((blockDecodeState = read_block_mode_marker(in)))
                return exit_process(BLOCK_DECODE_PROCESS_READ_BLOCK_MODE_MARKER,
                                    blockDecodeState);
            goto read_data;

        read_block_header:
            if ((blockDecodeState = read_block_header(in, out)))
                return exit_process(BLOCK_DECODE_PROCESS_READ_BLOCK_HEADER, blockDecodeState);

        read_data:
            inAvailableBefore = in->available_bytes_reserved(endDataOverhead);
            outAvailableBefore = out->available_bytes;

            switch (currentMode) {
            case COMPRESSION_MODE_COPY:
                blockRemaining = (uint_fast64_t)DENSITY_PREFERRED_COPY_BLOCK_SIZE - (totalWritten - outStart);
                inRemaining = in->available_bytes_reserved(endDataOverhead);
                outRemaining = out->available_bytes;

                if (inRemaining <= outRemaining) {
                    if (blockRemaining <= inRemaining) goto copy_until_end_of_block;
                    else {
                        in->copy(out, inRemaining);
                        update_totals(in, out, inAvailableBefore, outAvailableBefore);
                        in->copy_from_direct_buffer_to_staging_buffer();
                        return exit_process(BLOCK_DECODE_PROCESS_READ_DATA,
                                            BLOCK_DECODE_STATE_STALL_ON_INPUT);
                    }
                } else {
                    if (blockRemaining <= outRemaining) goto copy_until_end_of_block;
                    else {
                        in->copy(out, outRemaining);
                        update_totals(in, out, inAvailableBefore, outAvailableBefore);
                        if (blockType == BLOCK_TYPE_WITH_HASHSUM_INTEGRITY_CHECK)
                            update_integrity_hash(out, true);
                        return exit_process(BLOCK_DECODE_PROCESS_READ_DATA,
                                            BLOCK_DECODE_STATE_STALL_ON_OUTPUT);
                    }
                }

            copy_until_end_of_block:
                in->copy(out, blockRemaining);
                update_totals(in, out, inAvailableBefore, outAvailableBefore);
                goto read_block_footer;

            default:
                kernelDecodeState = _process(in, out, this->kernelDecodeState);
                update_totals(in, out, inAvailableBefore, outAvailableBefore);

                switch (kernelDecodeState) {
                case KERNEL_DECODE_STATE_STALL_ON_INPUT:
                    return exit_process(BLOCK_DECODE_PROCESS_READ_DATA,
                                        BLOCK_DECODE_STATE_STALL_ON_INPUT);
                case KERNEL_DECODE_STATE_STALL_ON_OUTPUT:
                    if (blockType == BLOCK_TYPE_WITH_HASHSUM_INTEGRITY_CHECK)
                        update_integrity_hash(out, true);
                    return exit_process(BLOCK_DECODE_PROCESS_READ_DATA,
                                        BLOCK_DECODE_STATE_STALL_ON_OUTPUT);
                case KERNEL_DECODE_STATE_INFO_NEW_BLOCK: goto read_block_footer;
                case KERNEL_DECODE_STATE_INFO_EFFICIENCY_CHECK: goto read_mode_marker;
                default: return BLOCK_DECODE_STATE_ERROR;
                }
            }
        read_block_footer:
            if (blockType == BLOCK_TYPE_WITH_HASHSUM_INTEGRITY_CHECK)
                if ((blockDecodeState = read_block_footer(in, out)))
                    return exit_process(BLOCK_DECODE_PROCESS_READ_BLOCK_FOOTER,
                                        blockDecodeState);
            goto read_block_header;
        }

        inline BLOCK_DECODE_STATE
        finish(memory_teleport_t *RESTRICT in, memory_location_t *RESTRICT out)
        {
            KERNEL_DECODE_STATE kernelDecodeState;
            BLOCK_DECODE_STATE blockDecodeState;
            uint_fast64_t inAvailableBefore;
            uint_fast64_t outAvailableBefore;
            uint_fast64_t blockRemaining;
            uint_fast64_t inRemaining;
            uint_fast64_t outRemaining;

            // Update integrity pointers
            if (blockType == BLOCK_TYPE_WITH_HASHSUM_INTEGRITY_CHECK && update)
                update_integrity_data(out);

            // Dispatch
            switch (process) {
            case BLOCK_DECODE_PROCESS_READ_BLOCK_HEADER: goto read_block_header;
            case BLOCK_DECODE_PROCESS_READ_BLOCK_MODE_MARKER: goto read_mode_marker;
            case BLOCK_DECODE_PROCESS_READ_DATA: goto read_data;
            case BLOCK_DECODE_PROCESS_READ_BLOCK_FOOTER: goto read_block_footer;
            default: return BLOCK_DECODE_STATE_ERROR;
            }

        read_mode_marker:
            if ((blockDecodeState = read_block_mode_marker(in)))
                return exit_process(BLOCK_DECODE_PROCESS_READ_BLOCK_MODE_MARKER,
                                    blockDecodeState);
            goto read_data;

        read_block_header:
            if ((blockDecodeState = read_block_header(in, out)))
                return exit_process(BLOCK_DECODE_PROCESS_READ_BLOCK_HEADER, blockDecodeState);

        read_data:
            inAvailableBefore = in->available_bytes_reserved(endDataOverhead);
            outAvailableBefore = out->available_bytes;

            switch (currentMode) {
            case COMPRESSION_MODE_COPY:
                blockRemaining = (uint_fast64_t)DENSITY_PREFERRED_COPY_BLOCK_SIZE - (totalWritten - outStart);
                inRemaining = in->available_bytes_reserved(endDataOverhead);
                outRemaining = out->available_bytes;

                if (inRemaining <= outRemaining) {
                    if (blockRemaining <= inRemaining) goto copy_until_end_of_block;
                    else {
                        in->copy(out, inRemaining);
                        update_totals(in, out, inAvailableBefore, outAvailableBefore);
                        goto read_block_footer;
                    }
                } else {
                    if (blockRemaining <= outRemaining) goto copy_until_end_of_block;
                    else {
                        in->copy(out, outRemaining);
                        update_totals(in, out, inAvailableBefore, outAvailableBefore);
                        if (blockType == BLOCK_TYPE_WITH_HASHSUM_INTEGRITY_CHECK)
                            update_integrity_hash(out, true);
                        return exit_process(BLOCK_DECODE_PROCESS_READ_DATA,
                                            BLOCK_DECODE_STATE_STALL_ON_OUTPUT);
                    }
                }

            copy_until_end_of_block:
                in->copy(out, blockRemaining);
                update_totals(in, out, inAvailableBefore, outAvailableBefore);
                goto read_block_footer;

            default:
                kernelDecodeState = _finish(in, out, this->kernelDecodeState);
                update_totals(in, out, inAvailableBefore, outAvailableBefore);

                switch (kernelDecodeState) {
                case KERNEL_DECODE_STATE_STALL_ON_INPUT: return BLOCK_DECODE_STATE_ERROR;
                case KERNEL_DECODE_STATE_STALL_ON_OUTPUT:
                    if (blockType == BLOCK_TYPE_WITH_HASHSUM_INTEGRITY_CHECK)
                        update_integrity_hash(out, true);
                    return exit_process(BLOCK_DECODE_PROCESS_READ_DATA,
                                        BLOCK_DECODE_STATE_STALL_ON_OUTPUT);
                case KERNEL_DECODE_STATE_READY:
                case KERNEL_DECODE_STATE_INFO_NEW_BLOCK: goto read_block_footer;
                case KERNEL_DECODE_STATE_INFO_EFFICIENCY_CHECK: goto read_mode_marker;
                default: return BLOCK_DECODE_STATE_ERROR;
                }
            }

        read_block_footer:
            if (blockType == BLOCK_TYPE_WITH_HASHSUM_INTEGRITY_CHECK)
                if ((blockDecodeState = read_block_footer(in, out)))
                    return exit_process(BLOCK_DECODE_PROCESS_READ_BLOCK_FOOTER,
                                        blockDecodeState);
            if (in->available_bytes_reserved(endDataOverhead)) goto read_block_header;
            //if (blockType == BLOCK_TYPE_WITH_HASHSUM_INTEGRITY_CHECK) delete context;
            return BLOCK_DECODE_STATE_READY;
        }
    protected:
        virtual KERNEL_DECODE_STATE
        _init(void*, const main_header_parameters_t, const uint_fast8_t) = 0;
        virtual KERNEL_DECODE_STATE
        _process(memory_teleport_t *, memory_location_t *, void*) = 0;
        virtual KERNEL_DECODE_STATE
        _finish(memory_teleport_t *, memory_location_t *, void*) = 0;
    private:
        inline BLOCK_DECODE_STATE
        exit_process(BLOCK_DECODE_PROCESS process, BLOCK_DECODE_STATE blockDecodeState)
        {   this->process = process; return blockDecodeState; }
        inline void
        update_integrity_data(memory_location_t *RESTRICT out)
        {   outputPointer = out->pointer; update = false; }
        inline void
        update_integrity_hash(memory_location_t *RESTRICT out, bool pendingExit)
        {   const uint8_t *const pointerBefore = outputPointer;
            const uint8_t *const pointerAfter = out->pointer;
            const uint_fast64_t processed = pointerAfter - pointerBefore;

            context.update(outputPointer, processed);
            if (pendingExit) update = true;
            else update_integrity_data(out);
        }
        inline BLOCK_DECODE_STATE
        read_block_header(memory_teleport_t *RESTRICT in, memory_location_t *out)
        {
            memory_location_t *readLocation;
            if (!(readLocation = in->read_reserved(sizeof(lastBlockHeader), endDataOverhead)))
                return BLOCK_DECODE_STATE_STALL_ON_INPUT;
            inStart = totalRead;
            outStart = totalWritten;
            if (readBlockHeaderContent)
                totalRead += lastBlockHeader.read(readLocation);
            currentMode = targetMode;
            if (blockType == BLOCK_TYPE_WITH_HASHSUM_INTEGRITY_CHECK) {
                context.init(DENSITY_SPOOKYHASH_SEED_1, DENSITY_SPOOKYHASH_SEED_2);
                update_integrity_data(out);
            }
            return BLOCK_DECODE_STATE_READY;
        }
        inline BLOCK_DECODE_STATE
        read_block_mode_marker(memory_teleport_t *RESTRICT in)
        {
            memory_location_t *readLocation;
            if (!(readLocation = in->read_reserved(sizeof(lastModeMarker), endDataOverhead)))
                return BLOCK_DECODE_STATE_STALL_ON_INPUT;
            totalRead += lastModeMarker.read(readLocation);
            currentMode = (COMPRESSION_MODE)lastModeMarker.mode;
            return BLOCK_DECODE_STATE_READY;
        }
        inline BLOCK_DECODE_STATE
        read_block_footer(memory_teleport_t *RESTRICT in, memory_location_t *RESTRICT out)
        {
            memory_location_t *readLocation;
            if (!(readLocation = in->read(sizeof(lastBlockFooter))))
                return BLOCK_DECODE_STATE_STALL_ON_INPUT;
            update_integrity_hash(out, false);
            uint64_t hashsum1, hashsum2;
            context.final(&hashsum1, &hashsum2);
            totalRead += lastBlockFooter.read(readLocation);
            if (lastBlockFooter.hashsum1 != hashsum1 || lastBlockFooter.hashsum2 != hashsum2)
                return BLOCK_DECODE_STATE_INTEGRITY_CHECK_FAIL;
            return BLOCK_DECODE_STATE_READY;
        }
        inline void
        update_totals(memory_teleport_t *RESTRICT in, memory_location_t *RESTRICT out,
                      const uint_fast64_t inAvailableBefore,
                      const uint_fast64_t outAvailableBefore)
        {
            totalRead += inAvailableBefore - in->available_bytes_reserved(endDataOverhead);
            totalWritten += outAvailableBefore - out->available_bytes;
        }
    };
}
