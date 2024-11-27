#ifndef __LOOKUP_DOMAIN_H__
#define __LOOKUP_DOMAIN_H__

#include "defs.h"

typedef struct {
	char ipaddr[IPADDR_LENGTH];
	char domain[DOMAIN_LENGTH];
	char* proto;
	uint16_t port;
} domain_info_t;

int lookup_category_of_domain2(const domain_info_t *domain_info);
int lookup_category_of_domain(char domain[DOMAIN_LENGTH]);

#endif
