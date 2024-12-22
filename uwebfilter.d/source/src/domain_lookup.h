#ifndef __DOMAIN_LOOKUP_H__
#define __DOMAIN_LOOKUP_H__

typedef struct {
	const char* domain;
} domain_request_t;

typedef struct {
	int category;
	int application;
} domain_response_t;

void
domain_lookup(
	domain_response_t *domain_response,
	const domain_request_t *domain_request
);

#endif
