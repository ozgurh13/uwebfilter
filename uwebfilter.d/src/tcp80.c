#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <netinet/ip.h>
#include <netinet/tcp.h>

#include <libmnl/libmnl.h>
#include <linux/netfilter.h>
#include <linux/netfilter/nfnetlink.h>
#include <libnetfilter_queue/libnetfilter_queue.h>

#include "config.h"
#include "utils.h"
#include "lookup_domain.h"
#include "logger.h"
#include "block.h"


static const int QUEUE_NUM = 2;

static void nfq_send_verdict(uint32_t id, int verdict);


static void
process(const uint32_t id, const uint8_t *packet)
{
	const struct iphdr *iphdr = (const struct iphdr *) packet;
	const struct tcphdr *tcphdr = (const struct tcphdr *) (packet + (iphdr->ihl << 2));

	const unsigned char *payload = (const unsigned char *)tcphdr + (tcphdr->th_off * 4);
	const uint16_t payload_length = ntohs(iphdr->tot_len) - (tcphdr->th_off * 4) - (iphdr->ihl * 4);

	if (payload_length < 10) {    /*  packet too short  */
		nfq_send_verdict(id, NF_ACCEPT);
		return;
	}

	const uint32_t src = iphdr->saddr;
	const uint32_t dst = iphdr->daddr;
	const uint16_t port = ntohs(tcphdr->th_sport);

	webfilterlog_t webfilterlog = {0};


	/*
	 *  HTTP request payload
	 * ======================
	 * METHOD PATH "HTTP/x.x"
	 * ...
	 * "HOST:" DOMAIN
	 * ...
	 */


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
		nfq_send_verdict(id, NF_ACCEPT);
		return;
	}

	/*  extract PATH  */
	const char *request_line_end = strstr(http_request, "\r\n");
	if (request_line_end != NULL && request_line_end < http_request_end) {
		const char *url_start = strchr(http_request, ' ') + 1;
		const char *url_end = strchr(url_start, ' ');
		if (url_start != NULL && url_end != NULL && url_end < request_line_end) {
			const size_t url_length = url_end - url_start;
			const size_t path_length =
				(url_length < PATH_LENGTH) ? url_length : PATH_LENGTH;
			memcpy(webfilterlog.path, url_start, path_length);
		}
	}

	/*  extract HOST  */
	const char *host_header_start = strcasestr(http_request, "\r\nHost: ");
	if (host_header_start != NULL && host_header_start < http_request_end) {
		host_header_start += 8;
		const char *host_end = strchr(host_header_start, '\r');
		if (host_end != NULL && host_end < http_request_end) {
			size_t host_length = host_end - host_header_start;
			const size_t domain_length =
				(host_length < DOMAIN_LENGTH) ? host_length : DOMAIN_LENGTH;
			memcpy(webfilterlog.domain, host_header_start, domain_length);
		}
	}

	get_ip_address(webfilterlog.srcip, src);
	webfilterlog.srcport = port;
	get_ip_address(webfilterlog.dstip, dst);
	webfilterlog.dstport = 80;

	webfilterlog.category = lookup_category_of_domain(webfilterlog.domain);

	memcpy(webfilterlog.proto, "tcp", 3);
	webfilterlog.logtime = time(NULL);

	const bool access_granted =
		is_access_to_category_allowed(webfilterlog.category);

	const char* action = access_granted ? "allow" : "block";
	memcpy(webfilterlog.action, action, strlen(action));

	if (debug_mode()) {
		webfilterlog_print(&webfilterlog);
	}

	webfilterlog_write(&webfilterlog);

	int verdict = access_granted ? NF_ACCEPT : NF_DROP;
	if (verdict == NF_DROP) {
		tcp_rst_send(packet);
	}

	nfq_send_verdict(id, verdict);
}










static struct mnl_socket *nl;


/*
 * verdict := NF_ACCEPT | NF_DROP
 */
