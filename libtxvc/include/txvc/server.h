/* SPDX-License-Identifier: BSD-2-Clause */

#pragma once

#include "driver.h"

#include <signal.h>

extern void txvc_run_server(const char *address,
        const struct txvc_driver *driver, volatile sig_atomic_t *shouldTerminate);

