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

#define FTDI_CHANNEL_LIST_ITEMS(X)                                                                 \
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
    FTDI_CHANNEL_LIST_ITEMS(RETURN_ENUM_IF_NAME_MATCHES)
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
typedef void (*rx_callback_fn)(const uint8_t *rxData, void *cbExtra);

struct rx_callback_node {
    struct rx_callback_node *next;
    rx_callback_fn fn;
    const uint8_t *data;
    void *extra;
};

struct ft_transaction {
    FT_HANDLE ftChip;
    struct txvc_mempool pool;
    uint8_t *txBuffer;
    int txNumBytes;
    int maxTxBufferBytes;
    uint8_t *rxBuffer;
    int rxNumBytes;
    int maxRxBufferBytes;
    struct rx_callback_node *rxCallbackFirst;
    struct rx_callback_node *rxCallbackLast;
};

struct driver {
    struct ft_params params;
    int chipBufferBytes;
    bool highSpeedCapable;
    FT_HANDLE ftHandle;
    struct txvc_jtag_splitter jtagSplitter;
    unsigned lastTdi : 1;
    struct txvc_mempool pool;
    struct ft_transaction transaction;
};

static struct driver gFtdi;


/* FTDI MPSSE opcodes */
static const uint8_t OP_BAD_COMMANDS = 0xfau;
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

