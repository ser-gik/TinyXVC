/*
 * Copyright 2021 Sergey Guralnik
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

#include "ttest/test.h"
#include "txvc/server.h"

#include <netinet/in.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

TEST_SUITE(Server)

static int mock_max_vector_bit(void);
static int mock_set_tck_period(int tckPeriodNs);
static bool mock_shift_bits(int numBits,
        const uint8_t *tmsVector, const uint8_t *tdiVector, uint8_t *tdoVector);

static struct {
    int callCountMaxVectorBit;
    int callCountSetTckPeriod;
    int callCountShiftBits;
    const struct txvc_driver driver;
} gDriverMock = {
    .driver = {
        .name = "mock",
        .help = "",
        .activate = 0,
        .deactivate = 0,
        .max_vector_bits = mock_max_vector_bit,
        .set_tck_period = mock_set_tck_period,
        .shift_bits = mock_shift_bits,
    },
};

static int mock_max_vector_bit(void) {
    gDriverMock.callCountMaxVectorBit++;
    return 123;
}

static int mock_set_tck_period(int tckPeriodNs) {
    gDriverMock.callCountSetTckPeriod++;
    return tckPeriodNs + 10;
}

static bool mock_shift_bits(int numBits,
        const uint8_t *tmsVector, const uint8_t *tdiVector, uint8_t *tdoVector) {
    gDriverMock.callCountShiftBits++;
    int numBytes = numBits / 8 + !!(numBits % 8);
    for (int i = 0; i < numBytes; i++) {
        tdoVector[i] = tmsVector[i] ^ tdiVector[i];
    }
    return true;
}

static void reset_driver_mock(void) {
    gDriverMock.callCountMaxVectorBit = 0;
    gDriverMock.callCountSetTckPeriod = 0;
    gDriverMock.callCountShiftBits = 0;
}

static int gClientSocket;
static sig_atomic_t gServerShouldTerminate;
static const char * const gServerAddr = "127.0.0.1"; 
static in_port_t gServerPort = 9000;
static pthread_t gServerThread;

static void* server_thread(void* arg) {
    (void) arg;
    char addr[32];
    snprintf(addr, sizeof(addr), "%s:%d", gServerAddr, gServerPort);
    txvc_run_server(addr, &gDriverMock.driver, &gServerShouldTerminate);
    return NULL;
}

DO_BEFORE_EACH_CASE() {
    reset_driver_mock();
    gServerShouldTerminate = 0;
    pthread_create(&gServerThread, NULL, server_thread, NULL);
    usleep(100 * 1000); /* Let server to start */
    gClientSocket = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_TRUE(gClientSocket >= 0);
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(gServerAddr);
    addr.sin_port = htons(gServerPort);
    int connectRes = connect(gClientSocket, (struct sockaddr*)&addr, sizeof(addr));
    if (connectRes != 0) {
        FAIL_FATAL("Can not connect to %s:%d - %s", gServerAddr, gServerPort, strerror(errno));
    }
}

DO_AFTER_EACH_CASE() {
    gServerShouldTerminate = 1;
    shutdown(gClientSocket, SHUT_RDWR);
    close(gClientSocket);
    pthread_join(gServerThread, NULL);
    /*
     * Server connection is now in a TIME_WAIT, and it's address is unusable for some time.
     * Instruct next cases to use different port to let their servers to bind(2) immediately.
     */
    gServerPort++;
}

TEST_CASE(RequestInfo_DriverIsCalledAndResponseIsReceived) {
    const char expectedResponse[] = "xvcServer_v1.0:123\n";
    const size_t expectedResponseSz = sizeof(expectedResponse) - 1;
    char responseBuffer[64];

    ASSERT_EQ(0, gDriverMock.callCountMaxVectorBit);

    ASSERT_EQ(send(gClientSocket, "getinfo:", 8, 0), 8);
    ASSERT_EQ(recv(gClientSocket, responseBuffer, expectedResponseSz, 0), expectedResponseSz);
    responseBuffer[expectedResponseSz] = '\0';
    ASSERT_EQ(CSTR(expectedResponse), CSTR(responseBuffer));
    ASSERT_EQ(1, gDriverMock.callCountMaxVectorBit);

    ASSERT_EQ(send(gClientSocket, "getinfo:", 8, 0), 8);
    ASSERT_EQ(recv(gClientSocket, responseBuffer, expectedResponseSz, 0), expectedResponseSz);
    responseBuffer[expectedResponseSz] = '\0';
    ASSERT_EQ(CSTR(expectedResponse), CSTR(responseBuffer));
    ASSERT_EQ(2, gDriverMock.callCountMaxVectorBit);
}

TEST_CASE(RequestTckPeriodChange_DriverIsCalledAndResponseIsReceived) {
    uint8_t response[4];

    ASSERT_EQ(0, gDriverMock.callCountSetTckPeriod);

    ASSERT_EQ(send(gClientSocket, &(uint8_t []){'s', 'e', 't', 't', 'c', 'k', ':', 100, 0, 0, 0, },
                11, 0), 11);
    ASSERT_EQ(recv(gClientSocket, response, 4, 0), 4);
    EXPECT_EQ(SPAN(((uint8_t[]){ 110, 0, 0, 0, }), 4), SPAN(response, 4));
    ASSERT_EQ(1, gDriverMock.callCountSetTckPeriod);

    ASSERT_EQ(send(gClientSocket, &(uint8_t []){'s', 'e', 't', 't', 'c', 'k', ':', 255, 0, 0, 0, },
                11, 0), 11);
    ASSERT_EQ(recv(gClientSocket, response, 4, 0), 4);
    EXPECT_EQ(SPAN(((uint8_t[]){ 9, 1, 0, 0, }), 4), SPAN(response, 4));
    ASSERT_EQ(2, gDriverMock.callCountSetTckPeriod);
}

TEST_CASE(RequestShiftBits_DriverIsCalledAndResponseIsReceived) {
    const uint8_t request[] = { 's', 'h', 'i', 'f', 't', ':',
        64, 0, 0, 0, /* <num bits> */
        0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0, /* <tms vector> */
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* <tdi vector> */
    };
    const uint8_t expectedTdo[] = {
        0x12 ^ 0xff, 0x34 ^ 0xff, 0x56 ^ 0xff, 0x78 ^ 0xff,
        0x9a ^ 0xff, 0xbc ^ 0xff, 0xde ^ 0xff, 0xf0 ^ 0xff,
    };
    uint8_t actualTdo[sizeof(expectedTdo)] = { 0 };

    ASSERT_EQ(0, gDriverMock.callCountShiftBits);
    ASSERT_EQ(send(gClientSocket, request, sizeof(request), 0), sizeof(request));
    ASSERT_EQ(recv(gClientSocket, actualTdo, sizeof(actualTdo), 0), sizeof(actualTdo));
    ASSERT_EQ(1, gDriverMock.callCountShiftBits);
    ASSERT_EQ(SPAN(expectedTdo, sizeof(expectedTdo)), SPAN(actualTdo, sizeof(actualTdo)));
}

