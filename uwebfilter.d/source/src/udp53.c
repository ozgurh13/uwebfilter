#include <string.h>
#include <stdbool.h>

#include <netinet/ip.h>
#include <netinet/udp.h>

#include "defs.h"
#include "block.h"
#include "check.h"
#include "logger.h"
#include "nfqueue.h"
#include "uwebfilterlog.h"

static bool
extract_dns_domain(
	char domain[DOMAIN_LENGTH],
	const uint8_t *packet,
	const size_t packet_length
);


void
packet_handle_udp53(uwebfilterlog_t *uwebfilterlog, const packet_info_t *packet_info)
{
	const uint8_t *packet = packet_info->payload;
	const uint16_t packet_length = packet_info->payload_length;

	const struct iphdr *iphdr = (const struct iphdr *) packet;

	const uint8_t *payload = (const uint8_t *)
		(packet + ((iphdr->ihl) * sizeof(uint32_t)) + sizeof(struct udphdr));
	const uint16_t payload_length =
		packet_length - ((iphdr->ihl) * sizeof(uint32_t)) - sizeof(struct udphdr);

	if (extract_dns_domain(uwebfilterlog->domain, payload, payload_length)) {
		int verdict = check_access(uwebfilterlog, iphdr);

		if (verdict == NF_DROP) {  /*  FAKE DNS RESPONSE  */
			logger_debug("DNS Fake Response: %d", uwebfilterlog->domain);
			dns_answer_send(packet);
		}

		nfq_send_verdict(packet_info->id, verdict);
		return;
	}

	nfq_send_verdict(packet_info->id, NF_ACCEPT);
}

struct dnshdr {
	uint16_t id;
	uint16_t flags;
	uint16_t qdcount;
	uint16_t ancount;
	uint16_t nscount;
	uint16_t arcount;
};

static bool
extract_dns_domain(char domain[DOMAIN_LENGTH], const uint8_t *packet, const size_t packet_length)
{
	if (packet_length < sizeof(struct dnshdr)) {
		return false;
	}

	const struct dnshdr *dnshdr = (const struct dnshdr*) packet;
	const uint16_t qdcount = ntohs(dnshdr->qdcount);

	if (qdcount == 0) {
		return false;
	}

	const uint8_t *dns_question = packet + sizeof(struct dnshdr);
	const uint8_t *dns_question_end = packet + packet_length;

	const uint8_t *src = dns_question;
	char *dst = domain;

	while (src < dns_question_end) {
		const uint8_t label_len = *src++;
		if (label_len == 0) {
			break;
		}

		if (dst != domain) {
			*dst++ = '.';
		}

		memcpy(dst, src, label_len);
		src += label_len;
		dst += label_len;
	}

	const size_t d_len = strlen(domain);
	const uint8_t dns_type = dns_question[d_len+3];

	if (dns_type > 17) {
		/*   TYPE HTTPS        TYPE IPV6      */
		if (dns_type != 65 && dns_type != 28) {
			return false;
		}
	}

	return true;
}
