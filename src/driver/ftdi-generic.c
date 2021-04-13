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

#include "driver.h"
#include "jtag_splitter.h"
#include "log.h"
#include "txvc_defs.h"

#include <bits/stdint-uintn.h>
#include <libftdi1/ftdi.h>

#include <unistd.h>

#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>

TXVC_DEFAULT_LOG_TAG(FTDI-GNR);

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

static bool load_config(const char **argNames, const char **argValues, struct ft_params *out) {
    memset(out, 0, sizeof(*out));
    out->channel = -1;

    for (;*argNames; argNames++, argValues++) {
#define CONVERT_AND_SET_IF_MATCHES(name, configField, converterFunc, validation, descr)            \
        if (strcmp(name, *argNames) == 0) {                                                        \
            out->configField = converterFunc(*argValues);                                          \
            continue;                                                                              \
        }
        PARAM_LIST_ITEMS(CONVERT_AND_SET_IF_MATCHES)
#undef CONVERT_AND_SET_IF_MATCHES
        WARN("Unknown parameter: \"%s=%s\"\n", *argNames, *argValues);
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
};

static struct driver gFtdi;

static bool tmsSenderFn(int numBits, const uint8_t* tms, void* extra) {
    struct driver* d = extra;
    TXVC_UNUSED(d);
    TXVC_UNUSED(numBits);
    TXVC_UNUSED(tms);
    WARN("%s: unimplemented stub\n", __func__);
    return false;
}

static bool tdiSenderFn(int numBits, const uint8_t* tdi, uint8_t* tdo, bool lastBitTmsHigh,
        void* extra) {
    struct driver* d = extra;
    TXVC_UNUSED(d);
    TXVC_UNUSED(numBits);
    TXVC_UNUSED(tdi);
    TXVC_UNUSED(tdo);
    TXVC_UNUSED(lastBitTmsHigh);
    WARN("%s: unimplemented stub\n", __func__);
    return false;
}

static bool do_transfer(struct ftdi_context* ctx,
        unsigned char* outgoing, int outgoingSz,
        unsigned char* incoming, int incomingSz) {
    bool shouldWrite = outgoingSz > 0;
    bool shouldRead = incomingSz > 0;
    struct ftdi_transfer_control* wrCtl =
        shouldWrite ? ftdi_write_data_submit(ctx, outgoing, outgoingSz) : NULL;
    struct ftdi_transfer_control* rdCtl =
        shouldRead ? ftdi_read_data_submit(ctx, incoming, incomingSz) : NULL;
    if (shouldWrite && ftdi_transfer_data_done(wrCtl) != outgoingSz) {
        ERROR("Failed to write %d bytes: %s\n", outgoingSz, ftdi_get_error_string(ctx));
        return false;
    }
    if (shouldRead && ftdi_transfer_data_done(rdCtl) != incomingSz) {
        ERROR("Failed to read %d bytes: %s\n", incomingSz, ftdi_get_error_string(ctx));
        return false;
    }
    return true;
}

static bool ensure_synced(struct ftdi_context* ctx) {
    unsigned char resp[2];
    return do_transfer(ctx, (unsigned char[]){ 0xab }, 1, resp, sizeof(resp))
        && resp[0] == 0xfa && resp[1] == 0xab;
}

static inline bool do_transfer_wronly(struct ftdi_context* ctx,
        unsigned char* outgoing, int outgoingSz) {
    return do_transfer(ctx, outgoing, outgoingSz, NULL, 0);
}



static bool activate(const char **argNames, const char **argValues){
    struct driver *d = &gFtdi;

    if (!load_config(argNames, argValues, &d->params)) return false;

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

    unsigned char setupCmds[] = {
        SET_BITS_LOW,
        0x08, /* Initial levels */
        0x0b, /* Directions */
        TCK_DIVISOR,
        0u, /* Div low */
        0u, /* Div high */
        LOOPBACK_START, /* TODO remove after testing */
    };
    if (!do_transfer_wronly(&d->ctx, setupCmds, sizeof(setupCmds)) || !ensure_synced(&d->ctx)) {
        ERROR("Failed to setup device\n");
        goto bail_reset_mode;
    }

    unsigned char cmd[] = {
        /* Push through */
        MPSSE_DO_READ | MPSSE_DO_WRITE | MPSSE_LSB | MPSSE_WRITE_NEG,
        0x03, 0x00,
        0x0d, 0xdd, 0xf0, 0x0d,
        /* TMS */
        MPSSE_WRITE_TMS | MPSSE_LSB | MPSSE_BITMODE,
        0x04,
        0x80,
        MPSSE_WRITE_TMS | MPSSE_LSB | MPSSE_BITMODE,
        0x04,
        0x7f,
        MPSSE_WRITE_TMS | MPSSE_LSB | MPSSE_BITMODE,
        0x06,
        0xaa,
        MPSSE_WRITE_TMS | MPSSE_LSB | MPSSE_BITMODE,
        0x00,
        0x00,
        MPSSE_WRITE_TMS | MPSSE_LSB | MPSSE_BITMODE,
        0x00,
        0x83,
    };
    unsigned char resp[1024] = { 0 };

    if (do_transfer(&d->ctx, cmd, sizeof(cmd), resp, 4)) {
        INFO("response:\n"
             "%02x, %02x, %02x, %02x\n"
             "%02x, %02x, %02x, %02x\n"
             "%02x, %02x, %02x, %02x\n"
             "%02x, %02x, %02x, %02x\n",
             resp[0], resp[1], resp[2], resp[3],
             resp[4], resp[5], resp[6], resp[7],
             resp[8], resp[9], resp[10], resp[11],
             resp[12], resp[13], resp[14], resp[15]);
    }

    ensure_synced(&d->ctx);

    if (!txvc_jtag_splitter_init(&d->jtagSplitter, tmsSenderFn, d, tdiSenderFn, d)) {
        goto bail_reset_mode;
    }

    goto bail_reset_mode;




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
    enum ftdi_chip_type type = d->ctx.type;
    switch (type) {
        case TYPE_2232H:
            return 4096 * 8;
        case TYPE_232H:
            return 1024 * 8;
        default:
            WARN("Unknown chip type: %d, using small buffer size\n", type);
            return 256 * 8;
    }
}

static int set_tck_period(int tckPeriodNs){
    /*
     * Find out needed divider by using official formula from FTDI docs:
     * TCK/SK period = 12MHz / (( 1 +[(0xValueH * 256) OR 0xValueL] ) * 2)
     * or, if high speed available:
     * TCK period = 60MHz / (( 1 +[ (0xValueH * 256) OR 0xValueL] ) * 2)
     *
     * Strictly speaking these formulae yield frequency, not a period. 
     */
    struct driver *d = &gFtdi;
    const bool highSpeed = true; /* TODO set accordingly to current chip type */
    const int maxFreqMHz = highSpeed ? 30 : 6;
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
    if (!do_transfer(&d->ctx, cmd, highSpeed ? 4 : 3, NULL, 0)) {
        ERROR("Can't set TCK period %dns\n", tckPeriodNs);
    }
    return actualPeriodNs;
}

static bool shift_bits(int numBits, const uint8_t *tmsVector, const uint8_t *tdiVector,
        uint8_t *tdoVector){
    struct driver *d = &gFtdi;
    return txvc_jtag_splitter_process(&d->jtagSplitter, numBits, tmsVector, tdiVector, tdoVector);
}

TXVC_DRIVER(ftdi_generic) = {
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

