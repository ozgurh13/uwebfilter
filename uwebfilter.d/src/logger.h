#ifndef __LOGGER_H__
#define __LOGGER_H__

#include <stdint.h>
#include <time.h>

#include "defs.h"

typedef struct {
	char          srcip[IPADDR_LENGTH];
	uint16_t      srcport;
	char          dstip[IPADDR_LENGTH];
	uint16_t      dstport;
	char          proto[PROTO_LENGTH + 1];
	char          domain[DOMAIN_LENGTH + 1];
	char          path[PATH_LENGTH + 1];
	char          action[ACTION_LENGTH + 1];
	int           category;
	time_t        logtime;
} webfilterlog_t;

void webfilterlog_write(webfilterlog_t *webfilterlog);

void webfilterlog_print(webfilterlog_t *webfilterlog);

#endif
