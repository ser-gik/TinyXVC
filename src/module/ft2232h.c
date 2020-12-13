
#include "module.h"
#include "fixtures.h"

#include <ftdi.h>

#include <unistd.h>

#include <stdio.h>

struct profile {
    const char* name;
    enum ftdi_interface channel;
    int vendorId;
    int productId;
};

struct tck_period {
    int periodNs;
    unsigned char dividerHigh;
    unsigned char dividerLow;
};

static struct {
    const struct profile *profile;
    struct ftdi_version_info info;
    struct ftdi_context ctx;
    struct tck_period curTckPeriod;
} gFtdi;

__attribute__((used))
static const struct profile gProfiles[] = {
    {
        .name = "mimas_a7",
        .channel = INTERFACE_B,
        .vendorId = 0x2a19,
        .productId = 0x1009,
    },
};

static int ft2232h_set_tck_period(int tckPeriodNs);

static struct tck_period get_closest_tck_period(int periodNs) {
    /*
     * TCK frequency and divider value are related in the next way:
     *
     * TCK period = 12MHz / (( 1 +[ (0xValueH * 256) OR 0xValueL] ) * 2)
     *
     * Round-up divider to get nearest period that is not greater than requested.
     */
    int divider = ((6 * periodNs + 999) / 1000) - 1;
    return (struct tck_period){
        .periodNs = 1000 * (1 + divider) / 6,
        .dividerHigh = divider / 256,
        .dividerLow = divider % 256,
    };
}


static const char * ft2232h_name(void){
    return "ft2232h";
}

static const char * ft2232h_help(void){
    return "Sends vectors to the device behind FT2232H chip, which is connected to this machine USB\n"
           "Parameters: profile=<name>\n";
}

static bool ft2232h_activate(const char **argNames, const char **argValues){
    gFtdi.info = ftdi_get_library_version();
    printf("libftdi %s %s\n", gFtdi.info.version_str, gFtdi.info.snapshot_str);

    int err = ftdi_init(&gFtdi.ctx);
    if (err) {
        ERROR("Can't init ftdi context: %d %s\n", err, ftdi_get_error_string(&gFtdi.ctx));
        return false;
    }
    err = ftdi_set_interface(&gFtdi.ctx, INTERFACE_A); /* TODO change to B */
    if (err) {
        ERROR("Can't select ftdi channel: %d %s\n", err, ftdi_get_error_string(&gFtdi.ctx));
        goto bail_0;
    }
    err = ftdi_usb_open(&gFtdi.ctx, 0x2a19, 0x1009);
    if (err) {
        ERROR("Can't open device: %d %s\n", err, ftdi_get_error_string(&gFtdi.ctx));
        goto bail_1;
    }
    err = ftdi_set_bitmode(&gFtdi.ctx, 0x00, BITMODE_RESET);
    if (err) {
        ERROR("Can't reset device: %d %s\n", err, ftdi_get_error_string(&gFtdi.ctx));
        goto bail_1;
    }
    err = ftdi_set_bitmode(&gFtdi.ctx, 0x00, BITMODE_MPSSE);
    if (err) {
        ERROR("Can't switch device to MPSSE mode: %d %s\n", err, ftdi_get_error_string(&gFtdi.ctx));
        goto bail_1;
    }

    gFtdi.curTckPeriod = (struct tck_period) { .periodNs = 0, .dividerLow = 0, .dividerHigh = 0, };
    if (ft2232h_set_tck_period(100) == 0) {
        goto bail_2;
    }

    unsigned char c = LOOPBACK_START;
    if (ftdi_write_data(&gFtdi.ctx, &c, 1) != 1) {
        ERROR("Can't enable loopback\n");
    }

    return true;

bail_2:
    ftdi_disable_bitbang(&gFtdi.ctx);
bail_1:
    ftdi_usb_close(&gFtdi.ctx);
bail_0:
    ftdi_deinit(&gFtdi.ctx);
    return false;
}

static bool ft2232h_deactivate(void){

    unsigned char c = LOOPBACK_END;
    if (ftdi_write_data(&gFtdi.ctx, &c, 1) != 1) {
        ERROR("Can't disable loopback\n");
    }

    ftdi_disable_bitbang(&gFtdi.ctx);
    ftdi_usb_close(&gFtdi.ctx);
    ftdi_deinit(&gFtdi.ctx);
    return true;
}

static int ft2232h_max_vector_bits(void){
    return 1024 * 8; /* TODO Datasheet says buffers are 4kB, should we increase this value ? */
}

static int ft2232h_set_tck_period(int tckPeriodNs){
    struct tck_period newPeriod = get_closest_tck_period(tckPeriodNs);
    if (newPeriod.periodNs != gFtdi.curTckPeriod.periodNs) {
        unsigned char packet[] = { TCK_DIVISOR, newPeriod.dividerLow, newPeriod.dividerHigh };
        int count = ftdi_write_data(&gFtdi.ctx, packet, sizeof(packet));
        if (count != sizeof(packet)) {
            ERROR("Can't set TCK divider [%x %x]: %d %s\n", newPeriod.dividerLow, newPeriod.dividerHigh,
                    count, count >=0 ? "" : ftdi_get_error_string(&gFtdi.ctx));
        } else {
            gFtdi.curTckPeriod = newPeriod;
        }
    }
    return gFtdi.curTckPeriod.periodNs;
}

static bool ft2232h_shift_bits(int numBits, const uint8_t *tmsVector, const uint8_t *tdiVector,
        uint8_t *tdoVector){


















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

