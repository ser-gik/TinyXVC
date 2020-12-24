
#pragma once

struct txvc_profile_alias {
    const char *description;
    const char *profile;
};

extern const struct txvc_profile_alias *txvc_find_alias_by_name(const char* name);
extern void txvc_print_all_aliases(void);

