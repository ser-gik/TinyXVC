
#include "module.h"
#include "fixtures.h"

static const char * ft2232h_name(void){
    return "ft2232h";
}

static const char * ft2232h_help(void){
    ERROR("%s: unimplemented stub\n", __func__);
    return "TO BE ADDED\n"
           "Parameters: ???\n";
}

static bool ft2232h_activate(const char **argNames, const char **argValues){
    ERROR("%s: unimplemented stub\n", __func__);
    return false;
}

static bool ft2232h_deactivate(void){
    ERROR("%s: unimplemented stub\n", __func__);
    return false;
}

static int ft2232h_max_vector_bits(void){
    ERROR("%s: unimplemented stub\n", __func__);
    return 1024;
}

static int ft2232h_set_tck_period(int tckPeriodNs){
    ERROR("%s: unimplemented stub\n", __func__);
    return tckPeriodNs;
}

static bool ft2232h_shift_bits(int numBits, const uint8_t *tmsVector, const uint8_t *tdiVector,
        uint8_t *tdoVector){
    ERROR("%s: unimplemented stub\n", __func__);
    return false;
}

TXVC_MODULE(ft2232h) = {
    .name = ft2232h_name,
    .help = ft2232h_help,
    .activate = ft2232h_activate,
    .deactivate = ft2232h_deactivate,
    .max_vector_bits = ft2232h_max_vector_bits,
    .set_tck_period = ft2232h_set_tck_period,
    .shift_bits = ft2232h_shift_bits,
};

