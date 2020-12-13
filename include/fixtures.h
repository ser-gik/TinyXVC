
#pragma once

#include <stdio.h>

#define INFO(fmt, ...) \
    (void) fprintf(stdout, "%s:%d INFO: " fmt, txvc_filename(__FILE__), __LINE__, ## __VA_ARGS__)
#define WARN(fmt, ...) \
    (void) fprintf(stdout, "%s:%d WARN: " fmt, txvc_filename(__FILE__), __LINE__, ## __VA_ARGS__)
#define ERROR(fmt, ...) \
    (void) fprintf(stderr, "%s:%d ERROR: " fmt, txvc_filename(__FILE__), __LINE__, ## __VA_ARGS__)

extern const char* txvc_filename(const char* pathname);

extern const char* byte_to_bitstring(unsigned char c);

struct str_buf {
    char str[1024];
    char* cur__;
    size_t avail__;
};

static void inline str_buf_reset(struct str_buf* buf) {
    buf->str[0] = '\0';
    buf->cur__ = buf->str;
    buf->avail__ = sizeof(buf->str);
}

__attribute__((format(printf, 2, 3)))
extern void str_buf_append(struct str_buf* buf, const char* fmt, ...);

