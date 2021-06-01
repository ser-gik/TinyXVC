/*
 * Copyright 2021 Sergey Guralnik
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "txvc/driver.h"
#include "txvc/jtag_splitter.h"
#include "txvc/log.h"
#include "txvc/defs.h"
#include "txvc/bit_vector.h"
#include "txvc/mempool.h"

#include <ftd2xx.h>

#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>

TXVC_DEFAULT_LOG_TAG(ftdiGeneric);

/*
 * Driver configuration loader.
 */
#define FTDI_SUPPORTED_DEVICES_LIST_ITEMS(X)                                                       \
    X("ft2232h", FT_DEVICE_2232H, "FT2232H chip")                                                  \
    X("ft232h", FT_DEVICE_232H, "FT232H chip")                                                     \

#define FTDI_INTERFACE_LIST_ITEMS(X)                                                               \
    X("A", 'A', "FTDI' ADBUS channel")                                                             \
    X("B", 'B', "FTDI' BDBUS channel")                                                             \

#define PIN_ROLE_LIST_ITEMS(X)                                                                     \
    X("driver_low", PIN_ROLE_OTHER_DRIVER_LOW, "permanent low level driver")                       \
    X("driver_high", PIN_ROLE_OTHER_DRIVER_HIGH, "permanent high level driver")                    \
    X("ignored", PIN_ROLE_OTHER_IGNORED, "ignored pin, configured as input")                       \

#define AS_ENUM_MEMBER(name, enumVal, descr) enumVal,

enum pin_role {
    PIN_ROLE_INVALID = 0,
    PIN_ROLE_LIST_ITEMS(AS_ENUM_MEMBER)
};

#undef AS_ENUM_MEMBER

#define RETURN_ENUM_IF_NAME_MATCHES(name, enumVal, descr) if (strcmp(name, s) == 0) return enumVal;

static FT_DEVICE str_to_ft_device(const char *s) {
    FTDI_SUPPORTED_DEVICES_LIST_ITEMS(RETURN_ENUM_IF_NAME_MATCHES)
    return -1;
}

static char str_to_ftdi_interface(const char *s) {
    FTDI_INTERFACE_LIST_ITEMS(RETURN_ENUM_IF_NAME_MATCHES)
    return '?';
}

static enum pin_role str_to_pin_role(const char *s) {
    PIN_ROLE_LIST_ITEMS(RETURN_ENUM_IF_NAME_MATCHES)
    return PIN_ROLE_INVALID;
}

#undef RETURN_ENUM_IF_NAME_MATCHES

static int str_to_usb_id(const char *s) {
    char *endp;
    long res = strtol(s, &endp, 16);
    return *endp != '\0' ||  res <= 0l || res > 0xffffl ? -1 : (int) res;
}

struct ft_params {
    FT_DEVICE device;
    int vid;
    int pid;
    char channel;
    enum pin_role d_pins[8];
};

#define PARAM_LIST_ITEMS(X)                                                                        \
    X("device", device, str_to_ft_device, > 0, "FTDI chip type")                                   \
    X("vid", vid, str_to_usb_id, > 0, "USB device vendor ID")                                      \
    X("pid", pid, str_to_usb_id, > 0, "USB device product ID")                                     \
    X("channel", channel, str_to_ftdi_interface, != '?', "FTDI channel to use")                    \
    X("d4", d_pins[4], str_to_pin_role, != PIN_ROLE_INVALID, "D4 pin role")                        \
    X("d5", d_pins[5], str_to_pin_role, != PIN_ROLE_INVALID, "D5 pin role")                        \
    X("d6", d_pins[6], str_to_pin_role, != PIN_ROLE_INVALID, "D6 pin role")                        \
    X("d7", d_pins[7], str_to_pin_role, != PIN_ROLE_INVALID, "D7 pin role")                        \

static bool load_config(int numArgs, const char **argNames, const char **argValues,
                            struct ft_params *out) {
    memset(out, 0, sizeof(*out));
    out->device = -1;
    out->channel = '?';

    for (int i = 0; i < numArgs; i++) {
#define CONVERT_AND_SET_IF_MATCHES(name, configField, converterFunc, validation, descr)            \
        if (strcmp(name, argNames[i]) == 0) {                                                      \
            out->configField = converterFunc(argValues[i]);                                        \
            continue;                                                                              \
        }
        PARAM_LIST_ITEMS(CONVERT_AND_SET_IF_MATCHES)
#undef CONVERT_AND_SET_IF_MATCHES
        WARN("Unknown parameter: \"%s\"=\"%s\"\n", argNames[i], argValues[i]);
    }

#define BAIL_IF_NOT_VALID(name, configField, converterFunc, validation, descr)                     \
    if (!(out->configField validation)) {                                                          \
        ERROR("Bad or missing \"%s\"\n", name);                                                    \
        return false;                                                                              \
    }
    PARAM_LIST_ITEMS(BAIL_IF_NOT_VALID)
#undef BAIL_IF_NOT_VALID
    return true;
}

