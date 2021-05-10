/*
 * Copyright 2020 Sergey Guralnik
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

#include "alias.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

static const struct {
    const char *name;
    struct txvc_profile_alias profile;
} gAliases[] = {
    {
        .name = "mimas_a7",
        {
            .description = "Numato Lab Mimas Artix 7 FPGA Module",
            .profile = "ft2232h:"
                "vid=2a19,"
                "pid=1009,"
                "channel=B,"
                "tck_idle=high,"
                "tdi_change_at=falling,"
                "tdo_sample_at=rising,"
                "d0=tck,"
                "d1=tdi,"
                "d2=tdo,"
                "d3=tms,"
                "d4=ignored,"
                "d5=ignored,"
                "d6=driver_low,"
                "d7=driver_low,",
        },
        },
        {
        .name = "mimas_a7_mini",
        {
            .description = "Numato Lab Mimas A7 Mini FPGA Development Board",
            .profile = "ft2232h:"
                "vid=2a19,"
                "pid=100E,"
                "channel=B,"
                "tck_idle=high,"
                "tdi_change_at=falling,"
                "tdo_sample_at=rising,"
                "d0=tck,"
                "d1=tdi,"
                "d2=tdo,"
                "d3=tms,"
                "d4=ignored,"
                "d5=ignored,"
                "d6=driver_low,"
                "d7=driver_low,",
        },
    },
    {
        .name = "narvi",
        {
            .description = "Numato Lab Narvi Spartan 7 FPGA Module",
            .profile = "ft2232h:"
                "vid=2a19,"
                "pid=100D,"
                "channel=B,"
                "tck_idle=high,"
                "tdi_change_at=falling,"
                "tdo_sample_at=rising,"
                "d0=tck,"
                "d1=tdi,"
                "d2=tdo,"
                "d3=tms,"
                "d4=ignored,"
                "d5=ignored,"
                "d6=driver_low,"
                "d7=driver_low,",
        },
    },
    {
        .name = "ft232h",
        {
            .description = "FT232H-based USB to JTAG cable",
            .profile = "ftdi-generic:"
                "vid=0403,"
                "pid=6014,"
                "channel=A,"
                "d4=ignored,"
                "d5=ignored,"
                "d6=ignored,"
                "d7=ignored,",
        },
    },
};

const struct txvc_profile_alias *txvc_find_alias_by_name(const char* name) {
    for (size_t i = 0; i < sizeof(gAliases) / sizeof(gAliases[0]); i++) {
        if (strcmp(name, gAliases[i].name) == 0) {
            return &gAliases[i].profile;
        }
    }
    return NULL;
}

void txvc_print_all_aliases(void) {
    for (size_t i = 0; i < sizeof(gAliases) / sizeof(gAliases[i]); i++) {
        printf("%s - %s\n", gAliases[i].name, gAliases[i].profile.description);
    }
}
