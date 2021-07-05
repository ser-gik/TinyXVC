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
#include "config/txvc.h"
#include "driver_wrapper.h"

#include "drivers/drivers.h"
#include "txvc/driver.h"
#include "txvc/log.h"
#include "txvc/server.h"
#include "txvc/profile.h"
#include "txvc/defs.h"

#include <unistd.h>

#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdbool.h>

TXVC_DEFAULT_LOG_TAG(txvc);

#define DEFAULT_SERVER_ADDR "127.0.0.1:2542"
#define DEFAULT_LOG_TAG_SPEC "all+"

#define CLI_OPTION_LIST_ITEMS(OPT_FLAG, OPT)                                                       \
    OPT_FLAG("h", help, "Print this message.")                                                     \
    OPT("p", profile, "Hardware profile or profile alias."                                         \
            " HW profile is a specification that defines a \"backend\" to be used to communicate"  \
            " with FPGA and its parameters. HW profile is specified in the following"              \
            " form:\n\n\t<driver_name>:<arg0>=<val0>,<arg1>=<val1>,<arg2>=<val2>,...\n\n"          \
            "Use '-D' to see available driver names as well as their specific parameters."         \
            " Also there are a few predefined profile aliases for specific HW that can be used"    \
            " instead of fully specified descriptions, use '-A' to see available aliases.",        \
            "profile_spec_or_alias", const char *, optarg, NULL)                                   \
    OPT("a", serverAddr, "IPv4 address and port to listen for incoming"                            \
                         " XVC connections at (default: " DEFAULT_SERVER_ADDR ").",                \
            "ipv4_address:port", const char *, optarg, DEFAULT_SERVER_ADDR)                        \
    OPT("t", tckPeriodNanos, "Enforced TCK period, expressed in nanoseconds.",                     \
            "tck_period_ns", int, parse_int(optarg), 0)                                            \
    OPT_FLAG("D", helpDrivers, "Print available drivers.")                                         \
    OPT_FLAG("A", helpAliases, "Print available aliases.")                                         \

#define ENV_LIST_ITEMS(ENV_FLAG, ENV)                                                              \
    ENV_FLAG("TXVC_LOG_VERBOSE", logVerbose, "Enable verbose logging")                             \
    ENV_FLAG("TXVC_LOG_TIMESTAMPS", logTimestamps, "Prefix logs with timestamp")                   \
    ENV("TXVC_LOG_SPEC", logTagSpec, "Log tags to enable/disable."                                 \
             " A sequence of tags names where each name is followed by '+' to enable"              \
             " or '-' to disable it. Use 'all[+-]' to enable or disable all tags."                 \
             " E.g. 'foo-all+bar-' enables all tags except for 'bar'."                             \
             " (default '" DEFAULT_LOG_TAG_SPEC "')",                                              \
             const char *, envVal, DEFAULT_LOG_TAG_SPEC)                                           \

struct config {
#define AS_STRUCT_FIELD_FLAG(optChar, name, description) bool name;
#define AS_STRUCT_FIELD(optChar, name, description, optArg, type, initializer, defVal) type name;
    CLI_OPTION_LIST_ITEMS(AS_STRUCT_FIELD_FLAG, AS_STRUCT_FIELD)
#undef AS_STRUCT_FIELD_FLAG
#undef AS_STRUCT_FIELD

#define AS_STRUCT_FIELD_FLAG(envName, name, description) bool name;
#define AS_STRUCT_FIELD(envName, name, description, type, initializer, defVal) type name;
    ENV_LIST_ITEMS(AS_STRUCT_FIELD_FLAG, AS_STRUCT_FIELD)
#undef AS_STRUCT_FIELD_FLAG
#undef AS_STRUCT_FIELD
};

static volatile sig_atomic_t shouldTerminate = 0;
const char *txvcProgname;

static void sigint_handler(int signo) {
    TXVC_UNUSED(signo);
    ssize_t ignored __attribute__((unused));
    ignored = write(STDOUT_FILENO, "Terminating...\n", 15);
    shouldTerminate = 1;
}

static void listen_for_user_interrupt(void) {
    /*
     * Received SIGINT must NOT restart interrupted syscalls, so that blocking I/O calls will
     * return immediately to let test termination flag in a timely manner.
     * Don't use signal() as it may force restarts.
     */
    struct sigaction sa;
    sa.sa_flags = 0; /* No SA_RESTART here */
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = sigint_handler;
    sigaction(SIGINT, &sa, NULL);
}

static bool driver_usage(const struct txvc_driver *d, const void *extra) {
    TXVC_UNUSED(extra);
    printf("\"%s\":\n%s\n", d->name, d->help);
    return true;
}

