
#include "server.h"

#include "log.h"
#include "utils.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <unistd.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>

TXVC_DEFAULT_LOG_TAG(server);

#define MAX_VECTOR_BITS (32 * 1024)

static void log_vector(const char* name, const uint8_t* data, size_t sz) {
    while (sz) {
        size_t lineItems = sz > 8 ? 8 : sz;
        VERBOSE("%s: %s %s %s %s %s %s %s %s\n", name,
                lineItems > 0 ? byte_to_bitstring(data[0]) : "",
                lineItems > 1 ? byte_to_bitstring(data[1]) : "",
                lineItems > 2 ? byte_to_bitstring(data[2]) : "",
                lineItems > 3 ? byte_to_bitstring(data[3]) : "",
                lineItems > 4 ? byte_to_bitstring(data[4]) : "",
                lineItems > 5 ? byte_to_bitstring(data[5]) : "",
                lineItems > 6 ? byte_to_bitstring(data[6]) : "",
                lineItems > 7 ? byte_to_bitstring(data[7]) : "");
        sz -= lineItems;
        if (sz) {
            data += lineItems;
        }
    }
}

static bool send_data(int s, const void* buf, size_t sz) {
    ssize_t res = send(s, buf, sz, 0);
    size_t sent = res > 0 ? (size_t) res : 0;
    if (res < 0) {
        ERROR("Can not send %zu bytes: %s\n", sz, strerror(errno));
    } else if (sent < sz) {
        ERROR("Can not send %zu bytes in full\n", sz);
    }
    return sent == sz;
}

static bool recv_data(int s, void* buf, size_t sz) {
    ssize_t res = recv(s, buf, sz, MSG_WAITALL);
    size_t read = res > 0 ? (size_t) res : 0;
    if (res < 0) {
        ERROR("Can not receive %zu bytes: %s\n", sz, strerror(errno));
    } else if (read < sz) {
        ERROR("Can not receive %zu bytes in full\n", sz);
    }
    return read == sz;
}

static int recv_xvc_int(int s) {
    uint8_t payload[4];
    if (recv_data(s, payload, 4)) {
        return (payload[3] << 24) | (payload[2] << 16) | (payload[1] << 8) | (payload[0] << 0);
    }
    return -1;
}

static bool cmd_getinfo(int s, const struct txvc_driver *d) {
    int maxVectorBits = d->max_vector_bits();
    if (maxVectorBits > MAX_VECTOR_BITS) {
        maxVectorBits = MAX_VECTOR_BITS;
    }
    VERBOSE("%s: responding with vector size %d\n", __func__, maxVectorBits);
    char response[64];
    int len = snprintf(response, sizeof(response), "xvcServer_v1.0:%d\n", maxVectorBits);
    return send_data(s, response, (size_t) len);
}

static bool cmd_settck(int s, const struct txvc_driver *d) {
    int suggestedTckPeriod = recv_xvc_int(s);
    if (suggestedTckPeriod < 0) {
        return false;
    }
    int tckPeriod = d->set_tck_period(suggestedTckPeriod);
    if (tckPeriod <= 0) {
        ERROR("%s: bad period: %dns\n", __func__, tckPeriod);
        return false;
    }
    VERBOSE("%s: suggested TCK period: %dns, actual: %dns\n", __func__, suggestedTckPeriod, tckPeriod);
    uint8_t response[4] = {
        (uint8_t) (tckPeriod >> 0),
        (uint8_t) (tckPeriod >> 8),
        (uint8_t) (tckPeriod >> 16),
        (uint8_t) (tckPeriod >> 24)
    };
    return send_data(s, response, 4);
}

