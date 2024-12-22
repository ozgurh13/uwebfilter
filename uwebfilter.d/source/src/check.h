#ifndef __CHECK_H__
#define __CHECK_H__

#include "uwebfilterlog.h"

int check_access(
	uwebfilterlog_t *uwebfilterlog,
	const struct iphdr *iphdr
);

#endif
