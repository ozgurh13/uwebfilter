#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <netinet/ip.h>
#include <netinet/tcp.h>

#include <libmnl/libmnl.h>
#include <linux/netfilter.h>
#include <libnetfilter_queue/libnetfilter_queue.h>

#include "config.h"
#include "lookup_domain.h"
#include "utils.h"
#include "logger.h"
#include "color.h"
#include "block.h"

#include "uthash.h"


static const int QUEUE_NUM = 0;

static void nfq_send_verdict(uint32_t id, int verdict);

static uint16_t min(uint16_t v1, uint16_t v2) { return v1 < v2 ? v1 : v2; }
static uint16_t from_u8be(uint16_t b1, uint16_t b2) { return (b1 << 8) + (b2 & 0xFF); }

static bool extract_sni(
	char domain[DOMAIN_LENGTH],
	const uint8_t* payload,
	const uint16_t payload_length
);


typedef struct {
	uint32_t src;
	uint32_t dst;
	uint16_t port;
} tcpflow_key_t;
static const size_t tcpflow_key_len = sizeof(tcpflow_key_t);

typedef struct {
	uint16_t expected;
	uint16_t current;

#define  MAX_PACKET_COUNT          8
#define  MAX_PACKET_SIZE        1500      /*  MTU  */
#define  FLOW_PAYLOAD_SIZE    (MAX_PACKET_COUNT * MAX_PACKET_SIZE)
	uint8_t payload[FLOW_PAYLOAD_SIZE];
	uint32_t ids[MAX_PACKET_COUNT];
	size_t index;
} tcpflow_value_t;

typedef struct {
	UT_hash_handle hh;

	time_t timestamp;

	tcpflow_key_t key;
	tcpflow_value_t value;
} tcpflow_t;

static tcpflow_t* tcpflow_create(tcpflow_key_t *key, uint16_t expected);
static void tcpflow_free(tcpflow_t *flow);
static bool tcpflow_value_append(
	tcpflow_value_t *value,
	uint32_t id,
	const uint8_t* payload,
	uint16_t payload_length
);
static tcpflow_t* tcpflow_lookup(tcpflow_key_t *key);
static void tcpflow_insert(tcpflow_t *flow);
static bool tcpflow_iscomplete(tcpflow_t *flow);
static void tcpflow_setverdict_and_free(tcpflow_t *flow, int verdict);
static void tcpflow_print(tcpflow_t *flow);


static int handler(
	const tcpflow_key_t *key,
	const char domain[DOMAIN_LENGTH]
);

static void
process(const uint32_t id, const uint8_t *packet)
{
	const struct iphdr *iphdr = (const struct iphdr *) packet;
	const struct tcphdr *tcphdr = (const struct tcphdr *) (packet + (iphdr->ihl << 2));

	const uint8_t *payload = (const uint8_t *)tcphdr + (tcphdr->th_off * 4);
	const uint16_t payload_length = ntohs(iphdr->tot_len) - (tcphdr->th_off * 4) - (iphdr->ihl * 4);

	if (payload_length < 10) {    /*  packet too short  */
		nfq_send_verdict(id, NF_ACCEPT);
		return;
	}

	tcpflow_t *flow;

	tcpflow_key_t key = {0};
	key.src = iphdr->saddr;
	key.dst = iphdr->daddr;
	key.port = ntohs(tcphdr->th_sport);

	flow = tcpflow_lookup(&key);
	if (flow != NULL) {
		if (!tcpflow_value_append(&flow->value, id, payload, payload_length)) {
			tcpflow_setverdict_and_free(flow, NF_DROP);
			return;
		}

		if (tcpflow_iscomplete(flow)) {
			int verdict = NF_ACCEPT;

			char domain[DOMAIN_LENGTH] = {0};
			if (extract_sni(domain, flow->value.payload, flow->value.current)) {
				verdict = handler(&key, domain);
			}
			if (verdict == NF_DROP) {   /*  TCP RST  */
				tcp_rst_send(packet);
			}
			tcpflow_setverdict_and_free(flow, verdict);
		}

		return;
	}

	/*  TLS packet            Client Hello       */
	if (payload[0] != 0x16 || payload[5] != 0x01) {
		nfq_send_verdict(id, NF_ACCEPT);
		return;
	}

	const uint16_t handshake_length = from_u8be(payload[3], payload[4]);
	if (payload_length >= handshake_length) {     /*  single SNI packet  */
		int verdict = NF_ACCEPT;

		char domain[DOMAIN_LENGTH] = {0};
		if (extract_sni(domain, payload, payload_length)) {
			verdict = handler(&key, domain);
		}

		if (verdict == NF_DROP) {   /*  TCP RST  */
			tcp_rst_send(packet);
		}

		nfq_send_verdict(id, verdict);
		return;
	}

	flow = tcpflow_create(&key, handshake_length);
	if (flow == NULL) {
		nfq_send_verdict(id, NF_ACCEPT);
		return;
	}

	if (!tcpflow_value_append(&flow->value, id, payload, payload_length)) {
		tcpflow_free(flow);
		nfq_send_verdict(id, NF_ACCEPT);
		return;
	}

	tcpflow_insert(flow);
}


