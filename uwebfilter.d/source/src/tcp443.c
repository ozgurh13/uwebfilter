#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>

#include "nfqueue.h"
#include "uwebfilterlog.h"
#include "check.h"
#include "block.h"
#include "logger.h"
#include "tcpflow.h"


static uint16_t min(uint16_t v1, uint16_t v2) { return v1 < v2 ? v1 : v2; }
static uint16_t from_u8be(uint16_t b1, uint16_t b2) { return (b1 << 8) + (b2 & 0xFF); }

static bool
extract_sni(
	char domain[DOMAIN_LENGTH],
	const uint8_t* payload,
	const uint16_t payload_length
);

void
packet_handle_tcp443(uwebfilterlog_t *uwebfilterlog, const packet_info_t *packet_info)
{
	const uint8_t *packet = packet_info->payload;

	const struct iphdr *iphdr = (const struct iphdr *) packet;
	const struct tcphdr *tcphdr = (const struct tcphdr *) (packet + (iphdr->ihl << 2));

	const uint8_t *payload = (const uint8_t *) tcphdr + (tcphdr->th_off * 4);
	const uint16_t payload_length = ntohs(iphdr->tot_len) - (tcphdr->th_off * 4) - (iphdr->ihl * 4);

	if (payload_length < 10) {    /*  packet too short  */
		nfq_send_verdict(packet_info->id, NF_ACCEPT);
		return;
	}

	tcpflow_key_t key = {
		.src = iphdr->saddr,
		.dst = iphdr->daddr,
		.port = uwebfilterlog->srcport
	};
	tcpflow_t *tcpflow;
	tcpflow = tcpflow_lookup(&key);
	if (tcpflow != NULL) {
		if (!tcpflow_value_append(
			&tcpflow->value,
			packet_info->id,
			payload,
			payload_length
		)) {
			tcpflow_setverdict_and_free(tcpflow, NF_ACCEPT);
			return;
		}

		if (tcpflow_iscomplete(tcpflow)) {
			int verdict = NF_ACCEPT;

			if (extract_sni(
				uwebfilterlog->domain,
				tcpflow->value.payload,
				tcpflow->value.current
			)) {
				verdict = check_access(uwebfilterlog, iphdr);
			}

			if (verdict == NF_DROP) {   /*  TCP RST  */
				logger_debug("TCP(443) RST: %s", uwebfilterlog->domain);
				tcp_rst_send(packet);
			}

			tcpflow_setverdict_and_free(tcpflow, verdict);
		}

		return;
	}

	/*  TLS packet            Client Hello       */
	if (payload[0] != 0x16 || payload[5] != 0x01) {
		nfq_send_verdict(packet_info->id, NF_ACCEPT);
		return;
	}

	const uint16_t handshake_length = from_u8be(payload[3], payload[4]);
	if (payload_length >= handshake_length) {     /*  single SNI packet  */
		int verdict = NF_ACCEPT;

		if (extract_sni(uwebfilterlog->domain, payload, payload_length)) {
			verdict = check_access(uwebfilterlog, iphdr);
		}

		if (verdict == NF_DROP) {   /*  TCP RST  */
			logger_debug("TCP(443) RST: %s", uwebfilterlog->domain);
			tcp_rst_send(packet);
		}

		nfq_send_verdict(packet_info->id, verdict);
		return;
	}

	if (handshake_length > FLOW_PAYLOAD_SIZE) {
		nfq_send_verdict(packet_info->id, NF_ACCEPT);
		return;
	}

	tcpflow = tcpflow_create(&key, handshake_length);
	if (tcpflow == NULL) {
		nfq_send_verdict(packet_info->id, NF_ACCEPT);
		return;
	}

	if (!tcpflow_value_append(&tcpflow->value, packet_info->id, payload, payload_length)) {
		nfq_send_verdict(packet_info->id, NF_ACCEPT);
		return;
	}

	tcpflow_insert(tcpflow);
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
