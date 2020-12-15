
#include "module.h"
#include "log.h"

#include <ftdi.h>

#include <unistd.h>

#include <assert.h>
#include <string.h>

TXVC_DEFAULT_LOG_TAG(FT2232H);

#define MAX_VECTOR_BITS_PER_ROUND 2048
static_assert((MAX_VECTOR_BITS_PER_ROUND % 8) == 0, "todo");
#define MAX_VECTOR_BYTES_PER_ROUND (MAX_VECTOR_BITS_PER_ROUND / 8)

struct profile {
    const char* name;
    enum ftdi_interface channel;
    int vendorId;
    int productId;
    uint8_t directionMask;
    uint8_t (*assemblePattern)(bool tck, bool tdi, bool tms);
    bool (*extractTDO)(uint8_t pattern);
};

static struct {
    const struct profile *profile;
    struct ftdi_version_info info;
    struct ftdi_context ctx;
} gFtdi;

static uint8_t mimasa7_assemblePattern(bool tck, bool tdi, bool tms) {
    return  (tck << 0)
          | (tdi << 1)
          | (tms << 3)
          | (0u << 6)  /* Always enable JTAG pin buffers  */
          | (0u << 7); /* Newer assert PROG_B */
}

static bool mimasa7_extractTDO(uint8_t pattern) {
    return !!(pattern & (1u << 2));
}

static const struct profile gProfiles[] = {
    {
        .name = "mimas_a7",
        .channel = INTERFACE_B,
        .vendorId = 0x2a19,
        .productId = 0x1009,
        /*
         * BDBUS pin connections:
         * 0 -> TCK buf
         * 1 -> TDI buf
         * 2 <- TDO buf
         * 3 -> TMS buf
         * 4 | Not Connected
         * 5 | Not Connected
         * 6 -> OE_BUF (enable buffer for JTAG pins, active low)
         * 7 -> PROGRAM_B (active high)
         */
        .directionMask = (1u << 0)
                       | (1u << 1)
                       | (1u << 3)
                       | (1u << 6)
                       | (1u << 7),
        .assemblePattern = mimasa7_assemblePattern,
        .extractTDO = mimasa7_extractTDO,
    },
    {
        .name = NULL,
    },
};