/*
 * Driver implementation.
 */
static const uint8_t OP_SHIFT_WR_FALLING_FLAG = 1u << 0;
static const uint8_t OP_SHIFT_BITMODE_FLAG = 1u << 1;
/* static const uint8_t OP_SHIFT_RD_FALLING_FLAG = 1u << 2; */
static const uint8_t OP_SHIFT_LSB_FIRST_FLAG = 1u << 3;
static const uint8_t OP_SHIFT_WR_TDI_FLAG = 1u << 4;
static const uint8_t OP_SHIFT_RD_TDO_FLAG = 1u << 5;
static const uint8_t OP_SHIFT_WR_TMS_FLAG = 1u << 6;

static const uint8_t OP_SET_DBUS_LOBYTE = 0x80u;
static const uint8_t OP_SET_TCK_DIVISOR = 0x86u;
static const uint8_t OP_DISABLE_CLK_DIVIDE_BY_5 = 0x8au;

struct readonly_granule {
    const uint8_t* data;
    int size;
};

struct granule {
    uint8_t* data;
    int size;
};

struct bitcopy_params {
    const uint8_t* src;
    int fromBit;
    uint8_t* dst;
    int toBit;
    int numBits;
};

#define GRANULES_BUFFER_CAPACITY 1000

struct granules_buffer {
    struct readonly_granule outgoing[GRANULES_BUFFER_CAPACITY];
    int numOutgoingGranules;
    int totalOutgoingBytes;
    struct granule incoming[GRANULES_BUFFER_CAPACITY];
    int numIncomingGranules;
    int totalIncomingBytes;
    struct bitcopy_params postTransferCopy[GRANULES_BUFFER_CAPACITY];
    int numPostTransferCopies;

    bool lastOutgoingIsOpen;
};

struct driver {
    struct ft_params params;
    int chipBufferBytes;
    bool highSpeedCapable;
    FT_HANDLE ftHandle;
    struct txvc_jtag_splitter jtagSplitter;
    struct txvc_mempool granulesPool;
    struct txvc_spanpool compactionPool;
    struct granules_buffer granulesBuffer;
    unsigned lastTdi : 1;
};

static struct driver gFtdi;


static inline int min(int a, int b) {
    return a < b ? a : b;
}

static inline bool get_bit(const uint8_t* p, int idx) {
    return !!(p[idx / 8] & (1 << (idx % 8)));
}

static inline void set_bit(uint8_t* p, int idx, bool bit) {
    uint8_t* octet = p + idx / 8;
    if (bit) *octet |= 1 << (idx % 8);
    else *octet &= ~(1 << (idx % 8));
}

static void copy_bits(const uint8_t* src, int fromIdx,
        uint8_t* dst, int toIdx, int numBits, bool duplicateLastBit) {
    for (int i = 0; i < numBits; i++) {
        set_bit(dst, toIdx++, get_bit(src, fromIdx++));
    }
    if (duplicateLastBit) {
        set_bit(dst, toIdx, get_bit(src, fromIdx - 1));
    }
}

static const char *ftStatusName(FT_STATUS s) {
    switch (s) {
#define CASE(val) case val: return #val
        CASE(FT_OK);
        CASE(FT_INVALID_HANDLE);
        CASE(FT_DEVICE_NOT_FOUND);
        CASE(FT_DEVICE_NOT_OPENED);
        CASE(FT_IO_ERROR);
        CASE(FT_INSUFFICIENT_RESOURCES);
        CASE(FT_INVALID_PARAMETER);
        CASE(FT_INVALID_BAUD_RATE);
        CASE(FT_DEVICE_NOT_OPENED_FOR_ERASE);
        CASE(FT_DEVICE_NOT_OPENED_FOR_WRITE);
        CASE(FT_FAILED_TO_WRITE_DEVICE);
        CASE(FT_EEPROM_READ_FAILED);
        CASE(FT_EEPROM_WRITE_FAILED);
        CASE(FT_EEPROM_ERASE_FAILED);
        CASE(FT_EEPROM_NOT_PRESENT);
        CASE(FT_EEPROM_NOT_PROGRAMMED);
        CASE(FT_INVALID_ARGS);
        CASE(FT_NOT_SUPPORTED);
        CASE(FT_OTHER_ERROR);
        CASE(FT_DEVICE_LIST_NOT_READY);
#undef CASE
        default:
            return "???";
    }
}

