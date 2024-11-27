#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <stdbool.h>
#include "defs.h"

bool is_access_to_category_allowed(int category);
bool is_domain_from_user(char domain[DOMAIN_LENGTH]);
void config_get_serveraddr(char addr[SERVERADDR_LENGTH + 1]);

// int lookup_category_for_domain_custom(char domain[DOMAIN_LENGTH]);
// bool is_ruleid_allowed_to_access_category(int ruleid, int category);
// int lookup_webfilterprofileid_from_ruleid(int ruleid);

#endif
