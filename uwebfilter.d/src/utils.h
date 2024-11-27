#ifndef __UTILS_H__
#define __UTILS_H__

#include <stdbool.h>
#include <stdint.h>

#include "defs.h"

void debug_set(bool value);
bool debug_mode();

void get_ip_address(char ipaddr[IPADDR_LENGTH], uint32_t u32_ip);

#endif
