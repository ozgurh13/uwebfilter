#ifndef __BLOCK_H__
#define __BLOCK_H__

#include <stdint.h>

void dns_answer_send(const uint8_t *packet);
void tcp_rst_send(const uint8_t* packet);

#endif
