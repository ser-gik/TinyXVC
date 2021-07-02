/*
 * Copyright 2020 Sergey Guralnik
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
#include "txvc/log.h"
#include "txvc/defs.h"

#include <libftdi1/ftdi.h>

#include <unistd.h>

#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>

TXVC_DEFAULT_LOG_TAG(FT2232H);

#define MAX_VECTOR_BITS_PER_ROUND 2048
static_assert((MAX_VECTOR_BITS_PER_ROUND % 8) == 0, "Max bits must be an integer number of bytes");
#define MAX_VECTOR_BYTES_PER_ROUND (MAX_VECTOR_BITS_PER_ROUND / 8)

#define PIN_ROLE_LIST_ITEMS(X)                                                                     \
    X("tck", PIN_ROLE_JTAG_TCK, "JTAG TCK signal (clock)")                                         \
    X("tdi", PIN_ROLE_JTAG_TDI, "JTAG TDI signal (test device input)")                             \
    X("tdo", PIN_ROLE_JTAG_TDO, "JTAG TDO signal (test device output)")                            \
    X("tms", PIN_ROLE_JTAG_TMS, "JTAG TMS signal (test mode select)")                              \
    X("driver_low", PIN_ROLE_OTHER_DRIVER_LOW, "permanent low level driver")                       \
    X("driver_high", PIN_ROLE_OTHER_DRIVER_HIGH, "permanent high level driver")                    \
    X("ignored", PIN_ROLE_OTHER_IGNORED, "ignored pin, configured as input")                       \

#define CLK_EDGE_LIST_ITEMS(X)                                                                     \
    X("falling", CLK_EDGE_FALLING, "falling/negative clock transition")                            \
    X("rising", CLK_EDGE_RISING, "rising/positive clock transition")                               \

#define PIN_LEVEL_LIST_ITEMS(X)                                                                    \
    X("low", PIN_LEVEL_LOW, "low/zero signal level")                                               \
    X("high", PIN_LEVEL_HIGH, "high/one signal level")                                             \

#define FTDI_INTERFACE_LIST_ITEMS(X)                                                               \
    X("A", INTERFACE_A, "FTDI' ADBUS channel")                                                     \
    X("B", INTERFACE_B, "FTDI' BDBUS channel")                                                     \

#define AS_ENUM_MEMBER(name, enumVal, descr) enumVal,

enum pin_role {
    PIN_ROLE_INVALID = 0,
    PIN_ROLE_LIST_ITEMS(AS_ENUM_MEMBER)
};

enum clk_edge {
    CLK_EDGE_INVALID = 0,
    CLK_EDGE_LIST_ITEMS(AS_ENUM_MEMBER)
};

enum pin_level {
    PIN_LEVEL_INVALID = 0,
    PIN_LEVEL_LIST_ITEMS(AS_ENUM_MEMBER)
};

#undef AS_ENUM_MEMBER

#define RETURN_ENUM_IF_NAME_MATCHES(name, enumVal, descr) if (strcmp(name, s) == 0) return enumVal;

static enum pin_role str_to_pin_role(const char *s) {
    PIN_ROLE_LIST_ITEMS(RETURN_ENUM_IF_NAME_MATCHES)
    return PIN_ROLE_INVALID;
}

static enum clk_edge str_to_clk_edge(const char *s) {
    CLK_EDGE_LIST_ITEMS(RETURN_ENUM_IF_NAME_MATCHES)
    return CLK_EDGE_INVALID;
}

static enum pin_level str_to_pin_level(const char *s) {
    PIN_LEVEL_LIST_ITEMS(RETURN_ENUM_IF_NAME_MATCHES)
    return PIN_LEVEL_INVALID;
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
    enum pin_level tck_idle_level;
    enum clk_edge tdi_tms_changing_edge;
    enum clk_edge tdo_sampling_edge;
    enum pin_role d_pins[8];
};

#define PARAM_LIST_ITEMS(X)                                                                        \
    X("vid", vid, str_to_usb_id, > 0, "USB device vendor ID")                                      \
    X("pid", pid, str_to_usb_id, > 0, "USB device product ID")                                     \
    X("channel", channel, str_to_ftdi_interface, >= 0, "FTDI channel to use")                      \
    X("tck_idle", tck_idle_level, str_to_pin_level, != PIN_LEVEL_INVALID,                          \
        "Level of the TCK signal between transactions")                                            \
    X("tdi_change_at", tdi_tms_changing_edge, str_to_clk_edge, != CLK_EDGE_INVALID,                \
        "TCK edge when TDI/TMS values are updated")                                                \
    X("tdo_sample_at", tdo_sampling_edge, str_to_clk_edge, != CLK_EDGE_INVALID,                    \
        "TCK edge when TDO value is sampled")                                                      \
    X("d0", d_pins[0], str_to_pin_role, != PIN_ROLE_INVALID, "D0 pin role")                        \
    X("d1", d_pins[1], str_to_pin_role, != PIN_ROLE_INVALID, "D1 pin role")                        \
    X("d2", d_pins[2], str_to_pin_role, != PIN_ROLE_INVALID, "D2 pin role")                        \
    X("d3", d_pins[3], str_to_pin_role, != PIN_ROLE_INVALID, "D3 pin role")                        \
    X("d4", d_pins[4], str_to_pin_role, != PIN_ROLE_INVALID, "D4 pin role")                        \
    X("d5", d_pins[5], str_to_pin_role, != PIN_ROLE_INVALID, "D5 pin role")                        \
    X("d6", d_pins[6], str_to_pin_role, != PIN_ROLE_INVALID, "D6 pin role")                        \
    X("d7", d_pins[7], str_to_pin_role, != PIN_ROLE_INVALID, "D7 pin role")                        \

static bool load_config(int numArg, const char **argNames, const char **argValues, struct ft_params *out) {
    memset(out, 0, sizeof(*out));
    out->channel = -1;

    for (int i = 0; i < numArg; i++) {
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

struct d_masks {
    uint8_t tck;
    uint8_t tdi;
    uint8_t tdo;
    uint8_t tms;
    uint8_t drivers_high;
    uint8_t drivers_low;
};

struct driver {
    struct ft_params params;
    struct d_masks masks;
    struct ftdi_version_info info;
    struct ftdi_context ctx;
};

static struct driver gFtdi;


inline static bool is_single_bit_set(uint8_t mask) {
    return !(mask & (mask - 1u));
}

static bool build_masks(const struct ft_params *params, struct d_masks *out) {
    memset(out, 0, sizeof(*out));
    for (size_t i = 0; i < sizeof(params->d_pins) / sizeof(params->d_pins[0]); i++) {
        switch (params->d_pins[i]) {
            case PIN_ROLE_JTAG_TCK: out->tck |= 1u << i; break;
            case PIN_ROLE_JTAG_TDI: out->tdi |= 1u << i; break;
            case PIN_ROLE_JTAG_TDO: out->tdo |= 1u << i; break;
            case PIN_ROLE_JTAG_TMS: out->tms |= 1u << i; break;
            case PIN_ROLE_OTHER_DRIVER_HIGH: out->drivers_high |= 1u << i; break;
            case PIN_ROLE_OTHER_DRIVER_LOW: out->drivers_low |= 1u << i; break;
            default: break;
        }
    }

#define VALIDATE_SINGLE_BIT_MASK(mask)                                                             \
    do {                                                                                           \
        if (!is_single_bit_set(out->mask)) {                                                       \
            ERROR("Missing or multiple \"%s\" is not allowed\n", #mask);                           \
            return false;                                                                          \
        }                                                                                          \
    } while (0)
    VALIDATE_SINGLE_BIT_MASK(tck);
    VALIDATE_SINGLE_BIT_MASK(tdi);
    VALIDATE_SINGLE_BIT_MASK(tdo);
    VALIDATE_SINGLE_BIT_MASK(tms);
#undef VALIDATE_SINGLE_BIT_MASK
    return true;
}

inline static uint8_t build_sample(const struct d_masks *masks, bool tck, bool tdi, bool tms) {
    uint8_t res = 0u;
    res |= tck ? masks->tck : 0u;
    res |= tdi ? masks->tdi : 0u;
    res |= tms ? masks->tms : 0u;
    res |= masks->drivers_high;
    return res;
}

inline static bool extract_tdo(const struct d_masks *masks, uint8_t sample) {
    return sample & masks->tdo;
}

static bool do_shift_bits(struct driver *d, int numBits, const uint8_t *tmsVector, const uint8_t *tdiVector,
        uint8_t *tdoVector){
    if (numBits > MAX_VECTOR_BITS_PER_ROUND) {
        ERROR("Too many bits to transfer: %d (max. supported: %d)\n",
                numBits, MAX_VECTOR_BITS_PER_ROUND);
        return false;
    }

    uint8_t sendBuf[MAX_VECTOR_BITS_PER_ROUND * 2];
    uint8_t recvBuf[MAX_VECTOR_BITS_PER_ROUND * 2];
    const struct ft_params *p = &d->params;

    const bool tck0 = !(p->tck_idle_level == PIN_LEVEL_HIGH);
    const bool updateTdiTms0 = (p->tdi_tms_changing_edge == CLK_EDGE_FALLING && !tck0)
                            || (p->tdi_tms_changing_edge == CLK_EDGE_RISING && tck0);
    bool tmsPrev = 0u;
    bool tdiPrev = 0u;
    for (int i = 0; i < numBits; i++) {
        int byteIdx = i / 8;
        int bitIdx = i % 8;
        bool tms = !!(tmsVector[byteIdx] & (1u << bitIdx));
        bool tdi = !!(tdiVector[byteIdx] & (1u << bitIdx));
        sendBuf[i * 2 + 0] = build_sample(&d->masks, tck0,
                updateTdiTms0 ? tdi : tdiPrev,
                updateTdiTms0 ? tms : tmsPrev);
        sendBuf[i * 2 + 1] = build_sample(&d->masks, !tck0,
                tdi,
                tms);
        tmsPrev = tms;
        tdiPrev = tdi;
    }

    int transferSz = numBits * 2;
    int res = ftdi_write_data(&d->ctx, sendBuf, transferSz);
    if (res != transferSz) {
        ERROR("Failed to write %d bytes: %s\n", transferSz, ftdi_get_error_string(&d->ctx));
        return false;
    }
    res = ftdi_read_data(&d->ctx, recvBuf, transferSz);
    if (res != transferSz) {
        ERROR("Failed to read %d bytes: %s\n", transferSz, ftdi_get_error_string(&d->ctx));
        return false;
    }

    int tdoSampleOffset =
                (p->tck_idle_level == PIN_LEVEL_HIGH && p->tdo_sampling_edge == CLK_EDGE_FALLING)
             || (p->tck_idle_level == PIN_LEVEL_LOW && p->tdo_sampling_edge == CLK_EDGE_RISING)
                    ? 0 : 1;
    for (int i = 0; i < numBits; i++) {
        int byteIdx = i / 8;
        int bitIdx = i % 8;
        if (extract_tdo(&d->masks, recvBuf[i * 2 + tdoSampleOffset])) {
            tdoVector[byteIdx] |= 1u << bitIdx;
        } else {
            tdoVector[byteIdx] &= ~(1u << bitIdx);
        }
    }
    return true;
}

static bool activate(int numArg, const char **argNames, const char **argValues){
    struct driver *d = &gFtdi;

    if (!load_config(numArg, argNames, argValues, &d->params)) return false;
    if (!build_masks(&d->params, &d->masks)) return false;

    d->info = ftdi_get_library_version();
    INFO("Using libftdi \"%s %s\"\n", d->info.version_str, d->info.snapshot_str);

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
    REQUIRE_FTDI_SUCCESS_(ftdi_set_latency_timer(&d->ctx, 1), bail_usb_close);
    REQUIRE_FTDI_SUCCESS_(ftdi_setflowctrl(&d->ctx, SIO_DISABLE_FLOW_CTRL), bail_usb_close);
    REQUIRE_FTDI_SUCCESS_(ftdi_set_baudrate(&d->ctx, 1000 * 1000 / 16), bail_usb_close);
    REQUIRE_FTDI_SUCCESS_(ftdi_set_bitmode(&d->ctx, 0x00, BITMODE_RESET), bail_usb_close);
    /*
     * Write idle pattern in all-inputs mode to get correct
     * pin levels once actual direction mask is  applied
     */
    REQUIRE_FTDI_SUCCESS_(ftdi_set_bitmode(&d->ctx, 0x00, BITMODE_SYNCBB), bail_reset_mode);
    const uint8_t idlePattern = build_sample(&d->masks,
            d->params.tck_idle_level == PIN_LEVEL_HIGH, false, false);
    uint8_t dummy;
    if (ftdi_write_data(&d->ctx, &idlePattern, 1) != 1
            || ftdi_read_data(&d->ctx, &dummy, 1) != 1) {
        ERROR("Can't apply idle pattern to channel pins: %s\n", ftdi_get_error_string(&d->ctx));
        goto bail_reset_mode;
    }
    const uint8_t directionMask = d->masks.tck | d->masks.tdi | d->masks.tms
                             | d->masks.drivers_high | d->masks.drivers_low;
    REQUIRE_FTDI_SUCCESS_(
        ftdi_set_bitmode(&d->ctx, directionMask, BITMODE_SYNCBB), bail_reset_mode);

