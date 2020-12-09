
#include "fixtures.h"

#include <string.h>

extern const char* txvc_filename(const char* pathname) {
    const char* lastSlash = strrchr(pathname, '/');
    return lastSlash ? lastSlash + 1 : pathname;
}

