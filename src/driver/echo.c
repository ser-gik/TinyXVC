
#include "driver.h"
#include "utils.h"

#include <stddef.h>
#include <string.h>

static bool activate(const char **argNames, const char **argValues){
    TXVC_UNUSED(argNames);
    TXVC_UNUSED(argValues);
    return true;
}

static bool deactivate(void){
    return true;
}

static int max_vector_bits(void){
    return 1024;
}

static int set_tck_period(int tckPeriodNs){
    return tckPeriodNs;
}

static bool shift_bits(int numBits, const uint8_t *tmsVector, const uint8_t *tdiVector,
        uint8_t *tdoVector){
    TXVC_UNUSED(tmsVector);
    memcpy(tdoVector, tdiVector, (size_t) (numBits / 8 + !!(numBits % 8)));
    return true;
}

TXVC_DRIVER(echo) = {
    .name = "echo",
    .help = "Simple loopback driver that forwards TDI vector to TDO. No real device is involved\n"
            "Parameters:\n   none\n",
    .activate = activate,
    .deactivate = deactivate,
    .max_vector_bits = max_vector_bits,
    .set_tck_period = set_tck_period,
    .shift_bits = shift_bits,
};

