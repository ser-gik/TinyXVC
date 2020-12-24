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

#pragma once

#include "utils.h"

struct txvc_log_tag {
    char str[16];
};

enum txvc_log_level {
    LOG_LEVEL_VERBOSE = 0,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
};

extern void txvc_set_log_min_level(enum txvc_log_level level);

TXVC_PRINTF_LIKE(3, 4)
extern void txvc_log(const struct txvc_log_tag *tag, enum txvc_log_level level, const char *fmt, ...);

#define TXVC_TAG_PADDED__(tag) \
    ("\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20" #tag)

#define TXVC_TAG_CHAR_INIT__(idx, tag) \
        [idx] = TXVC_TAG_PADDED__(tag)[sizeof(TXVC_TAG_PADDED__(tag)) - 2 - 15 + (idx)]

#define TXVC_DEFAULT_LOG_TAG(tag) \
    static const struct txvc_log_tag txvc_default_log_tag = { \
        .str = { \
            TXVC_TAG_CHAR_INIT__(0, tag), \
            TXVC_TAG_CHAR_INIT__(1, tag), \
            TXVC_TAG_CHAR_INIT__(2, tag), \
            TXVC_TAG_CHAR_INIT__(3, tag), \
            TXVC_TAG_CHAR_INIT__(4, tag), \
            TXVC_TAG_CHAR_INIT__(5, tag), \
            TXVC_TAG_CHAR_INIT__(6, tag), \
            TXVC_TAG_CHAR_INIT__(7, tag), \
            TXVC_TAG_CHAR_INIT__(8, tag), \
            TXVC_TAG_CHAR_INIT__(9, tag), \
            TXVC_TAG_CHAR_INIT__(10, tag), \
            TXVC_TAG_CHAR_INIT__(11, tag), \
            TXVC_TAG_CHAR_INIT__(12, tag), \
            TXVC_TAG_CHAR_INIT__(13, tag), \
            TXVC_TAG_CHAR_INIT__(14, tag), \
            TXVC_TAG_CHAR_INIT__(15, tag), \
        }, \
    }

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#endif

#define VERBOSE(fmt, ...) txvc_log(&txvc_default_log_tag, LOG_LEVEL_VERBOSE, (fmt), ## __VA_ARGS__)
#define INFO(fmt, ...) txvc_log(&txvc_default_log_tag, LOG_LEVEL_INFO, (fmt), ## __VA_ARGS__)
#define WARN(fmt, ...) txvc_log(&txvc_default_log_tag, LOG_LEVEL_WARN, (fmt), ## __VA_ARGS__)
#define ERROR(fmt, ...) txvc_log(&txvc_default_log_tag, LOG_LEVEL_ERROR, (fmt), ## __VA_ARGS__)

#ifdef __clang__
#pragma clang diagnostic pop
#endif

