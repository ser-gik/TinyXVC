/*
 * Copyright 2021 Sergey Guralnik
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

