/* SPDX-License-Identifier: BSD-2-Clause */

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

extern void txvc_log_configure(const char *tagSpec, enum txvc_log_level minLevel,
        bool withTimestamps);

extern bool txvc_log_level_enabled(enum txvc_log_level level);

struct txvc_log_tag {
    char str[16];
    bool (*isEnabled)(struct txvc_log_tag* tag);
    unsigned curConfigId;
};

#define TXVC_LOG_TAG_INITIALIZER(tag) {                                                            \
        .str = #tag,                                                                               \
        .isEnabled = txvc_log_tag_enabled,                                                         \
        .curConfigId = 0u,                                                                         \
    }

#define TXVC_DEFAULT_LOG_TAG(tag)                                                                  \
    static struct txvc_log_tag txvc_default_log_tag = TXVC_LOG_TAG_INITIALIZER(tag)

extern bool txvc_log_tag_enabled(struct txvc_log_tag *tag);

TXVC_PRINTF_LIKE(3, 4)
extern void txvc_log(struct txvc_log_tag *tag, enum txvc_log_level level, const char *fmt, ...);

#define VERBOSE_ENABLED txvc_log_level_enabled(LOG_LEVEL_VERBOSE)
#define VERBOSE(fmt, ...) txvc_log(&txvc_default_log_tag, LOG_LEVEL_VERBOSE, (fmt), ## __VA_ARGS__)
#define INFO(fmt, ...) txvc_log(&txvc_default_log_tag, LOG_LEVEL_INFO, (fmt), ## __VA_ARGS__)
#define WARN(fmt, ...) txvc_log(&txvc_default_log_tag, LOG_LEVEL_WARN, (fmt), ## __VA_ARGS__)
#define ERROR(fmt, ...) txvc_log(&txvc_default_log_tag, LOG_LEVEL_ERROR, (fmt), ## __VA_ARGS__)
#define FATAL(fmt, ...) txvc_log(&txvc_default_log_tag, LOG_LEVEL_FATAL, (fmt), ## __VA_ARGS__)

#define ALWAYS_ASSERT(cond) \
    do { if (!(cond)) FATAL("Violated condition: \"%s\"\n", #cond); } while (0)

