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

#include "defs.h"

#include <stdbool.h>

enum txvc_log_level {
    LOG_LEVEL_VERBOSE = 0,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_FATAL,
};

extern void txvc_log_init(const char *tagSpec, enum txvc_log_level level);

extern bool txvc_log_level_enabled(enum txvc_log_level level);

struct txvc_log_tag {
    bool (*isEnabled)(struct txvc_log_tag* tag);
    char str[16];
};

#define TXVC_LOG_TAG_INITIALIZER(tag) {                                                            \
        .isEnabled = txvc_log_tag_enabled,                                                         \
        .str = #tag,                                                                               \
    }

#define TXVC_DEFAULT_LOG_TAG(tag)                                                                  \
    static struct txvc_log_tag txvc_default_log_tag = TXVC_LOG_TAG_INITIALIZER(tag)

extern bool txvc_log_tag_enabled(struct txvc_log_tag *tag);

TXVC_PRINTF_LIKE(3, 4)
extern void txvc_log(struct txvc_log_tag *tag, enum txvc_log_level level, const char *fmt, ...);

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#endif

#define VERBOSE_ENABLED txvc_log_level_enabled(LOG_LEVEL_VERBOSE)
#define VERBOSE(fmt, ...) txvc_log(&txvc_default_log_tag, LOG_LEVEL_VERBOSE, (fmt), ## __VA_ARGS__)
#define INFO(fmt, ...) txvc_log(&txvc_default_log_tag, LOG_LEVEL_INFO, (fmt), ## __VA_ARGS__)
#define WARN(fmt, ...) txvc_log(&txvc_default_log_tag, LOG_LEVEL_WARN, (fmt), ## __VA_ARGS__)
#define ERROR(fmt, ...) txvc_log(&txvc_default_log_tag, LOG_LEVEL_ERROR, (fmt), ## __VA_ARGS__)
#define FATAL(fmt, ...) txvc_log(&txvc_default_log_tag, LOG_LEVEL_FATAL, (fmt), ## __VA_ARGS__)

#define ALWAYS_ASSERT(cond) \
    do { if (!(cond)) FATAL("Violated condition: \"%s\"\n", #cond); } while (0)

#ifdef __clang__
#pragma clang diagnostic pop
#endif

