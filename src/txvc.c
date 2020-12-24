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
#include <stdlib.h>
#include <stdbool.h>

TXVC_DEFAULT_LOG_TAG(txvc);

#define DEFAULT_SERVER_ADDR "127.0.0.1:2542"

#define CLI_OPTION_LIST_ITEMS(OPT_FLAG, OPT)                                                       \
    OPT_FLAG("h", help, "Print this message")                                                      \
    OPT_FLAG("v", verbose, "Enable verbose output")                                                \
    OPT("p", profile, "Server HW profile or profile alias",                                        \
            "profile_string_or_alias", const char *, optarg)                                       \
    OPT("a", serverAddr, "IPv4 address and port to listen for incoming"                            \
                         " XVC connections at (default: " DEFAULT_SERVER_ADDR ")",                 \
            "ipv4_address:port", const char *, optarg)                                             \

struct cli_options {
#define AS_STRUCT_FIELD_FLAG(optChar, name, description) bool name;
#define AS_STRUCT_FIELD(optChar, name, description, optArg, type, initializer) type name;
    CLI_OPTION_LIST_ITEMS(AS_STRUCT_FIELD_FLAG, AS_STRUCT_FIELD)
#undef AS_STRUCT_FIELD_FLAG
#undef AS_STRUCT_FIELD
};

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

static bool driver_usage(const struct txvc_driver *d, const void *extra) {
    TXVC_UNUSED(extra);
    printf("\"%s\":\n%s\n", d->name, d->help);
    return true;
}

static void printUsage(const char *progname, bool detailed) {
#define AS_SYNOPSYS_ENTRY_FLAG(optChar, name, description)                                         \
    "[-" optChar "]"
#define AS_SYNOPSYS_ENTRY(optChar, name, description, optArg, type, initializer)                   \
    "[-" optChar " <" optArg ">]"
    const char *synopsysOptions = CLI_OPTION_LIST_ITEMS(AS_SYNOPSYS_ENTRY_FLAG, AS_SYNOPSYS_ENTRY);
#undef AS_SYNOPSYS_ENTRY_FLAG
#undef AS_SYNOPSYS_ENTRY
#define AS_USAGE_ENTRY_FLAG(optChar, name, description)                                            \
    " -" optChar " : " description "\n"
#define AS_USAGE_ENTRY(optChar, name, description, optArg, type, initializer)                      \
    " -" optChar " : " description "\n"
    const char *usageOptions = CLI_OPTION_LIST_ITEMS(AS_USAGE_ENTRY_FLAG, AS_USAGE_ENTRY);
#undef AS_USAGE_ENTRY_FLAG
#undef AS_USAGE_ENTRY

    if (detailed) {
        printf("TinyXVC - minimalistic XVC (Xilinx Virtual Cable) server, v0.0\n\n");
    }
    printf("Usage: %s %s\n"
           "%s\n",
           progname, synopsysOptions, usageOptions);
    if (detailed) {
        printf("\tProfiles:\n");
        printf("HW profile is a specification that defines a backend to be used by server"
               " and its parameters. Backend here means a particular device that eventually"
               " receives and answers to XVC commands. HW profile is specified in the following"
               " form:\n\n\t<driver_name>:<arg0>=<val0>,<arg1>=<val1>,<arg2>=<val2>,...\n\n"
               "Available driver names as well as their specific parameters are listed below."
               " Also there are a few predefined profile aliases for specific HW that can be used"
               " instead of fully specified description, see below.\n\n");
        printf("\tDrivers:\n");
        txvc_enumerate_drivers(driver_usage, NULL);
        printf("\n");
        printf("\tAliases:\n");
        txvc_print_all_aliases();
        printf("\n");
    }
}

static bool parse_cli_options(int argc, char **argv, struct cli_options *out) {
    memset(out, 0, sizeof(*out));
#define AS_OPTSTR_FLAG(optChar, name, description) optChar
#define AS_OPTSTR(optChar, name, description, optArg, type, initializer) optChar ":"
    const char *optstr = CLI_OPTION_LIST_ITEMS(AS_OPTSTR_FLAG, AS_OPTSTR);
#undef AS_OPTSTR_FLAG
#undef AS_OPTSTR
    int opt;
    while ((opt = getopt(argc, argv, optstr)) != -1) {
#define APPLY_OPTION_FLAG(optChar, name, description)                                              \
        if (opt == optChar[0]) {                                                                   \
            out->name = true;                                                                      \
            continue;                                                                              \
        }
#define APPLY_OPTION(optChar, name, description, optArg, type, initializer)                        \
        if (opt == optChar[0]) {                                                                   \
            out->name = initializer;                                                               \
            continue;                                                                              \
        }
        CLI_OPTION_LIST_ITEMS(APPLY_OPTION_FLAG, APPLY_OPTION)
#undef APPLY_OPTION_FLAG
#undef APPLY_OPTION
        return false;
    }

    if (argv[optind] != NULL) {
        fprintf(stderr, "%s: unrecognized extra operands\n", argv[0]);
    }
    return argv[optind] == NULL;
}

static bool find_by_name(const struct txvc_driver *d, const void *extra) {
    const char* name = extra;
    return strcmp(name, d->name) != 0;
}

static const struct txvc_driver *activate_driver(const char *profile) {
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

    const struct txvc_driver *driver = txvc_enumerate_drivers(find_by_name, name);
    if (driver) {
        if (!driver->activate(argNames, argValues)) {
            ERROR("Failed to activate driver \"%s\"\n", name);
            driver = NULL;
        }
    } else {
        ERROR("Can not find driver \"%s\"\n", name);
    }
    return driver;
}

int main(int argc, char**argv) {
    listen_for_user_interrupt();

    struct cli_options opts = { 0 };
    if (!parse_cli_options(argc, argv, &opts)) {
        printUsage(argv[0], false);
        return EXIT_FAILURE;
    }

    txvc_set_log_min_level(opts.verbose ? LOG_LEVEL_VERBOSE : LOG_LEVEL_INFO);
    if (opts.help) {
        printUsage(argv[0], true);
        return EXIT_SUCCESS;
    }
    if (!opts.profile) {
        fprintf(stderr, "Profile is missing\n");
        return EXIT_FAILURE;
    }
    const struct txvc_profile_alias *alias = txvc_find_alias_by_name(opts.profile);
    if (alias) {
        INFO("Found alias %s (%s),\n", opts.profile, alias->description);
        INFO("using profile %s\n", alias->profile);
        opts.profile = alias->profile;
    }
    if (!opts.serverAddr) {
        opts.serverAddr = DEFAULT_SERVER_ADDR;
    }
    const struct txvc_driver *driver = activate_driver(opts.profile);
    if (!driver) {
        return EXIT_FAILURE;
    }

    txvc_run_server(opts.serverAddr, driver, &shouldTerminate);
    driver->deactivate();
    return EXIT_SUCCESS;
}

