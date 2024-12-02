#ifndef __TCPFLOW_H__
#define __TCPFLOW_H__

#include <stdint.h>
#include <stdbool.h>

#include "uthash.h"

typedef struct {
	uint32_t src;
	uint32_t dst;
	uint16_t port;
} tcpflow_key_t;

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

tcpflow_t *tcpflow_create(tcpflow_key_t *key, uint16_t expected);
void tcpflow_free(tcpflow_t *flow);

bool tcpflow_value_append(
	tcpflow_value_t *value,
	uint32_t id,
	const uint8_t *payload,
	uint16_t payload_length
);

bool tcpflow_iscomplete(tcpflow_t *tcpflow);
void tcpflow_setverdict_and_free(tcpflow_t *tcpflow, int verdict);

tcpflow_t *tcpflow_lookup(tcpflow_key_t *key);
void tcpflow_insert(tcpflow_t *tcpflow);

#endif
