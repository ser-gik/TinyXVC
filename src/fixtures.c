
#include "fixtures.h"

#include <string.h>
#include <stdarg.h>

const char* txvc_filename(const char* pathname) {
    const char* lastSlash = strrchr(pathname, '/');
    return lastSlash ? lastSlash + 1 : pathname;
}

const char* byte_to_bitstring(unsigned char c) {
    static const char* table[256] = {
#define STR(s)                      STR_(s)
#define STR_(s)                     # s
#define BITSTR_(a,b,c,d,e,f,g,h)    a ## b ## c ## d ## e ## f ## g ## h
#define BITSTR(a,b,c,d,e,f,g,h)     STR(BITSTR_(a,b,c,d,e,f,g,h))

#define G(a,b,c,d,e,f,g)            BITSTR(a,b,c,d,e,f,g,0), BITSTR(a,b,c,d,e,f,g,1)
#define F(a,b,c,d,e,f)              G(a,b,c,d,e,f,0), G(a,b,c,d,e,f,1)
#define E(a,b,c,d,e)                F(a,b,c,d,e,0), F(a,b,c,d,e,1)
#define D(a,b,c,d)                  E(a,b,c,d,0), E(a,b,c,d,1)
#define C(a,b,c)                    D(a,b,c,0), D(a,b,c,1)
#define B(a,b)                      C(a,b,0), C(a,b,1)
#define A(a)                        B(a,0), B(a,1)
#define ALL()                       A(0), A(1)

    ALL()

#undef STR
#undef STR_
#undef BITSTR_
#undef BITSTR
#undef G
#undef F
#undef E
#undef D
#undef C
#undef B
#undef A
#undef ALL
    };
    return table[c];
}

void str_buf_append(struct str_buf* buf, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int res = vsnprintf(buf->cur__, buf->avail__, fmt,ap);
    va_end(ap);
    if (res < 0) {
        return;
    }
    if (res > buf->avail__) {
        buf->avail__ = 0;
        buf->cur__ = buf->str + sizeof(buf->str);
        return;
    }
    buf->avail__ -= res;
    buf->cur__ += res;
}

