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

#include "log.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define TEXT_COLOR_RED "\x1b[31m"
#define TEXT_COLOR_GREEN "\x1b[32m"
#define TEXT_COLOR_YELLOW "\x1b[33m"
#define TEXT_COLOR_RESET "\x1b[0m"

static enum txvc_log_level gMinLevel = LOG_LEVEL_ERROR;

static const char gLevelNames[] = {
    [LOG_LEVEL_VERBOSE] = 'V',
    [LOG_LEVEL_INFO] = 'I',
    [LOG_LEVEL_WARN] = 'W',
    [LOG_LEVEL_ERROR] = 'E',
    [LOG_LEVEL_FATAL] = 'F',
};

static long long gOriginUs;

static long long getTimeUs(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return ts.tv_sec * 1000ll * 1000ll + ts.tv_nsec / 1000ll;
}

__attribute__((constructor))
static void initLogger(void) {
    gOriginUs = getTimeUs();
}

void txvc_set_log_min_level(enum txvc_log_level level) {
    gMinLevel = level;
}

bool txvc_log_level_enabled(enum txvc_log_level level) {
    return gMinLevel <= level;
}

void txvc_log(const struct txvc_log_tag *tag, enum txvc_log_level level, const char *fmt, ...) {
    if (level < gMinLevel) {
        return;
    }

    va_list ap;
    va_start(ap, fmt);
    char buf[1024];
    char* head = buf;
    size_t avail = sizeof(buf);
    int prefixLen = snprintf(head, avail, "%'10lld: %.*s: %c: ",
            getTimeUs() - gOriginUs, (int) sizeof(tag->str), tag->str, gLevelNames[level]);
    head += prefixLen;
    avail -= prefixLen;
    vsnprintf(head, avail, fmt, ap);
    va_end(ap);

    const char* escColor;
    switch (level) {
        case LOG_LEVEL_VERBOSE: escColor = TEXT_COLOR_RESET; break;
        case LOG_LEVEL_INFO: escColor = TEXT_COLOR_GREEN; break;
        case LOG_LEVEL_WARN: escColor = TEXT_COLOR_YELLOW; break;
        case LOG_LEVEL_ERROR:
        case LOG_LEVEL_FATAL: escColor = TEXT_COLOR_RED; break;
        default: escColor = TEXT_COLOR_RESET; break;
    }

    printf("%s%s%s", escColor, buf, TEXT_COLOR_RESET);
    fflush(stdout);
    if (level == LOG_LEVEL_FATAL) {
        abort();
    }
}

