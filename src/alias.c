
#include "alias.h"

#include <stddef.h>

const struct txvc_profile_alias txvc_profile_aliases[] = {
    {
        .alias = "mimas_a7",
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
        .description = "Numato Lab Mimas Artix 7 FPGA Module",
    },
    {
        .alias = NULL,
        .profile = NULL,
        .description = NULL,
    },
};
