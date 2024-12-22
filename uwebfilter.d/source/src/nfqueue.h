#ifndef __NFQUEUE_H__
#define __NFQUEUE_H__

#include <stdint.h>
#include <linux/netfilter.h>

typedef struct {
	uint32_t id;
	uint8_t *payload;
	uint16_t payload_length;
} packet_info_t;

/*
 * verdict := NF_ACCEPT | NF_DROP
 */
void nfq_send_verdict(uint32_t id, int verdict);

#endif
