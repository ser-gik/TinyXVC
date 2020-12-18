
#pragma once

#include "driver.h"

#include <stdbool.h>
#include <stdint.h>
#include <signal.h>

extern void txvc_run_server(uint32_t inAddr, uint16_t port,
        const struct txvc_driver *driver, volatile sig_atomic_t *shouldTerminate);

