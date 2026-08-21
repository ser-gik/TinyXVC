/* SPDX-License-Identifier: BSD-2-Clause */

#include "txvc/profile.h"

#include "txvc/log.h"

#include <string.h>

TXVC_DEFAULT_LOG_TAG(profile);

bool txvc_backend_profile_parse(const char *profileStr, struct txvc_backend_profile *out) {
    size_t len = strlen(profileStr);
    if (len > sizeof(out->privateScratchpad) - 1) {
        ERROR("Too long profile spec\n");
        return false;
    }
    memcpy(out->privateScratchpad, profileStr, len);
    out->privateScratchpad[len] = '\0';

    out->driverName = out->privateScratchpad;
    out->numArg = 0;

    char* cur = strchr(out->privateScratchpad, ':');
    if (cur) {
        *cur++ = '\0';
        while (cur && *cur) {
            if (out->numArg >= sizeof(out->argKeys) / sizeof(out->argKeys[0])) {
                ERROR("Too many profile args\n");
                return false;
            }
            char* tmp = cur;
            cur = strchr(cur, ',');
            if (cur) {
                *cur++ = '\0';
            }
            out->argKeys[out->numArg] = tmp;
            tmp = strchr(tmp, '=');
            if (tmp) {
                *tmp++ = '\0';
                out->argValues[out->numArg] = tmp;
            } else {
                out->argValues[out->numArg] = "";
            }
            out->numArg++;
        }
    }
    return true;
}

