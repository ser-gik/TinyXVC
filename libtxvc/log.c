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

#include "txvc/log.h"
#include "txvc/defs.h"

#include <stdarg.h>
#include <stddef.h>
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
static char *gTagSpec;

static long long getTimeUs(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return ts.tv_sec * 1000ll * 1000ll + ts.tv_nsec / 1000ll;
}

__attribute__((constructor))
static void initLogger(void) {
    gOriginUs = getTimeUs();
}

static bool tag_enabled(struct txvc_log_tag *tag) {
    TXVC_UNUSED(tag);
    return true;
}

static bool tag_disabled(struct txvc_log_tag *tag) {
    TXVC_UNUSED(tag);
    return false;
}

void txvc_log_init(const char *tagSpec, enum txvc_log_level level) {
    if (!gTagSpec) free(gTagSpec);
    gTagSpec = strdup(tagSpec);
    gMinLevel = level;
}

bool txvc_log_tag_enabled(struct txvc_log_tag *tag) {
    if (tag->isEnabled != txvc_log_tag_enabled) {
        /* This tag is resolved already */
        return tag->isEnabled(tag);
    }
    /*
     * First resolve this by scanning tag spec. Once resoultion is known - update tag to use
     * quick constant checker function.
     */
    int enabled = -1;
    char* name = gTagSpec;
    for (;;) {
        char* const nameResolution = strpbrk(name, "+-");
        if (!nameResolution) break;
        const size_t nameLen = nameResolution - name;
        if (nameLen > 0 && (strncmp(tag->str, name, nameLen) == 0
                            || strncmp("all", name, nameLen) == 0)) {
            enabled = *nameResolution == '+';
        }
        name = nameResolution + 1;
    }

    if (*name != '\0') {
        txvc_log(&(struct txvc_log_tag){ .isEnabled = tag_enabled, .str = "logger", },
                LOG_LEVEL_FATAL, "Tag spec \"%s\" has no resolution at the end\n", gTagSpec);
    }
    if (enabled == -1) {
        txvc_log(&(struct txvc_log_tag){ .isEnabled = tag_enabled, .str = "logger", },
                LOG_LEVEL_FATAL, "Tag spec \"%s\" does not define resolution for tag \"%s\"\n",
                gTagSpec, tag->str);
    }

    if (enabled) {
        tag->isEnabled = tag_enabled;
        return true;
    } else {
        tag->isEnabled = tag_disabled;
        return false;
    }
}

bool txvc_log_level_enabled(enum txvc_log_level level) {
    return gMinLevel <= level;
}

void txvc_log(struct txvc_log_tag *tag, enum txvc_log_level level, const char *fmt, ...) {
    if (level < gMinLevel || !tag->isEnabled(tag)) {
        return;
    }

    va_list ap;
    va_start(ap, fmt);
    char buf[1024];
    char* head = buf;
    size_t avail = sizeof(buf);
    int prefixLen = snprintf(head, avail, "%10lld: %15s: %c: ", getTimeUs() - gOriginUs,
                                                                    tag->str, gLevelNames[level]);
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

