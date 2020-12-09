
#include "fixtures.h"
#include "module.h"

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <unistd.h>

#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define MAX_VECTOR_BITS 4096

static const struct txvc_module* enumerate_modules(
        bool (*fn)(const struct txvc_module *m, void *extra), void *extra) {
    /* These symbols are defined in txvc.ld */
    extern const struct txvc_module __txvc_modules_begin[];
    extern const struct txvc_module __txvc_modules_end[];

    for (const struct txvc_module* m = __txvc_modules_begin; m != __txvc_modules_end; m++) {
        if(!fn(m, extra)) {
            return m;
        }
    }
    return NULL;
}

static bool module_usage(const struct txvc_module *m, void *extra) {
    printf("\"%s\":\n%s\n", m->name(), m->help());
    return true;
}

static bool find_by_name(const struct txvc_module *m, void *extra) {
    const char* name = extra;
    return strcmp(name, m->name()) != 0;
}

static const struct txvc_module *activate_module(const char *argStr) {
    /*
     * Copy the whole argstring in a temporary buffer and cut it onto name,value chuncks.
     * Provided format is:
     * <mdule name>:<name0>=<val0>,<name1>=<val1>,<name2>=<val2>,...
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
        for (int i = 0; i < sizeof(argNames) / sizeof(argNames[0]) && cur && *cur; i++) {
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

    const struct txvc_module *m = enumerate_modules(find_by_name, (void *) name);
    if (m) {
        if (!m->activate(argNames, argValues)) {
            ERROR("Failed to activate module \"%s\"\n", name);
            m = NULL;
        }
    } else {
        ERROR("Can not find module \"%s\"\n", name);
    }
    return m;
}


static bool send_data(int s, const void* buf, size_t sz) {
    ssize_t sent = send(s, buf, sz, 0);
    if (sent < 0) {
        ERROR("Can not send %zu bytes: %s\n", sz, strerror(errno));
    } else if (sent < sz) {
        ERROR("Can not send %zu bytes in full\n", sz);
    }
    return sent == sz;
}

static bool recv_data(int s, void* buf, size_t sz) {
    ssize_t read = recv(s, buf, sz, MSG_WAITALL);
    if (read < 0) {
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

static bool cmd_getinfo(int s, const struct txvc_module *m) {
    int maxVectorBits = m->max_vector_bits();
    if (maxVectorBits > MAX_VECTOR_BITS) {
        maxVectorBits = MAX_VECTOR_BITS;
    }
    INFO("%s: responding with vector size %d\n", __func__, maxVectorBits);
    char response[64];
    int len = snprintf(response, sizeof(response), "xvcServer_v1.0:%d\n", maxVectorBits);
    return send_data(s, response, len);
}

static bool cmd_settck(int s, const struct txvc_module *m) {
    int suggestedTckPeriod = recv_xvc_int(s);
    if (suggestedTckPeriod < 0) {
        return false;
    }
    int tckPeriod = m->set_tck_period(suggestedTckPeriod);
    INFO("%s: suggested TCK period: %dns, actual: %dns\n", __func__, suggestedTckPeriod, tckPeriod);
    uint8_t response[4] = { tckPeriod >> 0, tckPeriod >> 8, tckPeriod >> 16, tckPeriod >> 24 };
    return send_data(s, response, 4);
}

static bool cmd_shift(int s, const struct txvc_module *m) {
    int numBits = recv_xvc_int(s);
    if (numBits < 0) {
        return false;
    }
    if (numBits > MAX_VECTOR_BITS) {
        ERROR("Requested too big vector size: %d (max: %d)\n", numBits, MAX_VECTOR_BITS);
        return false;
    }
    INFO("%s: shifting %d bits\n", __func__, numBits);
    int bytesPerVector = numBits / 8 + !!(numBits % 8);
    uint8_t tms[MAX_VECTOR_BITS / 8 + 1];
    uint8_t tdi[MAX_VECTOR_BITS / 8 + 1];
    if (!recv_data(s, tms, bytesPerVector) || !recv_data(s, tdi, bytesPerVector)) {
        return false;
    }
    uint8_t tdo[MAX_VECTOR_BITS / 8 + 1];
    if (!m->shift_bits(numBits, tms, tdi, tdo)) {
        return false;
    }
    return send_data(s, tdo, bytesPerVector);
}

static void run_xvc_connectin(int s, const struct txvc_module *m) {
    const struct {
        size_t prefixSz;
        const char *prefix;
        bool (*handler)(int s, const struct txvc_module *m);
    } commands[] = {
#define CMD(name) { sizeof (#name ":") - 1, #name ":", cmd_ ## name }
        CMD(getinfo),
        CMD(settck),
        CMD(shift),
#undef CMD
    };

    for (;;) {
        char command[16] = { 0 };
        ssize_t read = recv(s, command, sizeof(command), MSG_PEEK);
        if (read == 0) {
            printf("Connection was closed by peer\n");
            return;
        }
        if (read < 0) {
            ERROR("Can not read from socket: %s\n", strerror(errno));
            return;
        }

        bool shouldContinue = false;
        for (int i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
            size_t prefixSz = commands[i].prefixSz;
            if (read < prefixSz) {
                shouldContinue = true;
            } else if (strncmp(commands[i].prefix, command, prefixSz) == 0) {
                ssize_t read = recv(s, command, prefixSz, MSG_WAITALL);
                if (read != prefixSz) {
                    ERROR("Can not pop from socket queue\n");
                    return;
                }
                if (!commands[i].handler(s, m)) {
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

static void run_server(unsigned short port, const struct txvc_module *m) {
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

    printf("Start listening for incoming connections at port %d...\n", port);
    for (;;) {
        socklen_t length;
        length = sizeof(addr);
        int s = accept(serverSocket, (struct sockaddr *) &addr, &length);
        if (s < 0) {
            ERROR("Failed to accept connection: %s\n", strerror(errno));
            continue;
        }
        if (addr.sin_family == AF_INET) {
            printf("Accepted connection from %s:%d\n",
                    inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
        } else {
            printf("Accepted connection from unknown address\n");
        }
        run_xvc_connectin(s, m);
        shutdown(s, SHUT_RDWR);
        close(s);
    }
}

int main(int argc, char**argv) {
    if (argc != 2) {
        printf("Usage:\n\t%s <module name>:<name0>=<val0>,<name1>=<val1>,<name2>=<val2>,...\n",
                argv[0]);
        printf("Available modules:\n");
        enumerate_modules(module_usage, NULL);
        return 1;
    }

    const struct txvc_module *m = activate_module(argv[1]);
    if (m) {
        run_server(2542, m);
        if (!m->deactivate()) {
            ERROR("Failed to deactivate module \"%s\"\n", m->name());
        }
    }
    return 0;
}

