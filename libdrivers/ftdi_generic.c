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

#include <libftdi1/ftdi.h>

#include <unistd.h>

#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>

TXVC_DEFAULT_LOG_TAG(ftdi-generic);

#define PIN_ROLE_LIST_ITEMS(X)                                                                     \
    X("driver_low", PIN_ROLE_OTHER_DRIVER_LOW, "permanent low level driver")                       \
    X("driver_high", PIN_ROLE_OTHER_DRIVER_HIGH, "permanent high level driver")                    \
    X("ignored", PIN_ROLE_OTHER_IGNORED, "ignored pin, configured as input")                       \

#define FTDI_INTERFACE_LIST_ITEMS(X)                                                               \
    X("A", INTERFACE_A, "FTDI' ADBUS channel")                                                     \
    X("B", INTERFACE_B, "FTDI' BDBUS channel")                                                     \

#define AS_ENUM_MEMBER(name, enumVal, descr) enumVal,

enum pin_role {
    PIN_ROLE_INVALID = 0,
    PIN_ROLE_LIST_ITEMS(AS_ENUM_MEMBER)
};

#undef AS_ENUM_MEMBER

#define RETURN_ENUM_IF_NAME_MATCHES(name, enumVal, descr) if (strcmp(name, s) == 0) return enumVal;

static enum pin_role str_to_pin_role(const char *s) {
    PIN_ROLE_LIST_ITEMS(RETURN_ENUM_IF_NAME_MATCHES)
    return PIN_ROLE_INVALID;
}

static enum ftdi_interface str_to_ftdi_interface(const char *s) {
    FTDI_INTERFACE_LIST_ITEMS(RETURN_ENUM_IF_NAME_MATCHES)
    return -1;
}

#undef RETURN_ENUM_IF_NAME_MATCHES

static int str_to_usb_id(const char *s) {
    char *endp;
    long res = strtol(s, &endp, 16);
    return *endp != '\0' ||  res <= 0l || res > 0xffffl ? -1 : (int) res;
}

struct ft_params {
    int vid;
    int pid;
    enum ftdi_interface channel;
    enum pin_role d_pins[8];
};

#define PARAM_LIST_ITEMS(X)                                                                        \
    X("vid", vid, str_to_usb_id, > 0, "USB device vendor ID")                                      \
    X("pid", pid, str_to_usb_id, > 0, "USB device product ID")                                     \
    X("channel", channel, str_to_ftdi_interface, >= 0, "FTDI channel to use")                      \
    X("d4", d_pins[4], str_to_pin_role, != PIN_ROLE_INVALID, "D4 pin role")                        \
    X("d5", d_pins[5], str_to_pin_role, != PIN_ROLE_INVALID, "D5 pin role")                        \
    X("d6", d_pins[6], str_to_pin_role, != PIN_ROLE_INVALID, "D6 pin role")                        \
    X("d7", d_pins[7], str_to_pin_role, != PIN_ROLE_INVALID, "D7 pin role")                        \