static bool do_transfer_granular(FT_HANDLE ft,
        const struct readonly_granule* outgoingGranules, int numOutgoingGranules,
        const struct granule* incomingGranules, int numIncomingGranules) {
    for (int i = 0; i < numOutgoingGranules; i++) {
        const struct readonly_granule* g = outgoingGranules + i;
        if (g->size > 0) {
            DWORD written;
            FT_STATUS status = FT_Write(ft, (LPVOID *) g->data, g->size, &written);
            if (!FT_SUCCESS(status)) {
                ERROR("Failed to send data: %s\n", ftStatusName(status));
                return false;
            }
            if (written != (DWORD) g->size) {
                ERROR("Sent only %u bytes of %d\n", written, g->size);
                return false;
            }
        }
    }

    for (int i = 0; i < numIncomingGranules; i++) {
        const struct granule* g = incomingGranules + i;
        if (g->size > 0) {
            DWORD read;
            FT_STATUS status = FT_Read(ft, g->data, g->size, &read);
            if (!FT_SUCCESS(status)) {
                ERROR("Failed to receive data: %s\n", ftStatusName(status));
                return false;
            }
            if (read != (DWORD) g->size) {
                ERROR("Received only %u bytes of %d\n", read, g->size);
                return false;
            }
        }
    }
    return true;
}

static bool do_transfer(FT_HANDLE ft,
        const uint8_t* outgoing, int outgoingSz,
        uint8_t* incoming, int incomingSz) {
    const struct readonly_granule out = { .data = outgoing, .size = outgoingSz, };
    const struct granule in = { .data = incoming, .size = incomingSz, };
    return do_transfer_granular(ft, &out, 1, &in, 1);
}

static bool ensure_synced(FT_HANDLE ft) {
    /* Send bad opcode and check that chip responds with "BadCommand" */
    unsigned char resp[2];
    return do_transfer(ft, (unsigned char[]){ 0xab, }, 1, resp, sizeof(resp))
        && resp[0] == 0xfa && resp[1] == 0xab;
}

static void resetGranulesBuffer(struct granules_buffer *gb) {
    gb->numOutgoingGranules = 0;
    gb->totalOutgoingBytes = 0;
    gb->numIncomingGranules = 0;
    gb->totalIncomingBytes = 0;
    gb->numPostTransferCopies = 0;
    gb->lastOutgoingIsOpen = false;
}

static int maxGranuleSize(const struct driver *d) {
    return d->chipBufferBytes;
}

static bool flushGranulesBuffer(struct driver *d) {
    struct granules_buffer *gb = &d->granulesBuffer;
    VERBOSE("Flushing granules: %d outgoing (%d bytes), %d incoming (%d bytes)"
            ", %d post-transfer copy tasks\n",
            gb->numOutgoingGranules, gb->totalOutgoingBytes,
            gb->numIncomingGranules, gb->totalIncomingBytes,
            gb->numPostTransferCopies);
    if (gb->numIncomingGranules >= GRANULES_BUFFER_CAPACITY
            || gb->numOutgoingGranules >= GRANULES_BUFFER_CAPACITY) {
        WARN("%s: buffer is full, consider increasing capacity\n", __func__);
    }

    if (gb->lastOutgoingIsOpen) {
        txvc_spanpool_close_span(&d->compactionPool);
        gb->lastOutgoingIsOpen = false;
    }
    if (!do_transfer_granular(d->ftHandle, gb->outgoing, gb->numOutgoingGranules,
                gb->incoming, gb->numIncomingGranules)) {
        return false;
    }

    for (int i = 0; i < gb->numPostTransferCopies; i++) {
        const struct bitcopy_params *bp = &gb->postTransferCopy[i];
        copy_bits(bp->src, bp->fromBit, bp->dst, bp->toBit, bp->numBits, false);
    }
    resetGranulesBuffer(gb);
    return true;
}

