#include <string.h>
#include <stdbool.h>

#include <netinet/ip.h>
#include <netinet/tcp.h>

#include "defs.h"
#include "block.h"
#include "check.h"
#include "logger.h"
#include "nfqueue.h"
#include "uwebfilterlog.h"

static bool
extract_http_payload(
	char domain[DOMAIN_LENGTH],
	char path[PATH_LENGTH],
	const uint8_t* payload,
	const uint16_t payload_length
);

void
packet_handle_tcp80(uwebfilterlog_t *uwebfilterlog, const packet_info_t *packet_info)
{
	const uint8_t *packet = packet_info->payload;

	const struct iphdr *iphdr = (const struct iphdr *) packet;
	const struct tcphdr *tcphdr = (const struct tcphdr *) (packet + (iphdr->ihl << 2));

	const unsigned char *payload = (const unsigned char *) tcphdr + (tcphdr->th_off * 4);
	const uint16_t payload_length = ntohs(iphdr->tot_len) - (tcphdr->th_off * 4) - (iphdr->ihl * 4);

	if (payload_length < 10) {    /*  packet too short  */
		nfq_send_verdict(packet_info->id, NF_ACCEPT);
		return;
	}

	if (extract_http_payload(
		uwebfilterlog->domain,
		uwebfilterlog->path,
		payload,
		payload_length
	)) {
		int verdict = check_access(uwebfilterlog, iphdr);

		if (verdict == NF_DROP) {
			logger_debug("TCP(80) RST: %s", uwebfilterlog->domain);
			tcp_rst_send(packet);
		}

		nfq_send_verdict(packet_info->id, verdict);
		return;
	}

	nfq_send_verdict(packet_info->id, NF_ACCEPT);
}

/*
 *  HTTP request payload
 * ======================
 * METHOD PATH "HTTP/x.x"
 * ...
 * "HOST:" DOMAIN
 * ...
 */
static bool
extract_http_payload(
	char domain[DOMAIN_LENGTH],
	char path[PATH_LENGTH],
	const uint8_t* payload,
	const uint16_t payload_length
)
{
	const char *http_request = (const char*) payload;
	const char *http_request_end = http_request + payload_length;

	typedef struct {
		const char* method;
		const size_t length;
	} http_method;

	static const http_method http_methods[] = {
		{ "GET ",     4 },
		{ "POST ",    5 },
		{ "PUT ",     4 },
		{ "DELETE ",  7 },
		{ "HEAD ",    5 },
		{ "OPTIONS ", 8 },
		{ "CONNECT ", 8 },
		{ "PATCH ",   6 },
		{ NULL,       0 }
	};

	bool http_method_matched = false;
	for (const http_method *h = http_methods; h->method != NULL; h++) {
		if (strncasecmp(http_request, h->method, h->length) == 0) {
			http_method_matched = true;
			break;
		}
	}

	if (!http_method_matched) {
		return false;
	}

	/*  extract PATH  */
	const char *request_line_end = strstr(http_request, "\r\n");
	if (request_line_end != NULL && request_line_end < http_request_end) {
		const char *url_start = strchr(http_request, ' ') + 1;
		const char *url_end = strchr(url_start, ' ');
		if (url_start != NULL && url_end != NULL && url_end < request_line_end) {
			const size_t url_length = url_end - url_start;
			const size_t path_length =
				(url_length < PATH_LENGTH) ? url_length : (PATH_LENGTH - 1);
			memcpy(path, url_start, path_length);
		}
	}

	/*  extract HOST  */
	const char *host_header_start = strcasestr(http_request, "\r\nHost:");
	if (host_header_start != NULL && host_header_start < http_request_end) {
		host_header_start += 8;
		const char *host_end = strchr(host_header_start, '\r');
		if (host_end != NULL && host_end < http_request_end) {
			size_t host_length = host_end - host_header_start;
			const size_t domain_length =
				(host_length < DOMAIN_LENGTH) ? host_length : (DOMAIN_LENGTH - 1);
			memcpy(domain, host_header_start, domain_length);
		}
	}

	return true;
}