static const char *ft_status_name(FT_STATUS s) {
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

static void ft_transaction_init(struct ft_transaction *t, FT_HANDLE ftChip, int maxBufferBytes) {
    t->ftChip = ftChip;
    txvc_mempool_init(&t->pool, 128 * 1024);
    t->maxTxBufferBytes = 2 * maxBufferBytes; // TODO explais
    t->maxRxBufferBytes = maxBufferBytes;
    t->txBuffer = t->rxBuffer = NULL;
    t->txNumBytes = t->rxNumBytes = 0;
    t->rxCallbackFirst = t->rxCallbackLast = NULL;
}

static void ft_transaction_deinit(struct ft_transaction *t) {
    txvc_mempool_deinit(&t->pool);
}

static bool ft_transaction_flush(struct ft_transaction *t) {
    if (t->txBuffer) {
        DWORD written;
        FT_STATUS status = FT_Write(t->ftChip, (LPVOID *) t->txBuffer, t->txNumBytes, &written);
        if (!FT_SUCCESS(status)) {
            ERROR("Failed to send data: %s\n", ft_status_name(status));
            return false;
        }
        if (written != (DWORD) t->txNumBytes) {
            ERROR("Sent only %u bytes of %d\n", written, t->txNumBytes);
            return false;
        }
        t->txBuffer = NULL;
        t->txNumBytes = 0;
    }
    if (t->rxBuffer) {
        DWORD read;
        FT_STATUS status = FT_Read(t->ftChip, t->rxBuffer, t->rxNumBytes, &read);
        if (!FT_SUCCESS(status)) {
            ERROR("Failed to receive data: %s\n", ft_status_name(status));
            return false;
        }
        if (read != (DWORD) t->rxNumBytes) {
            ERROR("Received only %u bytes of %d\n", read, t->rxNumBytes);
            return false;
        }
        for (const struct rx_callback_node *cb = t->rxCallbackFirst; cb; cb = cb->next) {
            cb->fn(cb->data, cb->extra);
        }
        t->rxCallbackFirst = t->rxCallbackLast = NULL;
        t->rxBuffer = NULL;
        t->rxNumBytes = 0;
    }
    txvc_mempool_reclaim_all(&t->pool);
    return true;
}

static bool ft_transaction_add_write_to_chip_with_readback(struct ft_transaction *t,
        const uint8_t *txData, int txNumBytes, rx_callback_fn cb, void *cbExtra, int rxNumBytes) {
    ALWAYS_ASSERT(txNumBytes <= t->maxRxBufferBytes && rxNumBytes <= t->maxRxBufferBytes);
    const bool shouldFlush = t->txNumBytes + txNumBytes > t->maxTxBufferBytes
                          || t->rxNumBytes + rxNumBytes > t->maxRxBufferBytes;
    if (shouldFlush && !ft_transaction_flush(t)) {
        return false;
    }
    if (txNumBytes > 0) {
        if (!t->txBuffer) {
            t->txBuffer = txvc_mempool_alloc_unaligned(&t->pool, t->maxTxBufferBytes);
        }
        memcpy(t->txBuffer + t->txNumBytes, txData, txNumBytes);
        t->txNumBytes += txNumBytes;
    }
    if (rxNumBytes > 0) {
        if (!t->rxBuffer) {
            t->rxBuffer = txvc_mempool_alloc_unaligned(&t->pool, t->maxRxBufferBytes);
        }
        if (cb) {
            struct rx_callback_node *rxCallback =
                txvc_mempool_alloc_object(&t->pool, struct rx_callback_node); 
            rxCallback->next = NULL;
            rxCallback->fn = cb;
            rxCallback->data = t->rxBuffer + t->rxNumBytes;
            rxCallback->extra = cbExtra;
            if (!t->rxCallbackFirst) {
                t->rxCallbackFirst = t->rxCallbackLast = rxCallback;
            } else {
                t->rxCallbackLast->next = rxCallback;
                t->rxCallbackLast = rxCallback;
            }
        }
        t->rxNumBytes += rxNumBytes;
    }
    return true;
}

struct bit_copier_rx_callback_extra {
    int fromBit;
    uint8_t* dst;
    int toBit;
    int numBits;
};

static void bit_copier_rx_callback_fn(const uint8_t *rxData, void *cbExtra) {
    const struct bit_copier_rx_callback_extra *e = cbExtra;
    copy_bits(rxData, e->fromBit, e->dst, e->toBit, e->numBits, false);
}

struct byte_copier_rx_callback_extra {
    uint8_t* dst;
    int numBytes;
};

static void byte_copier_rx_callback_fn(const uint8_t *rxData, void *cbExtra) {
    const struct byte_copier_rx_callback_extra *e = cbExtra;
    memcpy(e->dst, rxData, e->numBytes);
}

static inline bool ft_transaction_add_write_to_chip(struct ft_transaction *t,
        const uint8_t *txData, int txNumBytes) {
    return ft_transaction_add_write_to_chip_with_readback(t, txData, txNumBytes, 0, NULL, 0);
}

static bool ft_transaction_add_write_to_chip_with_readback_simple(struct ft_transaction *t,
        const uint8_t *txData, int txNumBytes, uint8_t *rxData, int rxNumBytes, struct txvc_mempool *pool) {
    struct byte_copier_rx_callback_extra *e =
        txvc_mempool_alloc_object(pool, struct byte_copier_rx_callback_extra);
    e->dst = rxData;
    e->numBytes = rxNumBytes;
    return ft_transaction_add_write_to_chip_with_readback(t, txData, txNumBytes,
            byte_copier_rx_callback_fn, e, rxNumBytes);
}

static bool check_device_in_sync(struct driver *d) {
    /* Send bad opcode and check that chip responds with "BadCommand" */
    const uint8_t cmd[1] = { 0xab, };
    uint8_t resp[2];
    return ft_transaction_add_write_to_chip_with_readback_simple(&d->transaction, cmd, 1, resp, 2, &d->pool)
        && ft_transaction_flush(&d->transaction)
        && resp[0] == OP_BAD_COMMANDS
        && resp[1] == cmd[0];
}

static bool append_tms_shift_to_transaction(struct driver *d,
        const uint8_t* tms, int fromBitIdx, int toBitIdx) {
    ALWAYS_ASSERT(fromBitIdx >= 0);
    ALWAYS_ASSERT(toBitIdx >= 0);
    ALWAYS_ASSERT(toBitIdx > fromBitIdx);

    while (fromBitIdx < toBitIdx) {
        /*
         * In theory it is possible to send up to 7 TMS bits per command but we reserve one to
         * duplicate the last bit which is needed to guarantee that TMS wire level is unchanged
         * after command is completed.
         */
        const int maxTmsBitsPerCommand = 6;
        const int bitsToTransfer = min(toBitIdx - fromBitIdx, maxTmsBitsPerCommand);
        uint8_t cmd[] = {
            OP_SHIFT_WR_TMS_FLAG | OP_SHIFT_LSB_FIRST_FLAG
                | OP_SHIFT_BITMODE_FLAG | OP_SHIFT_WR_FALLING_FLAG,
            bitsToTransfer - 1,
            (!!d->lastTdi) << 7,
        };
        copy_bits(tms, fromBitIdx, &cmd[2], 0, bitsToTransfer, true);
        fromBitIdx += bitsToTransfer;
        if (!ft_transaction_add_write_to_chip(&d->transaction, cmd, 3)) {
            return false;
        }
    }
    return true;
}

static bool append_tdi_shift_to_transaction(struct driver *d,
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
            const uint8_t cmd[] = {
                OP_SHIFT_RD_TDO_FLAG | OP_SHIFT_WR_TDI_FLAG | OP_SHIFT_LSB_FIRST_FLAG
                    | OP_SHIFT_BITMODE_FLAG | OP_SHIFT_WR_FALLING_FLAG,
                numLeadingBits - 1,
                tdi[fromBitIdx / 8] >> (fromBitIdx % 8),
            };
            struct bit_copier_rx_callback_extra *e =
                txvc_mempool_alloc_object(&d->pool, struct bit_copier_rx_callback_extra);
            e->fromBit = 8 - numLeadingBits;
            e->dst = tdo;
            e->toBit = fromBitIdx;
            e->numBits = numLeadingBits;
            if (!ft_transaction_add_write_to_chip_with_readback(&d->transaction,
                        cmd, 3, bit_copier_rx_callback_fn, e, 1)) {
                return false;
            }
            curIdx += numLeadingBits;
        }
        if (curIdx < lastBitIdx) {
            if (curIdx < innerEndIdx) {
                ALWAYS_ASSERT(innerBeginIdx % 8 == 0);
                ALWAYS_ASSERT(innerEndIdx % 8 == 0);
                ALWAYS_ASSERT(curIdx % 8 == 0);
                const int innerOctetsToSend = min((innerEndIdx - curIdx) / 8, d->chipBufferBytes);
                const uint8_t cmd[] = {
                    OP_SHIFT_RD_TDO_FLAG | OP_SHIFT_WR_TDI_FLAG
                        | OP_SHIFT_LSB_FIRST_FLAG | OP_SHIFT_WR_FALLING_FLAG,
                    ((innerOctetsToSend - 1) >> 0) & 0xff,
                    ((innerOctetsToSend - 1) >> 8) & 0xff,
                };
                if (!ft_transaction_add_write_to_chip(&d->transaction, cmd, 3)) {
                    return false;
                }
                if (!ft_transaction_add_write_to_chip_with_readback_simple(&d->transaction,
                            tdi + curIdx / 8, innerOctetsToSend,
                            tdo + curIdx / 8, innerOctetsToSend,
                            &d->pool)) {
                    return false;
                }
                curIdx += innerOctetsToSend * 8;
            }
            if (curIdx == innerEndIdx && numTrailingBits > 0) {
                const uint8_t cmd[] = {
                    OP_SHIFT_RD_TDO_FLAG | OP_SHIFT_WR_TDI_FLAG | OP_SHIFT_LSB_FIRST_FLAG
                        | OP_SHIFT_BITMODE_FLAG | OP_SHIFT_WR_FALLING_FLAG,
                    numTrailingBits - 1,
                    tdi[innerEndIdx / 8],
                };
                struct bit_copier_rx_callback_extra *e =
                    txvc_mempool_alloc_object(&d->pool, struct bit_copier_rx_callback_extra);
                e->fromBit = 8 - numTrailingBits;
                e->dst = tdo;
                e->toBit = innerEndIdx;
                e->numBits = numTrailingBits;
                if (!ft_transaction_add_write_to_chip_with_readback(&d->transaction,
                            cmd, 3, bit_copier_rx_callback_fn, e, 1)) {
                    return false;
                }
                curIdx += numTrailingBits;
            }
        }

        if (curIdx == lastBitIdx) {
            const int lastTdiBit = !!get_bit(tdi, lastBitIdx);
            const int lastTmsBit = !!lastTmsBitHigh;
            const uint8_t cmd[] = {
                OP_SHIFT_WR_TMS_FLAG | OP_SHIFT_RD_TDO_FLAG | OP_SHIFT_LSB_FIRST_FLAG
                    | OP_SHIFT_BITMODE_FLAG | OP_SHIFT_WR_FALLING_FLAG,
                0x00, /* Send 1 bit */
                (lastTdiBit << 7) | (lastTmsBit << 1) | lastTmsBit,
            };
            struct bit_copier_rx_callback_extra *e =
                txvc_mempool_alloc_object(&d->pool, struct bit_copier_rx_callback_extra);
            e->fromBit = 7; /* TDO is shifted in from the right side */
            e->dst = tdo;
            e->toBit = lastBitIdx;
            e->numBits = 1;
            if (!ft_transaction_add_write_to_chip_with_readback(&d->transaction,
                        cmd, 3, bit_copier_rx_callback_fn, e, 1)) {
                return false;
            }
            /* Let future TMS commands use proper TDI value when enqueued. */
            d->lastTdi = lastTdiBit;
            curIdx += 1;
        }
    }
    return true;
}