#undef REQUIRE_FTDI_SUCCESS_
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
    ftdi_set_bitmode(&d->ctx, 0x00, BITMODE_RESET);
    ftdi_usb_close(&d->ctx);
    ftdi_deinit(&d->ctx);
    return true;
}

static int max_vector_bits(void){
    return MAX_VECTOR_BITS_PER_ROUND;
}

static int set_tck_period(int tckPeriodNs){
    struct driver *d = &gFtdi;
    int baudrate = 2 * (1000 * 1000 * 1000 / tckPeriodNs) / 16;
    int err = ftdi_set_baudrate(&d->ctx, baudrate);
    if (err) {
        ERROR("Can't set TCK period %dns: %d %s\n", tckPeriodNs, err, ftdi_get_error_string(&d->ctx));
    }
    return err ? 0 : tckPeriodNs;
}

static bool shift_bits(int numBits, const uint8_t *tmsVector, const uint8_t *tdiVector,
        uint8_t *tdoVector){
    struct driver *d = &gFtdi;
    for (; numBits > MAX_VECTOR_BITS_PER_ROUND;
                numBits -= MAX_VECTOR_BITS_PER_ROUND,
                tmsVector += MAX_VECTOR_BYTES_PER_ROUND,
                tdiVector += MAX_VECTOR_BYTES_PER_ROUND,
                tdoVector += MAX_VECTOR_BYTES_PER_ROUND) {
        if (!do_shift_bits(d, MAX_VECTOR_BITS_PER_ROUND, tmsVector, tdiVector, tdoVector)) {
            return false;
        }
    }
    return do_shift_bits(d, numBits, tmsVector, tdiVector, tdoVector);
}

const struct txvc_driver driver_ft2232h = {
    .name = "ft2232h",
    .help =
        " !!! DEPRECATED !!!\n"
        "use \"ftdi-generic\" instead\n\n"
        "Sends vectors to the device behind FT2232H chip, which is connected to this machine USB\n"
        "Parameters:\n"
#define AS_HELP_STRING(name, configField, converterFunc, validation, descr) \
        "  \"" name "\" - " descr "\n"
        PARAM_LIST_ITEMS(AS_HELP_STRING)
#undef AS_HELP_STRING
#define AS_HELP_STRING(name, enumVal, descr) \
        "  \"" name "\" - " descr "\n"
        "Allowed pin roles:\n"
        PIN_ROLE_LIST_ITEMS(AS_HELP_STRING)
        "Allowed clock edges:\n"
        CLK_EDGE_LIST_ITEMS(AS_HELP_STRING)
        "Allowed pin levels:\n"
        PIN_LEVEL_LIST_ITEMS(AS_HELP_STRING)
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

