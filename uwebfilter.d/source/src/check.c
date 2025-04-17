#include <stdbool.h>

#include <netinet/ip.h>

#include "domain_lookup.h"
#include "nfqueue.h"
#include "check.h"
#include "config.h"
#include "utils.h"

int
check_access(uwebfilterlog_t *uwebfilterlog, const struct iphdr *iphdr)
{
	ipaddr_uint32_to_string(uwebfilterlog->srcip, iphdr->saddr);
	ipaddr_uint32_to_string(uwebfilterlog->dstip, iphdr->daddr);

	/*
	 * check if domain is allowed
	 *   1. application
	 *   2. category
	 *   3. user defined domains
	 *     a. blacklist
	 *     b. whitelist
	 */
	bool access_granted;

	domain_request_t domain_request = {
		.domain = uwebfilterlog->domain
	};
	domain_response_t domain_response = {0};
	domain_lookup(&domain_response, &domain_request);

	uwebfilterlog->category = domain_response.category;
	uwebfilterlog->application = domain_response.application;

	access_granted = !config_is_application_blocked(domain_response.application);
	uwebfilterlog->blocktype = BLOCKTYPE_APPLICATION;

	if (access_granted) {
		access_granted = !config_is_category_blocked(domain_response.category);
		uwebfilterlog->blocktype = BLOCKTYPE_CATEGORY;
	}

	if (access_granted) {
		access_granted = !config_is_domain_blacklisted(uwebfilterlog->domain);
		uwebfilterlog->blocktype = BLOCKTYPE_USER_BLACKLIST;
	}

	if (access_granted) {
		uwebfilterlog->blocktype = BLOCKTYPE_NONE;
	}

	if (!access_granted) {
		access_granted = config_is_domain_whitelisted(uwebfilterlog->domain);
		if (access_granted) {
			uwebfilterlog->blocktype = BLOCKTYPE_USER_WHITELIST;
		}
	}

	uwebfilterlog->logtime = time(NULL);
	uwebfilterlog->action = access_granted ? "allow" : "block";

	uwebfilterlog_write(uwebfilterlog);
	uwebfilterlog_print(uwebfilterlog);

	return access_granted ? NF_ACCEPT : NF_DROP;
}