static int
handler(const tcpflow_key_t *key, const char domain[DOMAIN_LENGTH])
{
	webfilterlog_t webfilterlog = {0};

	get_ip_address(webfilterlog.srcip, key->src);
	webfilterlog.srcport = key->port;
	get_ip_address(webfilterlog.dstip, key->dst);
	webfilterlog.dstport = 443;

	memcpy(webfilterlog.proto, "tcp", 3);
	webfilterlog.logtime = time(NULL);

	memcpy(webfilterlog.domain, domain, DOMAIN_LENGTH);
	webfilterlog.category = lookup_category_of_domain(webfilterlog.domain);

	const bool access_granted =
		is_access_to_category_allowed(webfilterlog.category);

	const char* action = access_granted ? "allow" : "block";
	const size_t action_length = strlen(action);

	memcpy(webfilterlog.action, action, action_length);

	if (debug_mode()) {
		webfilterlog_print(&webfilterlog);
	}

	webfilterlog_write(&webfilterlog);

	return access_granted ? NF_ACCEPT : NF_DROP;
}




#define  MAX_FLOW_CACHE_COUNT  32
static struct {
	tcpflow_t *cache;
	size_t size;
	time_t tcpflow_last_gc;
} tcpflow_cache = {0};

static tcpflow_t*
tcpflow_lookup(tcpflow_key_t *key)
{
	tcpflow_t *flow = NULL;
	HASH_FIND(hh, tcpflow_cache.cache, key, tcpflow_key_len, flow);
	return flow;
}

static void
tcpflow_delete(tcpflow_t *flow)
{
	HASH_DEL(tcpflow_cache.cache, flow);
	tcpflow_cache.size--;
}

static void
tcpflow_rungc(const time_t now)
{
	const time_t expired_time = now - 5;      /*  5 seconds is very generous  */

	tcpflow_t *f, *tmp;
	HASH_ITER(hh, tcpflow_cache.cache, f, tmp) {
		if (f->timestamp < expired_time) {
			tcpflow_setverdict_and_free(f, NF_DROP);
		}
	}

	tcpflow_cache.tcpflow_last_gc = now;
}

static void
tcpflow_insert(tcpflow_t *flow)
{
	HASH_ADD(hh, tcpflow_cache.cache, key, tcpflow_key_len, flow);
	tcpflow_cache.size++;

	const time_t now = time(NULL);
	/*  if we have alot of flows, run the garbage collector  */
	if ( tcpflow_cache.tcpflow_last_gc < (now - 60)
	  || tcpflow_cache.size > MAX_FLOW_CACHE_COUNT
	  ) {
		tcpflow_rungc(now);
	}
}

static tcpflow_t*
tcpflow_create(tcpflow_key_t *key, uint16_t expected)
{
	tcpflow_t *flow = calloc(1, sizeof(tcpflow_t));
	if (flow == NULL) {
		perror("calloc");
		return NULL;
	}

	flow->key.src = key->src;
	flow->key.dst = key->dst;
	flow->key.port = key->port;

	flow->value.expected = expected;

	return flow;
}

static void
tcpflow_free(tcpflow_t *flow)
{
	free(flow);
}

static bool
tcpflow_value_append(
	tcpflow_value_t *value,
	uint32_t id,
	const uint8_t* payload,
	uint16_t payload_length
)
{
	if (value->index >= MAX_PACKET_COUNT) {
		return false;
	}

	uint32_t newcurrent = (uint32_t) value->current + (uint32_t) payload_length;
	if (newcurrent > FLOW_PAYLOAD_SIZE) {
		return false;
	}

	memcpy(value->payload + value->current, payload, payload_length);
	value->current = (uint16_t) newcurrent;

	value->ids[value->index] = id;
	value->index++;

	return true;
}

static bool
tcpflow_iscomplete(tcpflow_t *flow)
{
	return flow->value.current >= flow->value.expected;
}

static void
tcpflow_setverdict(tcpflow_t *flow, int verdict)
{
	size_t size = flow->value.index;
	for (size_t i = 0; i < size; i++) {
		nfq_send_verdict(flow->value.ids[i], verdict);
	}
}

