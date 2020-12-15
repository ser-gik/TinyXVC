
#pragma once

struct txvc_log_tag {
    char str[16];
};

__attribute__((format(printf, 3, 4)))
extern void txvc_log(const struct txvc_log_tag *tag, char level, const char *fmt, ...);

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

#define VERBOSE(fmt, ...) txvc_log(&txvc_default_log_tag, 'V', (fmt), ## __VA_ARGS__)
#define INFO(fmt, ...) txvc_log(&txvc_default_log_tag, 'I', (fmt), ## __VA_ARGS__)
#define WARN(fmt, ...) txvc_log(&txvc_default_log_tag, 'W', (fmt), ## __VA_ARGS__)
#define ERROR(fmt, ...) txvc_log(&txvc_default_log_tag, 'E', (fmt), ## __VA_ARGS__)

#ifdef __clang__
#pragma clang diagnostic pop
#endif