static bool print_help(const char *progname, const struct config *config) {
    bool didPrint = false;
    if (config->help) {
#define AS_SYNOPSYS_ENTRY_FLAG(optChar, name, description)                                         \
        "[-" optChar "]"
#define AS_SYNOPSYS_ENTRY(optChar, name, description, optArg, type, initializer, defVal)           \
        "[-" optChar " <" optArg ">]"
        const char *optionsSynopsys =
            CLI_OPTION_LIST_ITEMS(AS_SYNOPSYS_ENTRY_FLAG, AS_SYNOPSYS_ENTRY);
#undef AS_SYNOPSYS_ENTRY_FLAG
#undef AS_SYNOPSYS_ENTRY
#define AS_USAGE_ENTRY_FLAG(optChar, name, description)                                            \
        " -" optChar " : " description "\n"
#define AS_USAGE_ENTRY(optChar, name, description, optArg, type, initializer, defVal)              \
        " -" optChar " : " description "\n"
        const char *optionsUsage = CLI_OPTION_LIST_ITEMS(AS_USAGE_ENTRY_FLAG, AS_USAGE_ENTRY);
#undef AS_USAGE_ENTRY_FLAG
#undef AS_USAGE_ENTRY

        printf("%s - %s, v%s\n\n"
               "Usage:\n"
               "\t%s %s\n"
               "\n%s\n",
               TXVC_PROJECT_NAME, TXVC_DESCRIPTION, TXVC_VERSION,
               progname, optionsSynopsys,
               optionsUsage);

#define AS_USAGE_ENTRY_FLAG(envName, name, description)                                            \
        envName " - " description " (non-zero to activate)\n"
#define AS_USAGE_ENTRY(envName, name, description, type, initializer, defVal)                      \
        envName " - " description "\n"
        const char *usageEnvVars = ENV_LIST_ITEMS(AS_USAGE_ENTRY_FLAG, AS_USAGE_ENTRY);
#undef AS_USAGE_ENTRY_FLAG
#undef AS_USAGE_ENTRY
        printf("Environment variables:\n%s\n", usageEnvVars);
        didPrint = true;
    }
    if (config->helpDrivers) {
        printf("Drivers:\n");
        txvc_enumerate_drivers(driver_usage, NULL);
        printf("\n");
        didPrint = true;
    }
    if (config->helpAliases) {
        printf("Aliases:\n");
        txvc_print_all_aliases();
        printf("\n");
        didPrint = true;
    }
    return didPrint;
}

static int parse_int(const char* s) {
    char *p;
    int res = strtol(s, &p, 0);
    return *s == '\0' || *p != '\0' ? INT_MIN : res;
}

static bool load_config(int argc, char **argv, struct config *out) {
#define APPLY_DEFAULTS_FLAG(optChar, name, description)                                            \
    out->name = false;
#define APPLY_DEFAULTS(optChar, name, description, optArg, type, initializer, defVal)              \
    out->name = defVal;
    CLI_OPTION_LIST_ITEMS(APPLY_DEFAULTS_FLAG, APPLY_DEFAULTS)
#undef APPLY_DEFAULTS_FLAG
#undef APPLY_DEFAULTS

#define AS_OPTSTR_FLAG(optChar, name, description) optChar
#define AS_OPTSTR(optChar, name, description, optArg, type, initializer, defVal) optChar ":"
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
#define APPLY_OPTION(optChar, name, description, optArg, type, initializer, defVal)                \
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
        return false;
    }

#define READ_ENV_VAR(envName, name, description, type, initializer, defVal)                        \
    do {                                                                                           \
        const char *envVal = getenv(envName);                                                      \
        out->name = envVal ? (initializer) : defVal;                                               \
    } while (0);
#define READ_ENV_FLAG(envName, name, description)                                                  \
        READ_ENV_VAR(envName, name, description, bool, parse_int(envVal) != 0, false)
    ENV_LIST_ITEMS(READ_ENV_FLAG, READ_ENV_VAR)
#undef READ_ENV_VAR
#undef READ_ENV_FLAG
    return true;
}

static bool find_by_name(const struct txvc_driver *d, const void *extra) {
    const char* name = extra;
    return strcmp(name, d->name) != 0;
}

int main(int argc, char**argv) {
    txvcProgname = argv[0];
    listen_for_user_interrupt();

    struct config config = { 0 };
    if (!load_config(argc, argv, &config)) {
        print_help(argv[0], &(struct config) { .help = true, });
        return EXIT_FAILURE;
    }

    if (print_help(argv[0], &config)) {
        return EXIT_SUCCESS;
    }

    txvc_log_configure(config.logTagSpec,
            config.logVerbose ? LOG_LEVEL_VERBOSE : LOG_LEVEL_INFO,
            config.logTimestamps);
    if (!config.profile) {
        fprintf(stderr, "Profile is missing\n");
        return EXIT_FAILURE;
    } else {
        const struct txvc_profile_alias *alias = txvc_find_alias_by_name(config.profile);
        if (alias) {
            INFO("Found alias %s (%s),\n", config.profile, alias->description);
            INFO("Using profile %s\n", alias->profile);
            config.profile = alias->profile;
        }
    }
    if (config.tckPeriodNanos < 0) {
        fprintf(stderr, "Bad TCK period\n");
        return EXIT_FAILURE;
    }

    struct txvc_backend_profile profile;
    if (!txvc_backend_profile_parse(config.profile, &profile)) {
        return EXIT_FAILURE;
    }

    const struct txvc_driver *driver = txvc_enumerate_drivers(find_by_name, profile.driverName);
    if (!driver) {
        ERROR("Can not find driver \"%s\"\n", profile.driverName);
        return EXIT_FAILURE;
    }

    if (!driver->activate(profile.numArg, profile.argKeys, profile.argValues)) {
        ERROR("Failed to activate driver \"%s\"\n", profile.driverName);
        return EXIT_FAILURE;
    }

    txvc_driver_wrapper_setup(driver, config.tckPeriodNanos);
    driver = &txvcDriverWrapper;

    txvc_run_server(config.serverAddr, driver, &shouldTerminate);
    driver->deactivate();
    return EXIT_SUCCESS;
}