static bool appendToGranuleBuffer(struct driver *d,
        const uint8_t *outgoing, int outgoingBytes,
        uint8_t *incoming, int incomingBytes,
        const struct bitcopy_params *postTransferCopy) {
    ALWAYS_ASSERT(outgoingBytes <= maxGranuleSize(d));
    ALWAYS_ASSERT(incomingBytes <= maxGranuleSize(d));

    VERBOSE("Appening granule: %d outgoing bytes, %d incoming bytes, %s post-transfer copy task\n",
            outgoingBytes, incomingBytes, postTransferCopy ? "with" : "no");
    struct granules_buffer *gb = &d->granulesBuffer;
    bool flush = false;
    if (outgoingBytes > 0 && (gb->numOutgoingGranules >= GRANULES_BUFFER_CAPACITY
                                || gb->totalOutgoingBytes + outgoingBytes > d->chipBufferBytes)) {
        flush = true;
    }
    if (incomingBytes > 0 && (gb->numIncomingGranules >= GRANULES_BUFFER_CAPACITY
                                || gb->totalIncomingBytes + incomingBytes > d->chipBufferBytes)) {
        flush = true;
    }
    if (postTransferCopy && gb->numPostTransferCopies >= GRANULES_BUFFER_CAPACITY) {
        flush = true;
    }

    if (flush && !flushGranulesBuffer(d)) {
        return false;
    }

    if (outgoingBytes > 0) {
        if (outgoingBytes < 512) {
            /* Append to the last granule or insert new open granule */
            if (!gb->lastOutgoingIsOpen) {
                gb->outgoing[gb->numOutgoingGranules].data =
                    txvc_spanpool_open_span_unaligned(&d->compactionPool);
                gb->outgoing[gb->numOutgoingGranules].size = 0;
                gb->numOutgoingGranules++;
                gb->lastOutgoingIsOpen = true;
            }
            uint8_t *deltaBuffer = txvc_spanpool_span_enlarge(&d->compactionPool, outgoingBytes);
            memcpy(deltaBuffer, outgoing, outgoingBytes);
            gb->outgoing[gb->numOutgoingGranules - 1].size += outgoingBytes;
            gb->totalOutgoingBytes += outgoingBytes;
        } else {
            /* Append separate granule */
            if (gb->lastOutgoingIsOpen) {
                txvc_spanpool_close_span(&d->compactionPool);
                gb->lastOutgoingIsOpen = false;
            }
            gb->outgoing[gb->numOutgoingGranules].data = outgoing;
            gb->outgoing[gb->numOutgoingGranules].size = outgoingBytes;
            gb->totalOutgoingBytes += outgoingBytes;
            gb->numOutgoingGranules++;
        }
    }
    if (incomingBytes > 0) {
        gb->incoming[gb->numIncomingGranules].data = incoming;
        gb->incoming[gb->numIncomingGranules].size = incomingBytes;
        gb->totalIncomingBytes += incomingBytes;
        gb->numIncomingGranules++;
    }
    if (postTransferCopy) {
        gb->postTransferCopy[gb->numPostTransferCopies] = *postTransferCopy;
        gb->numPostTransferCopies++;
    }
    return true;
}

static bool appendTmsShiftToGranulesBuffer(struct driver *d,
        const uint8_t* tms, int fromBitIdx, int toBitIdx) {
    ALWAYS_ASSERT(fromBitIdx >= 0);
    ALWAYS_ASSERT(toBitIdx >= 0);
    ALWAYS_ASSERT(toBitIdx > fromBitIdx);

    while (fromBitIdx < toBitIdx) {
        uint8_t *cmd = txvc_mempool_alloc_unaligned(&d->granulesPool, 3);
        /*
         * In theory it is possible to send up to 7 TMS bits per command but we reserve one to
         * duplicate the last bit which is needed to guarantee that TMS wire level is unchanged
         * after command is completed.
         */
        const int maxTmsBitsPerCommand = 6;
        const int bitsToTransfer = min(toBitIdx - fromBitIdx, maxTmsBitsPerCommand);
        cmd[0] = OP_SHIFT_WR_TMS_FLAG
               | OP_SHIFT_LSB_FIRST_FLAG
               | OP_SHIFT_BITMODE_FLAG
               | OP_SHIFT_WR_FALLING_FLAG;
        cmd[1] = bitsToTransfer - 1;
        cmd[2] = (!!d->lastTdi) << 7;
        copy_bits(tms, fromBitIdx, &cmd[2], 0, bitsToTransfer, true);
        fromBitIdx += bitsToTransfer;
        if (!appendToGranuleBuffer(d, cmd, 3, NULL, 0, NULL)) {
            return false;
        }
    }
    return true;
}

