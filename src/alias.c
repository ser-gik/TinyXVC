
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
    for (size_t i = 0; i < sizeof(gAliases) / sizeof(gAliases[0]); i++) {
        printf("%s - %s\n", gAliases[i].name, gAliases[i].profile.description);
    }
}
