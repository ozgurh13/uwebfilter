#ifndef __UWEBFILTERLOG_H__
#define __UWEBFILTERLOG_H__

#include <stdint.h>
#include <time.h>

#include "defs.h"

typedef struct {
	char srcip[IPADDR_LENGTH];
	char dstip[IPADDR_LENGTH];
	uint16_t srcport;
	uint16_t dstport;
	const char* proto;
	char domain[DOMAIN_LENGTH];
	char path[PATH_LENGTH];
	const char* action;
	int category;
	int application;
	int blocktype;
	time_t logtime;
} uwebfilterlog_t;

void uwebfilterlog_write(uwebfilterlog_t *uwebfilterlog);
void uwebfilterlog_print(uwebfilterlog_t *uwebfilterlog);

#endif
