#include <stdlib.h>

#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>

#include <libmnl/libmnl.h>
#include <linux/netfilter.h>
#include <libnetfilter_queue/libnetfilter_queue.h>

#include "nfqueue.h"
#include "uwebfilterlog.h"


static const int QUEUE_NUM = 0;

static struct mnl_socket *nl;

/*
 * verdict := NF_ACCEPT | NF_DROP
 */
void
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




void packet_handle_udp53(uwebfilterlog_t *uwebfilterlog, const packet_info_t *packet_info);
void packet_handle_tcp80(uwebfilterlog_t *uwebfilterlog, const packet_info_t *packet_info);
void packet_handle_tcp443(uwebfilterlog_t *uwebfilterlog, const packet_info_t *packet_info);

static void
packet_handle(const packet_info_t *packet_info)
{
	const uint8_t *packet = packet_info->payload;
	const struct iphdr *iphdr = (const struct iphdr *) packet;

	uwebfilterlog_t uwebfilterlog = {0};

	switch (iphdr->protocol) {
		case IPPROTO_TCP:
			{
				uwebfilterlog.proto = "tcp";

				const struct tcphdr *tcphdr = (const struct tcphdr *)
					(packet + (iphdr->ihl << 2));

				uwebfilterlog.srcport = ntohs(tcphdr->th_sport);
				uwebfilterlog.dstport = ntohs(tcphdr->th_dport);

				switch (uwebfilterlog.dstport) {
					case 80:
						packet_handle_tcp80(&uwebfilterlog, packet_info);
						return;

					case 443:
						packet_handle_tcp443(&uwebfilterlog, packet_info);
						return;
				}
			}
			break;

		case IPPROTO_UDP:
			{
				uwebfilterlog.proto = "udp";

				const struct udphdr *udphdr = (const struct udphdr *)
					(packet + (iphdr->ihl << 2));

				uwebfilterlog.srcport = ntohs(udphdr->uh_sport);
				uwebfilterlog.dstport = ntohs(udphdr->uh_dport);

				switch (uwebfilterlog.dstport) {
					case 53:
						packet_handle_udp53(&uwebfilterlog, packet_info);
						return;
				}
			}
			break;
	}

	nfq_send_verdict(packet_info->id, NF_ACCEPT);
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
                fprintf(stderr, "metaheader not set\n");
                return MNL_CB_ERROR;
        }

        ph = mnl_attr_get_payload(attr[NFQA_PACKET_HDR]);

	packet_info_t packet_info = {
        	.id = ntohl(ph->packet_id),
		.payload = mnl_attr_get_payload(attr[NFQA_PAYLOAD]),
		.payload_length = mnl_attr_get_payload_len(attr[NFQA_PAYLOAD])
	};

	packet_handle(&packet_info);

        return MNL_CB_OK;
}


void
nfqueue_main(void)
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
}
