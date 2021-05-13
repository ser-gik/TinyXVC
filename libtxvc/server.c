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

#include "txvc/server.h"

#include "txvc/driver.h"
#include "txvc/log.h"

#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <netinet/in.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <unistd.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>

TXVC_DEFAULT_LOG_TAG(server);

struct connection {
    int socket;
    const struct txvc_driver *driver;
    volatile sig_atomic_t *shouldTerminate;
    size_t vectorNumBytes;
    uint8_t *tmsVector;
    uint8_t *tdiVector;
    uint8_t *tdoVector;
};

static void deallocate_vectors(struct connection *conn) {
#define DEALLOC_VECTOR(name) if (conn->name) { free(conn->name); }
    DEALLOC_VECTOR(tmsVector);
    DEALLOC_VECTOR(tdiVector);
    DEALLOC_VECTOR(tdoVector);
#undef DEALLOC_VECTOR
    conn->vectorNumBytes = 0;
}

static void allocate_vectors(struct connection *conn, size_t numBytes) {
    deallocate_vectors(conn);
#define ALLOC_VECTOR(name) conn->name = malloc(numBytes); if (!conn->name) { goto bail; }
    ALLOC_VECTOR(tmsVector);
    ALLOC_VECTOR(tdiVector);
    ALLOC_VECTOR(tdoVector);
#undef ALLOC_VECTOR
    conn->vectorNumBytes = numBytes;
    return;
bail:
    FATAL("Can not allocate %zu bytes\n", numBytes);
}

static void log_vector(const char* name, const uint8_t* data, size_t numBits) {
    if (!VERBOSE_ENABLED) {
        return;
    }

    char bits[64 + 1];
    size_t row = 0u;
    for (size_t i = 0; i < numBits; i++) {
        size_t col = i % 64;
        char c = (data[i / 8] & (1 << (i % 8))) ? '1' : '0';
        bits[col] = c;
        if (i + 1 >= numBits || col == 63) {
            bits[col + 1] = '\0';
            VERBOSE("%s: %04zx: %s\n", name, row * 64, bits);
            row++;
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

static bool cmd_getinfo(struct connection *conn) {
    int maxVectorBits = conn->driver->max_vector_bits();
    if (maxVectorBits <= 0) {
        ERROR("Bad max vector bits: %d\n", maxVectorBits);
        return false;
    }
    VERBOSE("%s: responding with vector size %d\n", __func__, maxVectorBits);
    char response[64];
    int len = snprintf(response, sizeof(response), "xvcServer_v1.0:%d\n", maxVectorBits);
    return send_data(conn->socket, response, (size_t) len);
}

static bool cmd_settck(struct connection *conn) {
    int suggestedTckPeriod = recv_xvc_int(conn->socket);
    if (suggestedTckPeriod < 0) {
        return false;
    }
    int tckPeriod = conn->driver->set_tck_period(suggestedTckPeriod);
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
    return send_data(conn->socket, response, 4);
}

static bool cmd_shift(struct connection *conn) {
    int numBits = recv_xvc_int(conn->socket);
    if (numBits <= 0) {
        ERROR("Bad vector size: %d\n", numBits);
        return false;
    }
    VERBOSE("%s: shifting %d bits\n", __func__, numBits);
    size_t bytesPerVector = (size_t) numBits / 8 + !!((size_t) numBits % 8);
    if (bytesPerVector > conn->vectorNumBytes) {
        allocate_vectors(conn, bytesPerVector);
    }
    if (!recv_data(conn->socket, conn->tmsVector, bytesPerVector)
            || !recv_data(conn->socket, conn->tdiVector, bytesPerVector)) {
        return false;
    }
    log_vector("TMS", conn->tmsVector, numBits);
    log_vector("TDI", conn->tdiVector, numBits);
    if (!conn->driver->shift_bits(numBits,
                conn->tmsVector, conn->tdiVector, conn->tdoVector)) {
        return false;
    }
    log_vector("TDO", conn->tdoVector, numBits);
    return send_data(conn->socket, conn->tdoVector, bytesPerVector);
}

static void run_connectin(struct connection *conn) {
    const struct {
        size_t prefixSz;
        const char *prefix;
        bool (*handler)(struct connection *conn);
    } commands[] = {
#define CMD(name) { sizeof (#name ":") - 1, #name ":", cmd_ ## name }
        CMD(getinfo),
        CMD(settck),
        CMD(shift),
#undef CMD
    };

    while (!*conn->shouldTerminate) {
        char command[16] = { 0 };
        ssize_t sockRes = recv(conn->socket, command, sizeof(command), MSG_PEEK);
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
                ssize_t res = recv(conn->socket, command, prefixSz, MSG_WAITALL);
                size_t read = res > 0 ? (size_t) res : 0;
                if (read != prefixSz) {
                    ERROR("Can not pop from socket queue\n");
                    return;
                }
                if (!commands[i].handler(conn)) {
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

static void run_with_address(struct in_addr inAddr, in_port_t port,
        const struct txvc_driver *driver, volatile sig_atomic_t *shouldTerminate) {
    int serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket < 0) {
        ERROR("Can not create socket: %s\n", strerror(errno));
        return;
    }
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr = inAddr,
        .sin_zero = { 0 },
    };
    if (bind(serverSocket, (const struct sockaddr *)&addr, sizeof(addr))) {
        ERROR("Can not bind socket to %s:%d: %s\n", inet_ntoa(addr.sin_addr),
            ntohs(addr.sin_port), strerror(errno));
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
            struct connection conn = {
                .socket = s,
                .driver = driver,
                .shouldTerminate = shouldTerminate,
                .vectorNumBytes = 0,
                .tmsVector = NULL,
                .tdiVector = NULL,
                .tdoVector = NULL,
            };
            run_connectin(&conn);
            deallocate_vectors(&conn);
        } else {
            WARN("Ignored connection from family %d\n", peerAddr.sin_family);
        }
        shutdown(s, SHUT_RDWR);
        close(s);
    }
}

void txvc_run_server(const char *address,
        const struct txvc_driver *driver, volatile sig_atomic_t *shouldTerminate) {
    char buf[128];
    strncpy(buf, address, sizeof(buf));
    buf[sizeof(buf) - 1] = '\0';
    char* tmp = strchr(buf, ':');
    if (!tmp) goto bail_bad_addrstr;
    *tmp++ = '\0';

    const char *addrStr = buf;
    const char *portStr = tmp;

    struct in_addr addr;
    if (!inet_aton(addrStr, &addr)) goto bail_bad_addrstr;
    in_port_t port = (in_port_t) strtol(portStr, &tmp, 0);
    if (*tmp) goto bail_bad_addrstr;

    run_with_address(addr, port, driver,shouldTerminate);
    return;

bail_bad_addrstr:
    ERROR("Bad \"inet-addr:port\": %s\n", address);
}

