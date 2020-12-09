
#pragma once

#include <stdio.h>

#define INFO(fmt, ...) \
    (void) fprintf(stdout, "%s:%d INFO: " fmt, txvc_filename(__FILE__), __LINE__, ## __VA_ARGS__)
#define WARN(fmt, ...) \
    (void) fprintf(stdout, "%s:%d WARN: " fmt, txvc_filename(__FILE__), __LINE__, ## __VA_ARGS__)
#define ERROR(fmt, ...) \
    (void) fprintf(stderr, "%s:%d ERROR: " fmt, txvc_filename(__FILE__), __LINE__, ## __VA_ARGS__)

extern const char* txvc_filename(const char* pathname);

