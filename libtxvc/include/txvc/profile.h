/* SPDX-License-Identifier: BSD-2-Clause */

#pragma once

#include <stdbool.h>

struct txvc_backend_profile {
    const char *driverName;
    unsigned numArg;
    const char *argKeys[32];
    const char *argValues[32];
    char privateScratchpad[1024];
};

/**
 * Expected format is:
 * <driver name>:<name0>=<val0>,<name1>=<val1>,<name2>=<val2>,...
 */
extern bool txvc_backend_profile_parse(const char *profileStr, struct txvc_backend_profile *out);