static bool jtag_splitter_callback(const struct txvc_jtag_split_event *event, void *extra) {
    struct driver *d = extra;
    {
        const struct txvc_jtag_split_shift_tms *e = txvc_jtag_split_cast_to_shift_tms(event);
        if (e) {
            return append_tms_shift_to_transaction(d, e->tms, e->fromBitIdx, e->toBitIdx);
        }
    }
    {
        const struct txvc_jtag_split_shift_tdi *e = txvc_jtag_split_cast_to_shift_tdi(event);
        if (e) {
            return append_tdi_shift_to_transaction(d, e->tdi, e->tdo, e->fromBitIdx,e->toBitIdx,
                    !e->incomplete);
        }
    }
    {
        const struct txvc_jtag_split_flush_all *e = txvc_jtag_split_cast_to_flush_all(event);
        if (e) {
            bool res = ft_transaction_flush(&d->transaction);
            txvc_mempool_reclaim_all(&d->pool);
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
            ERROR("Unknown chip type\n");
            goto bail_noop;
    }

    FT_STATUS lastStatus = FT_OK;

#define REQUIRE_D2XX_SUCCESS_(d2xxCallExpr, cleanupLabel)                                          \
    do {                                                                                           \
        lastStatus = (d2xxCallExpr);                                                               \
        if (lastStatus != FT_OK) {                                                                 \
            ERROR("Failed: %s: %s\n", #d2xxCallExpr, ft_status_name(lastStatus));                  \
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
    FT_DEVICE_LIST_INFO_NODE *selectedDevice = NULL;
    if (numConnectedDevices) {
        if (!channelSelector) {
            selectedDevice = &connectedDevices[0];
        } else {
            for (DWORD i = 0; i < numConnectedDevices; i++) {
                char *curSerial = connectedDevices[i].SerialNumber;
                size_t curSerialLen = strlen(curSerial);
                if (channelSelector == curSerial[curSerialLen - 1]) {
                    selectedDevice = & connectedDevices[i];
                    break;
                }
            }
        }
    }
    if (!selectedDevice) {
        ERROR("No matching device was found\n");
        goto bail_noop;
    }
    INFO("Using device \"%s\" (serial number: \"%s\")\n",
            selectedDevice->Description, selectedDevice->SerialNumber);
    REQUIRE_D2XX_SUCCESS_(FT_OpenEx(selectedDevice->SerialNumber, FT_OPEN_BY_SERIAL_NUMBER,
                &d->ftHandle), bail_cant_open);

    REQUIRE_D2XX_SUCCESS_(FT_Purge(d->ftHandle, FT_PURGE_RX | FT_PURGE_TX),  bail_usb_close);
    REQUIRE_D2XX_SUCCESS_(FT_SetChars(d->ftHandle, 0, 0, 0, 0), bail_usb_close);
    REQUIRE_D2XX_SUCCESS_(FT_SetFlowControl(d->ftHandle, FT_FLOW_RTS_CTS, 0, 0), bail_usb_close);
    REQUIRE_D2XX_SUCCESS_(FT_SetBitMode(d->ftHandle, 0x00, FT_BITMODE_RESET), bail_usb_close);
    REQUIRE_D2XX_SUCCESS_(FT_SetBitMode(d->ftHandle, 0x00, FT_BITMODE_MPSSE), bail_usb_close);

#undef REQUIRE_D2XX_SUCCESS_

    txvc_mempool_init(&d->pool, 64 * 1024);
    ft_transaction_init(&d->transaction, d->ftHandle, d->chipBufferBytes);

    d->lastTdi = 0;
    uint8_t setupCmds[] = {
        OP_SET_DBUS_LOBYTE,
        0x08, /* Initial levels: TCK=0, TDI=0, TMS=1 */
        0x0b, /* Directions: TCK=out, TDI=out, TDO=in, TMS=out */
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
    if (!ft_transaction_add_write_to_chip(&d->transaction, setupCmds, 3)
            || !ft_transaction_flush(&d->transaction)
            || !check_device_in_sync(d)) {
        ERROR("Failed to setup device\n");
        goto bail_reset_mode;
    }
    if (!txvc_jtag_splitter_init(&d->jtagSplitter, jtag_splitter_callback, d)) {
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
        ERROR(" Did you forget to \"sudo rmmod ftdi_sio\"?\n");
        ERROR("--------------------------------------------\n");
    }
bail_noop:
    return false;
}

static bool deactivate(void){
    struct driver *d = &gFtdi;
    ft_transaction_deinit(&d->transaction);
    txvc_jtag_splitter_deinit(&d->jtagSplitter);
    txvc_mempool_deinit(&d->pool);
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
    if (!ft_transaction_add_write_to_chip(&d->transaction, cmd, d->highSpeedCapable ? 4 : 3)
            || !ft_transaction_flush(&d->transaction)
            || !check_device_in_sync(d)) {
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
        FTDI_CHANNEL_LIST_ITEMS(AS_HELP_STRING)
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