static bool appendTdiShiftToGranulesBuffer(struct driver *d,
        const uint8_t* tdi, uint8_t* tdo, int fromBitIdx, int toBitIdx, bool lastTmsBitHigh) {
    ALWAYS_ASSERT(fromBitIdx >= 0);
    ALWAYS_ASSERT(toBitIdx >= 0);
    ALWAYS_ASSERT(toBitIdx > fromBitIdx);

    /*
     * To minimize copying as much as possible we divide vectors onto ranges that have their
     * adjacent boundaries at appropriate octet boundaries (i.e. multiples of 8), so that
     * a payload for transferring bytes from the middle range can be constructed by direct
     * referencing tdi and tdo buffers.
     * Ranges are:
     * - leading, 0 to 7 bits. Length is chosen in a such way that it ends at octet
     *   boundary except for when last bit (see below) falls into leading range.
     *   It's length is 0 if vectors start at octet boundary.
     * - inner, 0 or more whole octets. These are all whole vector octets between end of
     *   a leading range and the last vector bit.
     * - trailing, 0 to 7 bits. All bits between end of inner range and the last bit.
     * - last bit, 1 bit. This one is always present and must be separated because it is send
     *   via TMS command that is needed e.g. when we are exiting shift state.
     */

    const int lastBitIdx = toBitIdx - 1;
    const int numRegularBits = lastBitIdx - fromBitIdx;
    const int numFirstOctetBits = 8 - fromBitIdx % 8;
    const int numLeadingBits = min(numFirstOctetBits == 8 ? 0 : numFirstOctetBits, numRegularBits);
    const bool leadingOnly = numLeadingBits == numRegularBits;
    const int innerBeginIdx = leadingOnly ? -1 : fromBitIdx + numLeadingBits;
    const int innerEndIdx = leadingOnly ? -1 : lastBitIdx - lastBitIdx % 8;
    const int numTrailingBits = leadingOnly ? 0 : lastBitIdx % 8;

    for (int curIdx = fromBitIdx; curIdx < toBitIdx;) {
        if (curIdx == fromBitIdx && numLeadingBits > 0) {
            uint8_t *cmd = txvc_mempool_alloc_unaligned(&d->granulesPool, 3);
            cmd[0] = OP_SHIFT_RD_TDO_FLAG
                   | OP_SHIFT_WR_TDI_FLAG
                   | OP_SHIFT_LSB_FIRST_FLAG
                   | OP_SHIFT_BITMODE_FLAG
                   | OP_SHIFT_WR_FALLING_FLAG;
            cmd[1] = numLeadingBits - 1;
            cmd[2] = tdi[fromBitIdx / 8] >> (fromBitIdx % 8);
            uint8_t *received = txvc_mempool_alloc_unaligned(&d->granulesPool, 1);
            struct bitcopy_params postCopy = {
                .src = received,
                .fromBit = 8 - numLeadingBits,
                .dst = tdo,
                .toBit = fromBitIdx,
                .numBits = numLeadingBits,
            };
            if (!appendToGranuleBuffer(d, cmd, 3, received, 1, &postCopy)) {
                return false;
            }
            curIdx += numLeadingBits;
        }
        if (curIdx < lastBitIdx) {
            if (curIdx < innerEndIdx) {
                ALWAYS_ASSERT(innerBeginIdx % 8 == 0);
                ALWAYS_ASSERT(innerEndIdx % 8 == 0);
                ALWAYS_ASSERT(curIdx % 8 == 0);
                const int innerOctetsToSend = min((innerEndIdx - curIdx) / 8, maxGranuleSize(d));
                uint8_t *cmd = txvc_mempool_alloc_unaligned(&d->granulesPool, 3);
                cmd[0] = OP_SHIFT_RD_TDO_FLAG
                       | OP_SHIFT_WR_TDI_FLAG
                       | OP_SHIFT_LSB_FIRST_FLAG
                       | OP_SHIFT_WR_FALLING_FLAG;
                cmd[1] = ((innerOctetsToSend - 1) >> 0) & 0xff;
                cmd[2] = ((innerOctetsToSend - 1) >> 8) & 0xff;
                if (!appendToGranuleBuffer(d, cmd, 3, NULL, 0, NULL)) {
                    return false;
                }
                if (!appendToGranuleBuffer(d, tdi + curIdx / 8, innerOctetsToSend,
                                              tdo + curIdx / 8, innerOctetsToSend, NULL)) {
                    return false;
                }
                curIdx += innerOctetsToSend * 8;
            }
            if (curIdx == innerEndIdx && numTrailingBits > 0) {
                uint8_t *cmd = txvc_mempool_alloc_unaligned(&d->granulesPool, 3);
                cmd[0] = OP_SHIFT_RD_TDO_FLAG
                       | OP_SHIFT_WR_TDI_FLAG
                       | OP_SHIFT_LSB_FIRST_FLAG
                       | OP_SHIFT_BITMODE_FLAG
                       | OP_SHIFT_WR_FALLING_FLAG;
                cmd[1] = numTrailingBits - 1;
                cmd[2] = tdi[innerEndIdx / 8];
                uint8_t *received = txvc_mempool_alloc_unaligned(&d->granulesPool, 1);
                struct bitcopy_params postCopy = {
                    .src = received,
                    .fromBit = 8 - numTrailingBits,
                    .dst = tdo,
                    .toBit = innerEndIdx,
                    .numBits = numTrailingBits,
                };
                if (!appendToGranuleBuffer(d, cmd, 3, received, 1, &postCopy)) {
                    return false;
                }
                curIdx += numTrailingBits;
            }
        }

        if (curIdx == lastBitIdx) {
            const int lastTdiBit = !!get_bit(tdi, lastBitIdx);
            const int lastTmsBit = !!lastTmsBitHigh;
            uint8_t *cmd = txvc_mempool_alloc_unaligned(&d->granulesPool, 3);
            cmd[0] = OP_SHIFT_WR_TMS_FLAG
                   | OP_SHIFT_RD_TDO_FLAG
                   | OP_SHIFT_LSB_FIRST_FLAG
                   | OP_SHIFT_BITMODE_FLAG
                   | OP_SHIFT_WR_FALLING_FLAG;
            cmd[1] = 0x00; /* Send 1 bit */
            cmd[2] = (lastTdiBit << 7) | (lastTmsBit << 1) | lastTmsBit;
            uint8_t *received = txvc_mempool_alloc_unaligned(&d->granulesPool, 1);
            struct bitcopy_params postCopy = {
                .src = received,
                .fromBit = 7, /* TDO is shifted in from the right side */
                .dst = tdo,
                .toBit = lastBitIdx,
                .numBits = 1,
            };
            if (!appendToGranuleBuffer(d, cmd, 3, received, 1, &postCopy)) {
                return false;
            }

            /* Update global last TDI bit so that future TMS commands will use proper value when enqueued. */
            d->lastTdi = lastTdiBit;
            curIdx += 1;
        }
    }
    return true;
}