static void
tcpflow_setverdict_and_free(tcpflow_t *flow, int verdict)
{
	tcpflow_delete(flow);
	tcpflow_setverdict(flow, verdict);
	tcpflow_free(flow);
}

static void __attribute__((unused))
tcpflow_print(tcpflow_t *flow)
{
	size_t size = flow->value.index;

	char srcip[IPADDR_LENGTH] = {0};
	char dstip[IPADDR_LENGTH] = {0};

	get_ip_address(srcip, flow->key.src);
	uint16_t srcport = flow->key.port;
	get_ip_address(dstip, flow->key.dst);
	uint16_t dstport = 443;

	char packet_ids[256];
	int index = 0;

	for (size_t i = 0; i < size; i++) {
		int k = sprintf(packet_ids + index, "%u", flow->value.ids[i]);
		index += k;
		if (i != size - 1) {
			k = sprintf(packet_ids + index, " ");
			index += k;
		}
	}

	printf(
		"\n\n<<<<<<<<<<<<<<<<<<  FLOW  >>>>>>>>>>>>>>>>>>\n"
		"  src(%s%s%s:%s%u%s) -> dst(%s%s%s:%s%u%s)\n"
		"  ids[%s%s%s]\n"
		"  current/expected: %s%u%s/%s%u%s\n"
		"\n"
		"  flow count (cache): %zu\n"
		">>>>>>>>>>>>>>>>>>  ----  <<<<<<<<<<<<<<<<<<\n\n\n",
		BOLDCYAN,  srcip,                RESET,
		BOLDRED,   srcport,              RESET,
		BOLDCYAN,  dstip,                RESET,
	       	BOLDRED,   dstport,              RESET,
		GREEN,     packet_ids,           RESET,
		BLUE,      flow->value.current,  RESET,
		YELLOW,    flow->value.expected, RESET,
		           tcpflow_cache.size
	);
}




/*
 * extract SNI from ClientHello packet
 * returns true if SNI was found
 */
static bool
extract_sni(
	char domain[DOMAIN_LENGTH],
	const uint8_t* payload,
	const uint16_t payload_length
)
{
	if (payload_length < 10) { /*  packet too short  */
		return false;
	}

	if (payload[0] != 0x16) { /*  not TLS packet  */
		return false;
	}

	if (payload[5] != 0x01) { /*  not Client Hello  */
		return false;
	}

	const uint16_t handshake_length =
		from_u8be(payload[3], payload[4]);

	if (payload_length < handshake_length) { /*  packet isn't complete  */
		return false;
	}

	uint16_t offset;
	uint16_t base_offset = 43;
	uint16_t extention_offset = 2;

	if (base_offset + 2 > payload_length) {
		return false;
	}

	const uint16_t session_id_length = payload[base_offset];

	if (session_id_length + base_offset + 2 > handshake_length) {
		return false;
	}

	const uint16_t cipher_length_start = base_offset + session_id_length + 1;
	const uint16_t cipher_length =
		from_u8be(payload[cipher_length_start], payload[cipher_length_start + 1]);

	offset = base_offset + session_id_length + cipher_length + 2;
	if (offset > handshake_length) {
		return false;
	}

	const uint16_t compression_length = payload[offset + 1];

	offset += compression_length + 2;
	if (offset > handshake_length) {
		return false;
	}

	const uint16_t extentions_length =
		from_u8be(payload[offset], payload[offset + 1]);
	extention_offset += offset;

	const uint16_t end_of_extentions = extention_offset + extentions_length;

	/*  limit loop iterations  */
	size_t iterations = 50;
	while (iterations-- && extention_offset < end_of_extentions) {
		const uint16_t extention_id =
			from_u8be(payload[extention_offset], payload[extention_offset + 1]);
		extention_offset += 2;

		const uint16_t extention_length =
			from_u8be(payload[extention_offset], payload[extention_offset + 1]);
		extention_offset += 2;

		if (extention_id == 0) {   /*  SNI  */
			extention_offset += 3;     /*  skip server name list length and name_type  */

			uint16_t sni_length =
				from_u8be(payload[extention_offset], payload[extention_offset + 1]);
			extention_offset += 2;

			/*  cap the SNI length at MAX_SNI_LENGTH  */
			uint16_t copy_length = min(sni_length, DOMAIN_LENGTH);
			memcpy(domain, payload + extention_offset, copy_length);

			return true;
		}

		extention_offset += extention_length;
	}

	return false;
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
tcp443_main(void *)
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