#define REQUIRE_FTDI_SUCCESS_(ftdiCallExpr, cleanupLabel)                                          \
    do {                                                                                           \
        int err = (ftdiCallExpr);                                                                  \
        if (err != 0) {                                                                            \
            ERROR("Failed: %s: %d %s\n", #ftdiCallExpr, err, ftdi_get_error_string(&gFtdi.ctx));   \
            goto cleanupLabel;                                                                     \
        }                                                                                          \
    } while (0)

static const struct profile* find_profile(const char* name) {
    const struct profile* p = gProfiles;
    while (p->name) {
        if (strcmp(name, p->name) == 0) {
            return p;
        }
        p++;
    }
    return NULL;
}

static bool do_shift_bits(int numBits, const uint8_t *tmsVector, const uint8_t *tdiVector,
        uint8_t *tdoVector){
    if (numBits > MAX_VECTOR_BITS_PER_ROUND) {
        ERROR("Too many bits to transfer: %d (max. supported: %d)\n",
                numBits, MAX_VECTOR_BITS_PER_ROUND);
        return false;
    }

    uint8_t sendBuf[MAX_VECTOR_BITS_PER_ROUND * 2];
    uint8_t recvBuf[MAX_VECTOR_BITS_PER_ROUND * 2];

    for (int i = 0; i < numBits; i++) {
        int byteIdx = i / 8;
        int bitIdx = i % 8;
        bool tms = !!(tmsVector[byteIdx] & (1u << bitIdx));
        bool tdi = !!(tdiVector[byteIdx] & (1u << bitIdx));
        sendBuf[i * 2 + 0] = gFtdi.profile->assemblePattern(0, tdi, tms);
        sendBuf[i * 2 + 1] = gFtdi.profile->assemblePattern(1, tdi, tms);
    }

    struct ftdi_transfer_control* rdCtrl =
        ftdi_read_data_submit(&gFtdi.ctx, recvBuf, numBits * 2);
    struct ftdi_transfer_control* wrCtrl =
        ftdi_write_data_submit(&gFtdi.ctx, sendBuf, numBits * 2);

    if (ftdi_transfer_data_done(wrCtrl) != numBits * 2
            || ftdi_transfer_data_done(rdCtrl) != numBits * 2) {
        ERROR("Failed to shift %d bits: %s\n", numBits, ftdi_get_error_string(&gFtdi.ctx));
        return false;
    }

    for (int i = 0; i < numBits; i++) {
        int byteIdx = i / 8;
        int bitIdx = i % 8;

        bool tdo = gFtdi.profile->extractTDO(recvBuf[i * 2 + 1]);
        if (tdo) {
            tdoVector[byteIdx] |= 1u << bitIdx;
        } else {
            tdoVector[byteIdx] &= ~(1u << bitIdx);
        }
    }
    return true;
}

static void list_profiles(void) {
    const struct profile* p = gProfiles;
    while (p->name) {
        WARN("\t%s\n", p->name);
        p++;
    }
}

static const char * ft2232h_name(void){
    return "ft2232h";
}

static const char * ft2232h_help(void){
    return "Sends vectors to the device behind FT2232H chip, which is connected to this machine USB\n"
           "Parameters: profile=<name>\n";
}

static bool ft2232h_activate(const char **argNames, const char **argValues){
    gFtdi.profile = NULL;
    while (*argNames) {
        if (strcmp("profile", *argNames) == 0) {
            gFtdi.profile = find_profile(*argValues);
        }
        argNames++;
        argValues++;
    }
    if (!gFtdi.profile) {
        WARN("Profile was not recognized, supported profiles:\n");
        list_profiles();
        return false;
    }

    gFtdi.info = ftdi_get_library_version();
    INFO("Using libftdi \"%s %s\"\n", gFtdi.info.version_str, gFtdi.info.snapshot_str);

    REQUIRE_FTDI_SUCCESS_(ftdi_init(&gFtdi.ctx), bail_noop);
    REQUIRE_FTDI_SUCCESS_(ftdi_set_interface(&gFtdi.ctx, gFtdi.profile->channel), bail_deinit);
    REQUIRE_FTDI_SUCCESS_(
        ftdi_usb_open(&gFtdi.ctx, gFtdi.profile->vendorId, gFtdi.profile->productId), bail_deinit);
    REQUIRE_FTDI_SUCCESS_(ftdi_set_latency_timer(&gFtdi.ctx, 1), bail_usb_close);
    REQUIRE_FTDI_SUCCESS_(ftdi_setflowctrl(&gFtdi.ctx, SIO_DISABLE_FLOW_CTRL), bail_usb_close);
    REQUIRE_FTDI_SUCCESS_(ftdi_set_baudrate(&gFtdi.ctx, 1000 * 1000 / 16), bail_usb_close);
    REQUIRE_FTDI_SUCCESS_(ftdi_set_bitmode(&gFtdi.ctx, 0x00, BITMODE_RESET), bail_usb_close);
    /*
     * Write idle pattern in all-inputs mode to get correct
     * pin levels once actual direction mask is  applied
     */
    REQUIRE_FTDI_SUCCESS_(ftdi_set_bitmode(&gFtdi.ctx, 0x00, BITMODE_SYNCBB), bail_reset_mode);
    const uint8_t idlePattern = gFtdi.profile->assemblePattern(1, 1, 1);
    uint8_t dummy;
    if (ftdi_write_data(&gFtdi.ctx, &idlePattern, 1) != 1
            || ftdi_read_data(&gFtdi.ctx, &dummy, 1) != 1) {
        ERROR("Can't apply idle pattern to channel pins: %s\n", ftdi_get_error_string(&gFtdi.ctx));
        goto bail_reset_mode;
    }
    REQUIRE_FTDI_SUCCESS_(
        ftdi_set_bitmode(&gFtdi.ctx, gFtdi.profile->directionMask, BITMODE_SYNCBB), bail_reset_mode);

    return true;

bail_reset_mode:
    ftdi_set_bitmode(&gFtdi.ctx, 0x00, BITMODE_RESET);
bail_usb_close:
    ftdi_usb_close(&gFtdi.ctx);
bail_deinit:
    ftdi_deinit(&gFtdi.ctx);
bail_noop:
    return false;
}

static bool ft2232h_deactivate(void){
    ftdi_set_bitmode(&gFtdi.ctx, 0x00, BITMODE_RESET);
    ftdi_usb_close(&gFtdi.ctx);
    ftdi_deinit(&gFtdi.ctx);
    return true;
}

static int ft2232h_max_vector_bits(void){
    return MAX_VECTOR_BITS_PER_ROUND;
}

static int ft2232h_set_tck_period(int tckPeriodNs){
    int baudrate = 2 * (1000 * 1000 * 1000 / tckPeriodNs) / 16;
    int err = ftdi_set_baudrate(&gFtdi.ctx, baudrate);
    if (err) {
        ERROR("Can't set TCK period %dns: %d %s\n", tckPeriodNs, err, ftdi_get_error_string(&gFtdi.ctx));
    }
    return err ? 0 : tckPeriodNs;
}

static bool ft2232h_shift_bits(int numBits, const uint8_t *tmsVector, const uint8_t *tdiVector,
        uint8_t *tdoVector){
    for (; numBits > MAX_VECTOR_BITS_PER_ROUND;
                numBits -= MAX_VECTOR_BITS_PER_ROUND,
                tmsVector += MAX_VECTOR_BYTES_PER_ROUND,
                tdiVector += MAX_VECTOR_BYTES_PER_ROUND,
                tdoVector += MAX_VECTOR_BYTES_PER_ROUND) {
        if (!do_shift_bits(MAX_VECTOR_BITS_PER_ROUND, tmsVector, tdiVector, tdoVector)) {
            return false;
        }
    }
    return do_shift_bits(numBits, tmsVector, tdiVector, tdoVector);
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

