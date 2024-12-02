#ifndef __UTILS_H__
#define __UTILS_H__

#include <stdint.h>
#include <stdnoreturn.h>

#include "defs.h"

noreturn void die(const char* restrict fmt, ...);

void ipaddr_uint32_to_string(char ipaddr[IPADDR_LENGTH], uint32_t u32_ipaddr);

#endif