static bool jtagSplitterCallback(const struct txvc_jtag_split_event *event, void *extra) {
    struct driver *d = extra;
    {
        const struct txvc_jtag_split_shift_tms *e = txvc_jtag_split_cast_to_shift_tms(event);
        if (e) {
            return appendTmsShiftToGranulesBuffer(d, e->tms, e->fromBitIdx, e->toBitIdx);
        }
    }
    {
        const struct txvc_jtag_split_shift_tdi *e = txvc_jtag_split_cast_to_shift_tdi(event);
        if (e) {
            return appendTdiShiftToGranulesBuffer(d, e->tdi, e->tdo, e->fromBitIdx,e->toBitIdx, !e->incomplete);
        }
    }
    {
        const struct txvc_jtag_split_flush_all *e = txvc_jtag_split_cast_to_flush_all(event);
        if (e) {
            bool res = flushGranulesBuffer(d);
            txvc_spanpool_reclaim_all(&d->compactionPool);
            txvc_mempool_reclaim_all(&d->granulesPool);
            return res;
        }
    }
    TXVC_UNREACHABLE();
}

static bool activate(int numArgs, const char **argNames, const char **argValues){
    struct driver *d = &gFtdi;

    if (!load_config(numArgs, argNames, argValues, &d->params)) goto bail_noop;
    char channelSelector = d->params.channel;
    switch (d->params.device) {
        case FT_DEVICE_2232H:
            d->chipBufferBytes = 4096;
            d->highSpeedCapable = true;
            if (channelSelector != 'A' && channelSelector != 'B') {
                ERROR("Bad channel\n");
                goto bail_noop;
            }
            break;
        case FT_DEVICE_232H:
            d->chipBufferBytes = 1024;
            d->highSpeedCapable = true;
            if (channelSelector != 'A') {
                ERROR("Bad channel\n");
                goto bail_noop;
            }
            channelSelector = 0; /* Single-port device has no channel name in their identifiers */
            break;
        default:
            ERROR("Unknown chip type: %d\n", d->params.device);
            goto bail_noop;
    }

    FT_STATUS lastStatus = FT_OK;

#define REQUIRE_D2XX_SUCCESS_(d2xxCallExpr, cleanupLabel)                                          \
    do {                                                                                           \
        lastStatus = (d2xxCallExpr);                                                               \
        if (lastStatus != FT_OK) {                                                                 \
            ERROR("Failed: %s: %s\n", #d2xxCallExpr, ftStatusName(lastStatus));                    \
            goto cleanupLabel;                                                                     \
        }                                                                                          \
    } while (0)

    DWORD ver;
    REQUIRE_D2XX_SUCCESS_(FT_GetLibraryVersion(&ver), bail_noop);
    INFO("Using d2xx v.%x.%x.%x\n", (ver >> 16) & 0xffu, (ver >> 8) & 0xffu, (ver >> 0) & 0xffu);

    REQUIRE_D2XX_SUCCESS_(FT_SetVIDPID(d->params.vid,d->params.pid), bail_noop);
    FT_DEVICE_LIST_INFO_NODE connectedDevices[4];
    DWORD numConnectedDevices = sizeof(connectedDevices) / sizeof(connectedDevices[0]);
    REQUIRE_D2XX_SUCCESS_(FT_CreateDeviceInfoList(&numConnectedDevices), bail_noop);
    REQUIRE_D2XX_SUCCESS_(FT_GetDeviceInfoList(connectedDevices, &numConnectedDevices), bail_noop);
    char *serialNumber = NULL;
    if (!channelSelector) {
        serialNumber = connectedDevices[0].SerialNumber;
    } else {
        for (DWORD i = 0; i < numConnectedDevices; i++) {
            char *curSerial = connectedDevices[i].SerialNumber;
            size_t curSerialLen = strlen(curSerial);
            if (channelSelector == curSerial[curSerialLen - 1]) {
                serialNumber = curSerial;
                break;
            }
        }
    }
    REQUIRE_D2XX_SUCCESS_(FT_OpenEx(serialNumber, FT_OPEN_BY_SERIAL_NUMBER, &d->ftHandle), bail_cant_open);

    REQUIRE_D2XX_SUCCESS_(FT_Purge(d->ftHandle, FT_PURGE_RX | FT_PURGE_TX),  bail_usb_close);
    REQUIRE_D2XX_SUCCESS_(FT_SetChars(d->ftHandle, 0, 0, 0, 0), bail_usb_close);
    REQUIRE_D2XX_SUCCESS_(FT_SetFlowControl(d->ftHandle, FT_FLOW_RTS_CTS, 0, 0), bail_usb_close);
    REQUIRE_D2XX_SUCCESS_(FT_SetBitMode(d->ftHandle, 0x00, FT_BITMODE_RESET), bail_usb_close);
    REQUIRE_D2XX_SUCCESS_(FT_SetBitMode(d->ftHandle, 0x00, FT_BITMODE_MPSSE), bail_usb_close);

#undef REQUIRE_D2XX_SUCCESS_

    txvc_mempool_init(&d->granulesPool, 64 * 1024);
    txvc_spanpool_init(&d->compactionPool, 64 * 1024);
    resetGranulesBuffer(&d->granulesBuffer);

    uint8_t setupCmds[] = {
        OP_SET_DBUS_LOBYTE,
        0x08, /* Initial levels: TCK=0, TDI=0, TMS=1 */
        0x0b, /* Directions: TCK=out, TDI=out, TDO=in, TMS=out */
        OP_SET_TCK_DIVISOR,
        0x05, /* Initialize to 1 MHz clock */
        0x00,
    };
    /* Append user choices for D4-D7 */
    for (int i = 4; i < 8; i++) {
        switch (d->params.d_pins[i]) {
            case PIN_ROLE_OTHER_DRIVER_HIGH:
                setupCmds[1] |= 1u << i;
                setupCmds[2] |= 1u << i;
                break;
            case PIN_ROLE_OTHER_DRIVER_LOW:
                setupCmds[2] |= 1u << i;
                break;
            case PIN_ROLE_OTHER_IGNORED:
                /* Nothing to do */
                break;
            default:
                TXVC_UNREACHABLE();
        }
    }
    if (!do_transfer(d->ftHandle, setupCmds, sizeof(setupCmds), NULL, 0) || !ensure_synced(d->ftHandle)) {
        ERROR("Failed to setup device\n");
        goto bail_reset_mode;
    }
    d->lastTdi = 0;

    if (!txvc_jtag_splitter_init(&d->jtagSplitter, jtagSplitterCallback, d)) {
        goto bail_reset_mode;
    }
    return true;

bail_reset_mode:
    FT_SetBitMode(d->ftHandle, 0x00, FT_BITMODE_RESET);
bail_usb_close:
    FT_Close(d->ftHandle);
bail_cant_open:
    if (lastStatus == FT_DEVICE_NOT_OPENED) {
        ERROR("--------------------------------------------\n");
        ERROR(" Did you forgot to \"sudo rmmod ftdi_sio\"?\n");
        ERROR("--------------------------------------------\n");
    }
bail_noop:
    return false;
}

static bool deactivate(void){
    struct driver *d = &gFtdi;
    txvc_jtag_splitter_deinit(&d->jtagSplitter);
    txvc_spanpool_deinit(&d->compactionPool);
    txvc_mempool_deinit(&d->granulesPool);
    FT_SetBitMode(d->ftHandle, 0x00, FT_BITMODE_RESET);
    FT_Close(d->ftHandle);
    return true;
}

static int max_vector_bits(void){
    struct driver *d = &gFtdi;
    return d->chipBufferBytes * 8;
}

static int set_tck_period(int tckPeriodNs){
    /*
     * Find out needed divider by using official formula from FTDI docs:
     * TCK/SK period = 12MHz / (( 1 +[(0xValueH * 256) OR 0xValueL] ) * 2)
     * or, if high speed available:
     * TCK period = 60MHz / (( 1 +[ (0xValueH * 256) OR 0xValueL] ) * 2)
     * Strictly speaking these formulae yield frequency, not a period.
     */
    struct driver *d = &gFtdi;
    const int maxFreqMHz = d->highSpeedCapable ? 30 : 6;
    /* Use nearest greater period if there is no exact match */
    int divider = (maxFreqMHz * tckPeriodNs) / 1000 - (!((maxFreqMHz * tckPeriodNs) % 1000));
    if (divider < 0) {
        TXVC_UNREACHABLE();
    }
    if (divider > 0xffff) {
        divider = 0xffff;
    }
    int actualPeriodNs = (1 + divider) * 1000 / maxFreqMHz;
    if (divider == 0) {
        WARN("Using minimal available period: %dns\n", actualPeriodNs);
    }
    if (divider == 0xffff) {
        WARN("Using maximal available period: %dns\n", actualPeriodNs);
    }
    const uint8_t cmd[] = {
        OP_SET_TCK_DIVISOR,
        divider & 0xff,
        (divider >> 8) & 0xff,
        OP_DISABLE_CLK_DIVIDE_BY_5,
    };
    if (!do_transfer(d->ftHandle, cmd, d->highSpeedCapable ? 4 : 3, NULL, 0)
            || !ensure_synced(d->ftHandle)) {
        ERROR("Can't set TCK period %dns\n", tckPeriodNs);
        actualPeriodNs = -1;
    }
    return actualPeriodNs;
}

static bool shift_bits(int numBits, const uint8_t *tmsVector, const uint8_t *tdiVector,
        uint8_t *tdoVector){
    struct driver *d = &gFtdi;
    return txvc_jtag_splitter_process(&d->jtagSplitter, numBits, tmsVector, tdiVector, tdoVector);
}

const struct txvc_driver driver_ftdi_generic = {
    .name = "ftdi-generic",
    .help =
        "Sends vectors to a device that is connected to JTAG pins of a MPSSE-capable FTDI chip,"
        " which is connected to this machine USB\n"
        "JTAG pins:\n"
        "  \"d0\" - TCK\n"
        "  \"d1\" - TDI\n"
        "  \"d2\" - TDO\n"
        "  \"d3\" - TMS\n"
        "Parameters:\n"
#define AS_HELP_STRING(name, configField, converterFunc, validation, descr) \
        "  \"" name "\" - " descr "\n"
        PARAM_LIST_ITEMS(AS_HELP_STRING)
#undef AS_HELP_STRING
#define AS_HELP_STRING(name, enumVal, descr) \
        "  \"" name "\" - " descr "\n"
        "Allowed pin roles:\n"
        PIN_ROLE_LIST_ITEMS(AS_HELP_STRING)
        "Allowed FTDI channels:\n"
        FTDI_INTERFACE_LIST_ITEMS(AS_HELP_STRING)
        "Allowed chip types\n"
        FTDI_SUPPORTED_DEVICES_LIST_ITEMS(AS_HELP_STRING)
#undef AS_HELP_STRING
        ,
    .activate = activate,
    .deactivate = deactivate,
    .max_vector_bits = max_vector_bits,
    .set_tck_period = set_tck_period,
    .shift_bits = shift_bits,
};