static void
nfq_send_verdict(uint32_t id, int verdict)
{
        char buf[MNL_SOCKET_BUFFER_SIZE];
        struct nlmsghdr *nlh;

        nlh = nfq_nlmsg_put(buf, NFQNL_MSG_VERDICT, QUEUE_NUM);
        nfq_nlmsg_verdict_put(nlh, id, verdict);

        if (mnl_socket_sendto(nl, nlh, nlh->nlmsg_len) < 0) {
                perror("mnl_socket_send");
                exit(EXIT_FAILURE);
        }
}



static int
queue_cb(const struct nlmsghdr *nlh, void *data __attribute__((unused)))
{
        struct nfqnl_msg_packet_hdr *ph = NULL;
        struct nlattr *attr[NFQA_MAX+1] = {0};

        if (nfq_nlmsg_parse(nlh, attr) < 0) {
                perror("problems parsing");
                return MNL_CB_ERROR;
        }

        if (attr[NFQA_PACKET_HDR] == NULL) {
                fputs("metaheader not set\n", stderr);
                return MNL_CB_ERROR;
        }

        ph = mnl_attr_get_payload(attr[NFQA_PACKET_HDR]);

        const uint32_t id = ntohl(ph->packet_id);
	const uint8_t *payload = mnl_attr_get_payload(attr[NFQA_PAYLOAD]);

	process(id, payload);

        return MNL_CB_OK;
}


void*
tcp80_main(void *)
{
        char *buf;
        /* largest possible packet payload, plus netlink data overhead: */
        size_t sizeof_buf = 0xffff + (MNL_SOCKET_BUFFER_SIZE/2);
        struct nlmsghdr *nlh;
        int ret;
        unsigned int portid;

        nl = mnl_socket_open(NETLINK_NETFILTER);
        if (nl == NULL) {
                perror("mnl_socket_open");
                exit(EXIT_FAILURE);
        }

        if (mnl_socket_bind(nl, 0, MNL_SOCKET_AUTOPID) < 0) {
                perror("mnl_socket_bind");
                exit(EXIT_FAILURE);
        }
        portid = mnl_socket_get_portid(nl);

        buf = malloc(sizeof_buf);
        if (!buf) {
                perror("allocate receive buffer");
                exit(EXIT_FAILURE);
        }

        nlh = nfq_nlmsg_put(buf, NFQNL_MSG_CONFIG, QUEUE_NUM);
        nfq_nlmsg_cfg_put_cmd(nlh, AF_INET, NFQNL_CFG_CMD_BIND);

        if (mnl_socket_sendto(nl, nlh, nlh->nlmsg_len) < 0) {
                perror("mnl_socket_send");
                exit(EXIT_FAILURE);
        }

	const uint32_t queue_maxlen = 0xFFFF;
	mnl_attr_put_u32(nlh, NFQA_CFG_QUEUE_MAXLEN, htonl(queue_maxlen));

        nlh = nfq_nlmsg_put(buf, NFQNL_MSG_CONFIG, QUEUE_NUM);
        nfq_nlmsg_cfg_put_params(nlh, NFQNL_COPY_PACKET, 0xffff);

        mnl_attr_put_u32(nlh, NFQA_CFG_FLAGS, htonl(NFQA_CFG_F_GSO));
        mnl_attr_put_u32(nlh, NFQA_CFG_MASK, htonl(NFQA_CFG_F_GSO));

        if (mnl_socket_sendto(nl, nlh, nlh->nlmsg_len) < 0) {
                perror("mnl_socket_send");
                exit(EXIT_FAILURE);
        }

        /* ENOBUFS is signalled to userspace when packets were lost
         * on kernel side.  In most cases, userspace isn't interested
         * in this information, so turn it off.
         */
        ret = 1;
        mnl_socket_setsockopt(nl, NETLINK_NO_ENOBUFS, &ret, sizeof(int));

        for (;;) {
                ret = mnl_socket_recvfrom(nl, buf, sizeof_buf);
                if (ret == -1) {
                        perror("mnl_socket_recvfrom");
                        exit(EXIT_FAILURE);
                }

                ret = mnl_cb_run(buf, ret, 0, portid, queue_cb, NULL);
                if (ret < 0){
                        perror("mnl_cb_run");
                        exit(EXIT_FAILURE);
                }
        }

        mnl_socket_close(nl);

        return 0;
}
