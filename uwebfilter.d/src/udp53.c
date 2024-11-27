#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <netinet/ip.h>
#include <netinet/udp.h>

#include <libmnl/libmnl.h>
#include <linux/netfilter.h>
#include <linux/netfilter/nfnetlink.h>
#include <libnetfilter_queue/libnetfilter_queue.h>

#include "config.h"
#include "utils.h"
#include "lookup_domain.h"
#include "logger.h"
#include "block.h"

static const int QUEUE_NUM = 1;

static void nfq_send_verdict(uint32_t id, int verdict);

static bool
extract_dns_domain(
	char domain[DOMAIN_LENGTH],
	const uint8_t *packet,
	const size_t packet_length
);

static void
process(const uint32_t id, const uint8_t *packet, const uint16_t packet_length)
{
	const struct iphdr *iphdr = (const struct iphdr *) packet;
	const struct udphdr *udphdr = (const struct udphdr *) (packet + (iphdr->ihl << 2));

	const uint8_t *payload =
		(const uint8_t *)(packet + ((iphdr->ihl) * sizeof(uint32_t)) + sizeof(struct udphdr));
	const uint16_t payload_length =
		packet_length - ((iphdr->ihl) * sizeof(uint32_t)) - sizeof(struct udphdr);

	const uint32_t src = iphdr->saddr;
	const uint32_t dst = iphdr->daddr;
	const uint16_t port = ntohs(udphdr->uh_sport);

	webfilterlog_t webfilterlog = {0};

	if (!extract_dns_domain(webfilterlog.domain, payload, payload_length)) {
		nfq_send_verdict(id, NF_ACCEPT);
		return;
	}

	get_ip_address(webfilterlog.srcip, src);
	webfilterlog.srcport = port;
	get_ip_address(webfilterlog.dstip, dst);
	webfilterlog.dstport = 53;

	webfilterlog.category = lookup_category_of_domain(webfilterlog.domain);

	memcpy(webfilterlog.proto, "udp", 3);
	webfilterlog.logtime = time(NULL);

	const bool access_granted =
		is_access_to_category_allowed(webfilterlog.category);

	const char* action = access_granted ? "allow" : "block";
	const size_t action_length = strlen(action);

	memcpy(webfilterlog.action, action, action_length);

	int verdict = access_granted ? NF_ACCEPT : NF_DROP;

	if (debug_mode()) {
		webfilterlog_print(&webfilterlog);
	}

	webfilterlog_write(&webfilterlog);

	if (verdict == NF_DROP) {  /*  FAKE DNS RESPONSE  */
		dns_answer_send(packet);
	}

	nfq_send_verdict(id, verdict);
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

	if (qdcount <= 0) {
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

	size_t d_len = strlen(domain);
	int dns_type = dns_question[d_len+3];

	if (dns_type < 0 || dns_type > 17) {
		/*   TYPE HTTPS        TYPE IPV6      */
		if (dns_type != 65 && dns_type != 28) {
			return false;
		}
	}


	return true;
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
	const uint16_t payload_length = mnl_attr_get_payload_len(attr[NFQA_PAYLOAD]);

	process(id, payload, payload_length);

        return MNL_CB_OK;
}


void*
udp53_main(void *)
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
