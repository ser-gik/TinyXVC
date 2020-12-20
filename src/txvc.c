
#include "alias.h"
#include "driver.h"
#include "log.h"
#include "server.h"
#include "utils.h"

#include <unistd.h>

#include <string.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stddef.h>

TXVC_DEFAULT_LOG_TAG(txvc);

static volatile sig_atomic_t shouldTerminate = 0;

static void sigint_handler(int signo) {
    TXVC_UNUSED(signo);
    ssize_t ignored __attribute__((unused));
    ignored = write(STDOUT_FILENO, "Terminating...\n", 15);
    shouldTerminate = 1;
}

static void listen_for_user_interrupt(void) {
    /*
     * Received SIGINT must NOT restart interrupted syscalls, so that server code will be able
     * to test termination flag in a timely manner.
     * Don't use signal() as it may force restarts.
     */
    struct sigaction sa;
    sa.sa_flags = 0; /* No SA_RESTART here */
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
}

static bool find_by_name(const struct txvc_driver *d, const void *extra) {
    const char* name = extra;
    return strcmp(name, d->name) != 0;
}

static const struct txvc_driver *activate_driver(const char *profile) {
    for (const struct txvc_profile_alias *alias = txvc_profile_aliases; alias->alias; alias++) {
        if (strcmp(profile, alias->alias) == 0) {
            INFO("Found alias %s (%s),\n", alias->alias, alias->description);
            INFO("using profile %s\n", alias->profile);
            profile = alias->profile;
        }
    }

    /*
     * Copy the whole profile string in a temporary buffer and cut it onto name,value chuncks.
     * Expected format is:
     * <driver name>:<name0>=<val0>,<name1>=<val1>,<name2>=<val2>,...
     */
    char args[1024];
    strncpy(args, profile, sizeof(args));
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

    listen_for_user_interrupt();

    txvc_set_log_min_level(LOG_LEVEL_VERBOSE);
    const struct txvc_driver *d = activate_driver(argv[1]);
    if (d) {
        txvc_run_server(0x7f000001, 2542, d, &shouldTerminate);
        if (!d->deactivate()) {
            WARN("Failed to deactivate driver \"%s\"\n", d->name);
        }
    }
    return 0;
}

