#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "nfqueue.h"
#include "tcpflow.h"


static const size_t tcpflow_key_len = sizeof(tcpflow_key_t);

#define  MAX_FLOW_CACHE_COUNT  32
static struct {
	tcpflow_t *cache;
	time_t tcpflow_last_gc;
} tcpflow_cache = {0};

tcpflow_t*
tcpflow_lookup(tcpflow_key_t *key)
{
	tcpflow_t *tcpflow = NULL;
	HASH_FIND(hh, tcpflow_cache.cache, key, tcpflow_key_len, tcpflow);
	return tcpflow;
}

static void
tcpflow_delete(tcpflow_t *tcpflow)
{
	HASH_DEL(tcpflow_cache.cache, tcpflow);
}

static void
tcpflow_rungc(const time_t now)
{
	const time_t expired_time = now - 5;      /*  5 seconds is very generous  */

	tcpflow_t *tcpflow, *tmp;
	HASH_ITER(hh, tcpflow_cache.cache, tcpflow, tmp) {
		if (tcpflow->timestamp < expired_time) {
			tcpflow_setverdict_and_free(tcpflow, NF_DROP);
		}
	}

	tcpflow_cache.tcpflow_last_gc = now;
}

void
tcpflow_insert(tcpflow_t *tcpflow)
{
	HASH_ADD(hh, tcpflow_cache.cache, key, tcpflow_key_len, tcpflow);

	const time_t now = time(NULL);
	/*  if we have alot of flows, run the garbage collector  */
	if (tcpflow_cache.tcpflow_last_gc < (now - 60)) {
		tcpflow_rungc(now);
	}
}

tcpflow_t*
tcpflow_create(tcpflow_key_t *key, uint16_t expected)
{
	tcpflow_t *tcpflow = calloc(1, sizeof(tcpflow_t));
	if (tcpflow == NULL) {
		perror("calloc");
		return NULL;
	}

	tcpflow->key.src = key->src;
	tcpflow->key.dst = key->dst;
	tcpflow->key.port = key->port;

	tcpflow->value.expected = expected;

	return tcpflow;
}

void
tcpflow_free(tcpflow_t *tcpflow)
{
	free(tcpflow);
}


bool
tcpflow_value_append(
	tcpflow_value_t *value,
	uint32_t id,
	const uint8_t *payload,
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

bool
tcpflow_iscomplete(tcpflow_t *tcpflow)
{
	return tcpflow->value.current >= tcpflow->value.expected;
}

static void
tcpflow_setverdict(tcpflow_t *tcpflow, int verdict)
{
	size_t size = tcpflow->value.index;
	for (size_t i = 0; i < size; i++) {
		nfq_send_verdict(tcpflow->value.ids[i], verdict);
	}
}

void
tcpflow_setverdict_and_free(tcpflow_t *tcpflow, int verdict)
{
	tcpflow_delete(tcpflow);
	tcpflow_setverdict(tcpflow, verdict);
	tcpflow_free(tcpflow);
}
