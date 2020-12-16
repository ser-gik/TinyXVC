
#include "module.h"

#include "fixtures.h"
#include "log.h"

#include <stddef.h>
#include <string.h>

TXVC_DEFAULT_LOG_TAG(echo);

static const char * echo_name(void){
    return "echo";
}

static const char * echo_help(void){
    return "Simple loopback module that forwards TDI vector to TDO\n"
           "No real device is involved\n"
           "Parameters: none\n";
}

static bool echo_activate(const char **argNames, const char **argValues){
    while (*argNames) {
        WARN("Ignored arg: \"%s\", val: \"%s\"\n", *argNames, *argValues);
        argNames++;
        argValues++;
    }
    return true;
}

static bool echo_deactivate(void){
    return true;
}

static int echo_max_vector_bits(void){
    return 1024;
}

static int echo_set_tck_period(int tckPeriodNs){
    return tckPeriodNs;
}

static bool echo_shift_bits(int numBits, const uint8_t *tmsVector, const uint8_t *tdiVector,
        uint8_t *tdoVector){
    TXVC_UNUSED(tmsVector);
    memcpy(tdoVector, tdiVector, (size_t) (numBits / 8 + !!(numBits % 8)));
    return true;
}

TXVC_MODULE(echo) = {
    .name = echo_name,
    .help = echo_help,
    .activate = echo_activate,
    .deactivate = echo_deactivate,
    .max_vector_bits = echo_max_vector_bits,
    .set_tck_period = echo_set_tck_period,
    .shift_bits = echo_shift_bits,
};

