/* SPDX-License-Identifier: BSD-2-Clause */

#include "alias.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

static const struct {
    const char *name;
    struct txvc_profile_alias profile;
} gAliases[] = {
#define PROFILE_ALIAS(aliasName, aliasDescription, aliasProfile)                                   \
    { .name = aliasName, { .description = aliasDescription, .profile = aliasProfile, }, },
#include "aliases_registry.inc"
#undef PROFILE_ALIAS
};

const struct txvc_profile_alias *txvc_find_alias_by_name(const char* name) {
    for (size_t i = 0; i < sizeof(gAliases) / sizeof(gAliases[0]); i++) {
        if (strcmp(name, gAliases[i].name) == 0) {
            return &gAliases[i].profile;
        }
    }
    return NULL;
}

void txvc_print_all_aliases(void) {
    for (size_t i = 0; i < sizeof(gAliases) / sizeof(gAliases[i]); i++) {
        printf("%20s - %s\n", gAliases[i].name, gAliases[i].profile.description);
    }
}
