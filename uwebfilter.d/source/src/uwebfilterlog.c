#include <stdio.h>

#include "uwebfilterlog.h"


void _uwebfilterlog_insert(uwebfilterlog_t *uwebfilterlog);
void _uwebfilterlog_sendto_cloud(uwebfilterlog_t *uwebfilterlog);

void
uwebfilterlog_write(uwebfilterlog_t *uwebfilterlog)
{
	_uwebfilterlog_insert(uwebfilterlog);

	_uwebfilterlog_sendto_cloud(uwebfilterlog);
}


void
uwebfilterlog_print(uwebfilterlog_t *uwebfilterlog)
{
	printf(
		"\n==========   LOG   ==========\n"
		"   SrcIP:                   %s\n"
		"   SrcPort:                 %u\n"
		"   DstIP:                   %s\n"
		"   DstPort:                 %u\n"
		"   Proto:                   %s\n"
		"   Domain:                  %s\n"
		"   Path:                    %s\n"
		"   Action:                  %s\n"
		"   Category:                %d\n"
		"   Application:             %d\n"
		"   BlockType:               %d\n"
		"   LogTime:                 %lu\n"
		"=============================\n\n",
		uwebfilterlog->srcip,
		uwebfilterlog->srcport,
		uwebfilterlog->dstip,
		uwebfilterlog->dstport,
		uwebfilterlog->proto,
		uwebfilterlog->domain,
		uwebfilterlog->path,
		uwebfilterlog->action,
		uwebfilterlog->category,
		uwebfilterlog->application,
		uwebfilterlog->blocktype,
		uwebfilterlog->logtime
	);
}
