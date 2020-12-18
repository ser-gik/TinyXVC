
#include "log.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

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
};

void txvc_set_log_min_level(enum txvc_log_level level) {
    gMinLevel = level;
}

void txvc_log(const struct txvc_log_tag *tag, enum txvc_log_level level, const char *fmt, ...) {
    if (level < gMinLevel) {
        return;
    }

    va_list ap;
    va_start(ap, fmt);
    char buf[1024];
    const size_t tagLen = sizeof(tag->str);
    memcpy(buf, tag->str, tagLen);
    buf[tagLen + 0] = ':';
    buf[tagLen + 1] = ' ';
    buf[tagLen + 2] = gLevelNames[level];
    buf[tagLen + 3] = ':';
    buf[tagLen + 4] = ' ';
    vsnprintf(buf + tagLen + 5, sizeof(buf) - tagLen - 5, fmt, ap);
    va_end(ap);

    const char* escColor;
    switch (level) {
        case LOG_LEVEL_VERBOSE: escColor = TEXT_COLOR_RESET; break;
        case LOG_LEVEL_INFO: escColor = TEXT_COLOR_GREEN; break;
        case LOG_LEVEL_WARN: escColor = TEXT_COLOR_YELLOW; break;
        case LOG_LEVEL_ERROR: escColor = TEXT_COLOR_RED; break;
        default: escColor = TEXT_COLOR_RESET; break;
    }

    printf("%s%s%s", escColor, buf, TEXT_COLOR_RESET);
}

