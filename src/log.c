
#include "log.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define TEXT_COLOR_RED "\x1b[31m"
#define TEXT_COLOR_GREEN "\x1b[32m"
#define TEXT_COLOR_YELLOW "\x1b[33m"
#define TEXT_COLOR_RESET "\x1b[0m"

void txvc_log(const struct txvc_log_tag *tag, char level, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    char buf[1024];
    const size_t tagLen = sizeof(tag->str);
    memcpy(buf, tag->str, tagLen);
    buf[tagLen + 0] = ':';
    buf[tagLen + 1] = ' ';
    buf[tagLen + 2] = level;
    buf[tagLen + 3] = ':';
    buf[tagLen + 4] = ' ';
    vsnprintf(buf + tagLen + 5, sizeof(buf) - tagLen - 5, fmt, ap);
    va_end(ap);

    const char* escColor;
    switch (level) {
        case 'V': escColor = TEXT_COLOR_RESET; break;
        case 'I': escColor = TEXT_COLOR_GREEN; break;
        case 'W': escColor = TEXT_COLOR_YELLOW; break;
        case 'E': escColor = TEXT_COLOR_RED; break;
        default: escColor = TEXT_COLOR_RESET; break;
    }

    printf("%s%s%s", escColor, buf, TEXT_COLOR_RESET);
}