static bool load_config(int numArgs, const char **argNames, const char **argValues, struct ft_params *out) {
    memset(out, 0, sizeof(*out));
    out->channel = -1;

    for (int i = 0; i < numArgs; i++) {
#define CONVERT_AND_SET_IF_MATCHES(name, configField, converterFunc, validation, descr)            \
        if (strcmp(name, argNames[i]) == 0) {                                                        \
            out->configField = converterFunc(argValues[i]);                                          \
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

struct driver {
    struct ft_params params;
    struct ftdi_context ctx;
    struct txvc_jtag_splitter jtagSplitter;
    int chipBufferSz;
    unsigned lastTdi : 1;
};

static struct driver gFtdi;


struct readonly_granule {
    const uint8_t* data;
    int size;
};

struct granule {
    uint8_t* data;
    int size;
};

static bool do_transfer_granular(struct ftdi_context* ctx,
        const struct readonly_granule* outgoingGranules, int numOutgoingGranules,
        const struct granule* incomingGranules, int numIncomingGranules) {
    for (int i = 0; i < numOutgoingGranules; i++) {
        const struct readonly_granule* g = outgoingGranules + i;
        if (g->size > 0) {
            struct ftdi_transfer_control* ctl = ftdi_write_data_submit(ctx,
                    (uint8_t*)g->data, g->size);
            if (!ctl) {
                ERROR("Failed to send data: %s\n", ftdi_get_error_string(ctx));
                return false;
            }
            int sz = ftdi_transfer_data_done(ctl);
            if (sz != g->size) {
                ERROR("Failed to send data: %s (res: %d)\n", ftdi_get_error_string(ctx), sz);
                return false;
            }
        }
    }
    for (int i = 0; i < numIncomingGranules; i++) {
        const struct granule* g = incomingGranules + i;
        if (g->size > 0) {
            struct ftdi_transfer_control* ctl = ftdi_read_data_submit(ctx,
                    g->data, g->size);
            if (!ctl) {
                ERROR("Failed to receive data: %s\n", ftdi_get_error_string(ctx));
                return false;
            }
            int sz = ftdi_transfer_data_done(ctl);
            if (sz != g->size) {
                ERROR("Failed to receive data: %s (res: %d)\n", ftdi_get_error_string(ctx), sz);
                return false;
            }
        }
    }
    return true;
}

static bool do_transfer(struct ftdi_context* ctx,
        const uint8_t* outgoing, int outgoingSz,
        uint8_t* incoming, int incomingSz) {
    const struct readonly_granule out = { .data = outgoing, .size = outgoingSz, };
    const struct granule in = { .data = incoming, .size = incomingSz, };
    return do_transfer_granular(ctx, &out, 1, &in, 1);
}

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

static bool ensure_synced(struct ftdi_context* ctx) {
    /* Send bad opcode and check that chip responds with "BadCommand" */
    unsigned char resp[2];
    return do_transfer(ctx, (unsigned char[]){ 0xab }, 1, resp, sizeof(resp))
        && resp[0] == 0xfa && resp[1] == 0xab;
}

static bool tmsSenderFn(const uint8_t* tms, int fromBitIdx, int toBitIdx, void* extra) {
    ALWAYS_ASSERT(fromBitIdx >= 0);
    ALWAYS_ASSERT(toBitIdx >= 0);
    ALWAYS_ASSERT(toBitIdx > fromBitIdx);

    struct driver* d = extra;
    while (fromBitIdx < toBitIdx) {
        const int bitsToTransfer = min(toBitIdx - fromBitIdx, 6);
        uint8_t pattern = (!!d->lastTdi) << 7;
        copy_bits(tms, fromBitIdx, &pattern, 0, bitsToTransfer, true);
        const uint8_t cmd[] = {
            MPSSE_WRITE_TMS | MPSSE_LSB | MPSSE_BITMODE,
            bitsToTransfer - 1,
            pattern,
        };
        if (!do_transfer(&d->ctx, cmd, sizeof(cmd), NULL, 0) || !ensure_synced(&d->ctx)) {
            return false;
        }
        fromBitIdx += bitsToTransfer;
    }
    return true;
}

static bool tdiSenderFn(const uint8_t* tdi, uint8_t* tdo, int fromBitIdx, int toBitIdx,
        bool lastTmsBitHigh, void* extra) {
    ALWAYS_ASSERT(fromBitIdx >= 0);
    ALWAYS_ASSERT(toBitIdx >= 0);
    ALWAYS_ASSERT(toBitIdx > fromBitIdx);

    struct driver* d = extra;
    /* Last is special because it is always sent separately via TMS command, ignore it for now. */
    const int lastBitIdx = toBitIdx - 1;
    /* Determine right boundaries of the first and last stream octets */
    const int firstOctetEnd = fromBitIdx + (8 - fromBitIdx % 8);
    const int lastOctetEnd = lastBitIdx + (lastBitIdx % 8 ? (8 - lastBitIdx % 8) : 0);
    const int lastOctetStart = lastOctetEnd - 8;
    const bool allInOneOctet = firstOctetEnd == lastOctetEnd;
    const int numLeadingBits = allInOneOctet ? lastBitIdx - fromBitIdx : firstOctetEnd - fromBitIdx;
    const int numTrailingBits = allInOneOctet ? 0 : lastBitIdx - lastOctetStart;

    /*
     * Find out maximal amount of whole bytes for a one round such that there is always
     * a bit of extra space for leading, trailing and last bit commands.
     * This just simplifies a loop below.
     */
    const int maxIntermOctetsPerTransfer = d->chipBufferSz / 2
        - 3 /* Command for sending heading bits */
        - 3 /* Command for sending middle bytes w/o payload */
        - 3 /* Command for sending trailing bits except for the last one */
        - 3 /* Command for sending the last trailing bit along with TMS bit */
        ;

    for (int curIdx = fromBitIdx; curIdx < toBitIdx;) {
        uint8_t leadingBitsCmd[3], intermBitsCmdHeader[3], trailingBitsCmd[3], lastBitCmd[3]; 
        uint8_t leadingTdoBits, trailingTdoBits, lastTdoBit;
        bool withLeading = false, withTrailing = false, withLast = false;
        struct readonly_granule out[5];
        struct granule in[4];
        int numOutGranules = 0;
        int numInGranules = 0;

        if (curIdx == fromBitIdx) {
            withLeading = true;
            leadingBitsCmd[0] = MPSSE_DO_READ | MPSSE_DO_WRITE | MPSSE_LSB
                                        | MPSSE_BITMODE | MPSSE_WRITE_NEG;
            leadingBitsCmd[1] = numLeadingBits - 1;
            leadingBitsCmd[2] = tdi[fromBitIdx / 8] >> (fromBitIdx % 8);
            out[numOutGranules++] = (struct readonly_granule){
                .data = leadingBitsCmd,
                .size = 3,
            };
            in[numInGranules++] = (struct granule){
                .data = &leadingTdoBits, 
                .size = 1,
            };
            curIdx += numLeadingBits;
        }
        if (curIdx < lastBitIdx) {
            if (curIdx < lastOctetStart) {
                ALWAYS_ASSERT(curIdx % 8 == 0);
                const int intermOctetsToSend =
                    min((lastOctetStart - curIdx) / 8, maxIntermOctetsPerTransfer);
                intermBitsCmdHeader[0] = MPSSE_DO_READ | MPSSE_DO_WRITE | MPSSE_LSB
                                                | MPSSE_WRITE_NEG;
                intermBitsCmdHeader[1] = ((intermOctetsToSend - 1) >> 0) & 0xff;
                intermBitsCmdHeader[2] = ((intermOctetsToSend - 1) >> 8) & 0xff;
                out[numOutGranules++] = (struct readonly_granule){
                    .data = intermBitsCmdHeader,
                    .size = 3,
                };
                out[numOutGranules++] = (struct readonly_granule){
                    .data = tdi + curIdx / 8,
                    .size = intermOctetsToSend,
                };
                in[numInGranules++] = (struct granule){
                    .data = tdo + curIdx / 8,
                    .size = intermOctetsToSend,
                };
                curIdx += intermOctetsToSend * 8;
            }
            if (curIdx == lastOctetStart) {
                withTrailing = true;
                trailingBitsCmd[0] = MPSSE_DO_READ | MPSSE_DO_WRITE | MPSSE_LSB
                                            | MPSSE_BITMODE | MPSSE_WRITE_NEG;
                trailingBitsCmd[1] = numTrailingBits - 1;
                trailingBitsCmd[2] = tdi[lastOctetStart / 8];
                out[numOutGranules++] = (struct readonly_granule){
                    .data = trailingBitsCmd,
                    .size = 3,
                };
                in[numInGranules++] = (struct granule){
                    .data = &trailingTdoBits,
                    .size = 1,
                };
                curIdx += numTrailingBits;
            }
        }
        if (!do_transfer_granular(&d->ctx, out, numOutGranules, in, numInGranules)) {
            return false;
        }

        if (curIdx == lastBitIdx) {
            withLast = true;
            numOutGranules = 0;
            numInGranules = 0;
            const int lastTdiBit = !!get_bit(tdi, lastBitIdx);
            const int lastTmsBit = !!lastTmsBitHigh;
            lastBitCmd[0] = MPSSE_WRITE_TMS | MPSSE_DO_READ | MPSSE_LSB | MPSSE_BITMODE;
            lastBitCmd[1] = 0x00;
            lastBitCmd[2] = (lastTdiBit << 7) | (lastTmsBit) << 1 | lastTmsBit;
            out[numOutGranules++] = (struct readonly_granule){
                .data = lastBitCmd,
                .size = 3,
            };
            in[numInGranules++] = (struct granule){
                .data = &lastTdoBit,
                .size = 1,
            };
            curIdx += 1;

            /* TODO having TMS command in the same batch somehow blocks writes */
            if (!do_transfer_granular(&d->ctx, out, numOutGranules, in, numInGranules)) {
                return false;
            }
        }

        if (withLeading) {
            copy_bits(&leadingTdoBits, 8 - numLeadingBits,
                    tdo, fromBitIdx, numLeadingBits, false);
        }
        if (withTrailing) {
            copy_bits(&trailingTdoBits, 8 - numTrailingBits,
                    tdo, lastOctetStart, numTrailingBits, false);
        }
        if (withLast) {
            const int lastTdi = !!get_bit(tdi, lastBitIdx);
            d->lastTdi = lastTdi;
            const bool lastTdo = get_bit(&lastTdoBit, 7); /* TDO is shifted in from the right. */
            set_bit(tdo, lastBitIdx, lastTdo);
        }
    }
    return true;
}

TXVC_USED
static bool loopback_test(struct driver* d) {
    INFO("%s: start\n", __func__);
    const uint8_t loopback_start = LOOPBACK_START;
    if (!do_transfer(&d->ctx, &loopback_start, 1, NULL, 0)) return false;

    bool res = true;
    const int maxVectorBits = 64;

    for (int round = 0; round < 100; round++) {
        uint8_t tdi[10];
        uint8_t tdo[10];
        txvc_bit_vector_random(tdi, sizeof(tdi));

        for (int start = 0; start < maxVectorBits; start++) {
            for (int end = start + 1; end < maxVectorBits; end++) {
                memcpy(tdo, tdi, sizeof(tdo));
                if (!tdiSenderFn(tdi, tdo, start, end, true, d)) {
                    ERROR("%s: failed to transfer\n", __func__);
                    return false;
                }
                if (memcmp(tdi, tdo, sizeof(tdo)) != 0) {
                    res = false;
                    char tdiStr[4096];
                    txvc_bit_vector_format_lsb(tdiStr, sizeof(tdiStr), tdi, start, end);
                    char tdoStr[4096];
                    txvc_bit_vector_format_lsb(tdoStr, sizeof(tdoStr), tdo, start, end);
                    WARN("%s: mismatch: (start: %d, end: %d)\n", __func__, start, end);
                    WARN("TDI: %s\n", tdiStr);
                    WARN("TDO: %s\n", tdoStr);
                }
            }
        }
    }

    if (0) {
        uint8_t tdi[1024];
        uint8_t tdo[1024];
        txvc_bit_vector_random(tdi, sizeof(tdi));

        for (int end = 1; end < 1024 * 8; end++) {
            memcpy(tdo, tdi, sizeof(tdo));
            INFO("Sending %d bits\n", end);
            if (!tdiSenderFn(tdi, tdo, 0, end, true, d)) {
                ERROR("%s: failed to transfer\n", __func__);
                return false;
            }
            if (memcmp(tdi, tdo, sizeof(tdo)) != 0) {
                res = false;
                char tdiStr[4096];
                txvc_bit_vector_format_lsb(tdiStr, sizeof(tdiStr), tdi, 0, end);
                char tdoStr[4096];
                txvc_bit_vector_format_lsb(tdoStr, sizeof(tdoStr), tdo, 0, end);
                WARN("%s: mismatch: (start: %d, end: %d)\n", __func__, 0, end);
                WARN("TDI: %s\n", tdiStr);
                WARN("TDO: %s\n", tdoStr);
            }
        }
    }




    const uint8_t loopback_end = LOOPBACK_END;
    if (!do_transfer(&d->ctx, &loopback_end, 1, NULL, 0)) return false;
    INFO("%s: done\n", __func__);
    return res;
}


static bool activate(int numArgs, const char **argNames, const char **argValues){
    struct driver *d = &gFtdi;

    if (!load_config(numArgs, argNames, argValues, &d->params)) return false;

    struct ftdi_version_info info = ftdi_get_library_version();
    INFO("Using libftdi \"%s %s\"\n", info.version_str, info.snapshot_str);

#define REQUIRE_FTDI_SUCCESS_(ftdiCallExpr, cleanupLabel)                                          \
    do {                                                                                           \
        int err = (ftdiCallExpr);                                                                  \
        if (err != 0) {                                                                            \
            ERROR("Failed: %s: %d %s\n", #ftdiCallExpr, err, ftdi_get_error_string(&gFtdi.ctx));   \
            goto cleanupLabel;                                                                     \
        }                                                                                          \
    } while (0)

    REQUIRE_FTDI_SUCCESS_(ftdi_init(&d->ctx), bail_noop);
    REQUIRE_FTDI_SUCCESS_(ftdi_set_interface(&d->ctx, d->params.channel), bail_deinit);
    REQUIRE_FTDI_SUCCESS_(
        ftdi_usb_open(&d->ctx, d->params.vid, d->params.pid), bail_deinit);
    REQUIRE_FTDI_SUCCESS_(ftdi_usb_purge_buffers(&d->ctx) , bail_usb_close);
    REQUIRE_FTDI_SUCCESS_(ftdi_set_event_char(&d->ctx, 0, 0) , bail_usb_close);
    REQUIRE_FTDI_SUCCESS_(ftdi_set_error_char(&d->ctx, 0, 0) , bail_usb_close);
    REQUIRE_FTDI_SUCCESS_(ftdi_set_latency_timer(&d->ctx, 1), bail_usb_close);
    REQUIRE_FTDI_SUCCESS_(ftdi_setflowctrl(&d->ctx, SIO_RTS_CTS_HS), bail_usb_close);
    REQUIRE_FTDI_SUCCESS_(ftdi_set_bitmode(&d->ctx, 0x00, BITMODE_RESET), bail_usb_close);
    REQUIRE_FTDI_SUCCESS_(ftdi_set_bitmode(&d->ctx, 0x00, BITMODE_MPSSE), bail_usb_close);

#undef REQUIRE_FTDI_SUCCESS_

    switch (d->ctx.type) {
        case TYPE_2232H:
            d->chipBufferSz = 4096;
            break;
        case TYPE_232H:
            d->chipBufferSz = 1024;
            break;
        default:
            ERROR("Unknown chip type: %d\n", d->ctx.type);
            goto bail_reset_mode;
    }

    uint8_t setupCmds[] = {
        SET_BITS_LOW,
        0x08, /* Initial levels: TCK=0, TDI=0, TMS=1 */
        0x0b, /* Directions: TCK=out, TDI=out, TDO=in, TMS=out */
        TCK_DIVISOR,
        0x05, /* Initialize to 1 MHz clock */
        0x00,
    };
    if (!do_transfer(&d->ctx, setupCmds, sizeof(setupCmds), NULL, 0) || !ensure_synced(&d->ctx)) {
        ERROR("Failed to setup device\n");
        goto bail_reset_mode;
    }
    d->lastTdi = 0;


    if (0 && !loopback_test(d)) {
        goto bail_reset_mode;
    }



    if (!txvc_jtag_splitter_init(&d->jtagSplitter, tmsSenderFn, d, tdiSenderFn, d)) {
        goto bail_reset_mode;
    }

    return true;

bail_reset_mode:
    ftdi_set_bitmode(&d->ctx, 0x00, BITMODE_RESET);
bail_usb_close:
    ftdi_usb_close(&d->ctx);
bail_deinit:
    ftdi_deinit(&d->ctx);
bail_noop:
    return false;
}

static bool deactivate(void){
    struct driver *d = &gFtdi;
    txvc_jtag_splitter_deinit(&d->jtagSplitter);
    ftdi_set_bitmode(&d->ctx, 0x00, BITMODE_RESET);
    ftdi_usb_close(&d->ctx);
    ftdi_deinit(&d->ctx);
    return true;
}

static int max_vector_bits(void){
    struct driver *d = &gFtdi;
    return d->chipBufferSz * 8;
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
    const bool highSpeed = true; /* TODO set accordingly to current chip type */
    const int maxFreqMHz = highSpeed ? 30 : 6;
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
        WARN("Using minimal available period - %dns\n", actualPeriodNs);
    }
    if (divider == 0xffff) {
        WARN("Using maximal available period - %dns\n", actualPeriodNs);
    }
    uint8_t cmd[] = { TCK_DIVISOR, divider & 0xff, (divider >> 8) & 0xff, DIS_DIV_5, };
    if (!do_transfer(&d->ctx, cmd, highSpeed ? 4 : 3, NULL, 0) || !ensure_synced(&d->ctx)) {
        ERROR("Can't set TCK period %dns\n", tckPeriodNs);
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
#undef AS_HELP_STRING
        ,
    .activate = activate,
    .deactivate = deactivate,
    .max_vector_bits = max_vector_bits,
    .set_tck_period = set_tck_period,
    .shift_bits = shift_bits,
};

