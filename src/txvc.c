
#include "alias.h"
#include "driver.h"
#include "log.h"
#include "utils.h"

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <unistd.h>

#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <stddef.h>

TXVC_DEFAULT_LOG_TAG(txvc);

#define MAX_VECTOR_BITS (32 * 1024)

static sig_atomic_t shouldTerminate = 0;

static void sigint_handler(int signo) {
    TXVC_UNUSED(signo);
    shouldTerminate = 1;
}

static bool find_by_name(const struct txvc_driver *d, const void *extra) {
    const char* name = extra;
    return strcmp(name, d->name) != 0;
}

static const struct txvc_driver *activate_driver(const char *argStr) {
    for (const struct txvc_profile_alias *alias = txvc_profile_aliases; alias->alias; alias++) {
        if (strcmp(argStr, alias->alias) == 0) {
            INFO("Found alias %s (%s),\n", alias->alias, alias->description);
            INFO("using profile %s\n", alias->profile);
            argStr = alias->profile;
        }
    }

    /*
     * Copy the whole argstring in a temporary buffer and cut it onto name,value chuncks.
     * Provided format is:
     * <driver name>:<name0>=<val0>,<name1>=<val1>,<name2>=<val2>,...
     */
    char args[1024];
    strncpy(args, argStr, sizeof(args));
    args[sizeof(args) - 1] = '\0';

    const char *name = args;
    const char *argNames[32] = { NULL };
    const char *argValues[32] = { NULL };

    char* cur = strchr(args, ':');
    if (cur) {
        *cur++ = '\0';
        for (size_t i = 0; i < sizeof(argNames) / sizeof(argNames[0]) && cur && *cur; i++) {
            char* tmp = cur;
            cur = strchr(cur, ',');
            if (cur) {
                *cur++ = '\0';
            }
            argNames[i] = tmp;
            tmp = strchr(tmp, '=');
            if (tmp) {
                *tmp++ = '\0';
                argValues[i] = tmp;
            } else {
                argValues[i] = "";
            }
        }
    }

    const struct txvc_driver *d = txvc_enumerate_drivers(find_by_name, name);
    if (d) {
        if (!d->activate(argNames, argValues)) {
            ERROR("Failed to activate driver \"%s\"\n", name);
            d = NULL;
        }
    } else {
        ERROR("Can not find driver \"%s\"\n", name);
    }
    return d;
}

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

static void run_xvc_connectin(int s, const struct txvc_driver *d) {
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

    while (!shouldTerminate) {
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

static void run_server(unsigned short port, const struct txvc_driver *d) {
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        ERROR("Can not create socket: %s\n", strerror(errno));
        return;
    }
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr = { htonl(INADDR_LOOPBACK) },
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

    INFO("Start listening for incoming connections at port %d...\n", port);
    while (!shouldTerminate) {
        socklen_t length = sizeof(addr);
        int s = accept(serverSocket, (struct sockaddr *) &addr, &length);
        if (s < 0) {
            ERROR("Failed to accept connection: %s\n", strerror(errno));
            continue;
        }
        if (addr.sin_family == AF_INET) {
            INFO("Accepted connection from %s:%d\n",
                    inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
        } else {
            INFO("Accepted connection from unknown address\n");
        }
        run_xvc_connectin(s, d);
        shutdown(s, SHUT_RDWR);
        close(s);
    }
}

static bool driver_usage(const struct txvc_driver *d, const void *extra) {
    TXVC_UNUSED(extra);
    printf("\"%s\":\n%s\n", d->name, d->help);
    return true;
}

int main(int argc, char**argv) {
    if (argc != 2) {
        printf("Usage:\n\t%s <driver name>:<name0>=<val0>,<name1>=<val1>,<name2>=<val2>,...\n",
                argv[0]);
        printf("Available drivers:\n");
        txvc_enumerate_drivers(driver_usage, NULL);
        return 1;
    }

    struct sigaction sa;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdisabled-macro-expansion"
#endif
    sa.sa_handler = sigint_handler;
#ifdef __clang__
#pragma clang diagnostic pop
#endif
    sigaction(SIGINT, &sa, NULL);

    const struct txvc_driver *d = activate_driver(argv[1]);
    if (d) {
        run_server(2542, d);
        if (!d->deactivate()) {
            WARN("Failed to deactivate driver \"%s\"\n", d->name);
        }
    }
    return 0;
}