static bool cmd_shift(int s, const struct txvc_driver *d) {
    int numBits = recv_xvc_int(s);
    if (numBits < 0 || numBits > MAX_VECTOR_BITS) {
        ERROR("Bad vector size: %d (max: %d)\n", numBits, MAX_VECTOR_BITS);
        return false;
    }
    VERBOSE("%s: shifting %d bits\n", __func__, numBits);
    size_t bytesPerVector = (size_t) numBits / 8 + !!((size_t) numBits % 8);
    uint8_t tms[MAX_VECTOR_BITS / 8 + 1];
    uint8_t tdi[MAX_VECTOR_BITS / 8 + 1];
    if (!recv_data(s, tms, bytesPerVector) || !recv_data(s, tdi, bytesPerVector)) {
        return false;
    }
    log_vector("TMS", tms, bytesPerVector);
    log_vector("TDI", tdi, bytesPerVector);
    uint8_t tdo[MAX_VECTOR_BITS / 8 + 1];
    if (!d->shift_bits(numBits, tms, tdi, tdo)) {
        return false;
    }
    log_vector("TDO", tdo, bytesPerVector);
    return send_data(s, tdo, bytesPerVector);
}

static void run_connectin(int s, const struct txvc_driver *d,
        volatile sig_atomic_t *shouldTerminate) {
    const struct {
        size_t prefixSz;
        const char *prefix;
        bool (*handler)(int s, const struct txvc_driver *m);
    } commands[] = {
#define CMD(name) { sizeof (#name ":") - 1, #name ":", cmd_ ## name }
        CMD(getinfo),
        CMD(settck),
        CMD(shift),
#undef CMD
    };

    while (!*shouldTerminate) {
        char command[16] = { 0 };
        ssize_t sockRes = recv(s, command, sizeof(command), MSG_PEEK);
        if (sockRes == 0) {
            INFO("Connection was closed by peer\n");
            return;
        }
        if (sockRes < 0) {
            ERROR("Can not read from socket: %s\n", strerror(errno));
            return;
        }
        size_t sockRead = (size_t) sockRes;

        bool shouldContinue = false;
        for (size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
            size_t prefixSz = commands[i].prefixSz;
            if (sockRead < prefixSz) {
                shouldContinue = true;
            } else if (strncmp(commands[i].prefix, command, prefixSz) == 0) {
                ssize_t res = recv(s, command, prefixSz, MSG_WAITALL);
                size_t read = res > 0 ? (size_t) res : 0;
                if (read != prefixSz) {
                    ERROR("Can not pop from socket queue\n");
                    return;
                }
                if (!commands[i].handler(s, d)) {
                    return;
                }
                shouldContinue = true;
                break;
            }
        }

        if (!shouldContinue) {
            ERROR("No command recognized\n");
            return;
        }
    }
}

void txvc_run_server(uint32_t inAddr, uint16_t port,
        const struct txvc_driver *driver, volatile sig_atomic_t *shouldTerminate) {
    int serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket < 0) {
        ERROR("Can not create socket: %s\n", strerror(errno));
        return;
    }
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr = { htonl(inAddr) },
        .sin_zero = { 0 },
    };
    if (bind(serverSocket, (const struct sockaddr *)&addr, sizeof(addr))) {
        ERROR("Can not bind socket to port %d: %s\n", port, strerror(errno));
        close(serverSocket);
        return;
    }
    if (listen(serverSocket, 0)) {
        ERROR("Can not listen on socket: %s\n", strerror(errno));
        close(serverSocket);
        return;
    }

    INFO("Listening for incoming connections at %s:%d...\n", inet_ntoa(addr.sin_addr),
            ntohs(addr.sin_port));
    while (!*shouldTerminate) {
        struct sockaddr_in peerAddr;
        socklen_t length = sizeof(peerAddr);
        int s = accept(serverSocket, (struct sockaddr *) &peerAddr, &length);
        if (s < 0) {
            ERROR("Failed to accept connection: %s\n", strerror(errno));
            continue;
        }
        if (peerAddr.sin_family == AF_INET) {
            INFO("Accepted connection from %s:%d\n", inet_ntoa(peerAddr.sin_addr),
                    ntohs(peerAddr.sin_port));
            run_connectin(s, driver, shouldTerminate);
        } else {
            WARN("Ignored connection from family %d\n", peerAddr.sin_family);
        }
        shutdown(s, SHUT_RDWR);
        close(s);
    }
}

