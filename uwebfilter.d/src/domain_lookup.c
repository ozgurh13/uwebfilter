#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#include <json-c/json.h>
#include <curl/curl.h>

#include "config.h"
#include "logger.h"
#include "domain_lookup.h"
#include "uthash.h"


static void json_object_put1(json_object **obj) { json_object_put(*obj); }
static void
domain_request_to_json_object(json_object *root, const domain_request_t *domain_request)
{
	json_object_object_add(root, "domain", json_object_new_string(domain_request->domain));
}


typedef struct {
	json_tokener *tok;
	json_object *obj;
} curl_json_buffer_t;

static void
curl_json_buffer_cleanup(curl_json_buffer_t *b)
{
	if (b->obj != NULL) {
		json_object_put(b->obj);
	}
	json_tokener_free(b->tok);
}



static size_t
curl_write_cb(void *contents, size_t size, size_t nmemb, void *userp)
{
	const size_t realsize = size * nmemb;
	curl_json_buffer_t *b = userp;
	b->obj = json_tokener_parse_ex(b->tok, contents, realsize);

	return realsize;
}

static bool
domain_lookup_curl(curl_json_buffer_t *buffer, const char* data)
{
	CURLcode ret;
	CURL *hnd;
	struct curl_slist *slist1;

	slist1 = NULL;
	slist1 = curl_slist_append(slist1, "content-type: application/json");

	hnd = curl_easy_init();

	const char* url = config_get_serveraddr();
	curl_easy_setopt(hnd, CURLOPT_URL, url);
	curl_easy_setopt(hnd, CURLOPT_HTTP_VERSION, (long)CURL_HTTP_VERSION_2TLS);
	curl_easy_setopt(hnd, CURLOPT_CUSTOMREQUEST, "POST");
	curl_easy_setopt(hnd, CURLOPT_BUFFERSIZE, 102400L);
	curl_easy_setopt(hnd, CURLOPT_TIMEOUT_MS, 10000L);
	curl_easy_setopt(hnd, CURLOPT_USERAGENT, "curl/8.10.1");
	curl_easy_setopt(hnd, CURLOPT_HTTPHEADER, slist1);

	curl_easy_setopt(hnd, CURLOPT_WRITEFUNCTION, curl_write_cb);
	curl_easy_setopt(hnd, CURLOPT_POSTFIELDS, data);
	curl_easy_setopt(hnd, CURLOPT_WRITEDATA, buffer);

	curl_easy_setopt(hnd, CURLOPT_NOPROGRESS, 1L);
	curl_easy_setopt(hnd, CURLOPT_MAXREDIRS, 50L);
	curl_easy_setopt(hnd, CURLOPT_TCP_KEEPALIVE, 1L);

	ret = curl_easy_perform(hnd);

	curl_easy_cleanup(hnd);
	hnd = NULL;
	curl_slist_free_all(slist1);
	slist1 = NULL;

	return ret == CURLE_OK;
}




typedef struct {
	UT_hash_handle hh;

	char domain[DOMAIN_LENGTH];

	struct {
		int application;
		int category;
	};

	time_t expire;
} domain_info_t;

static domain_info_t *domain_info_allocate(void);
static void domain_info_deallocate(domain_info_t *domain_info);

static domain_info_t*
domain_info_create(
	const char* domain,
	int application,
	int category,
	time_t expire
)
{
	domain_info_t *domain_info = domain_info_allocate();
	if (domain_info == NULL) {
		return NULL;
	}

	memcpy(domain_info->domain, domain, strlen(domain));
	domain_info->category = category;
	domain_info->application = application;
	domain_info->expire = expire;

	return domain_info;
}


static void
domain_info_free(domain_info_t *domain_info)
{
	domain_info_deallocate(domain_info);
}


/*  domain_info memory arena  */
#define   DOMAIN_INFO_ARENA_SIZE      65536
static domain_info_t _domain_info_arena[DOMAIN_INFO_ARENA_SIZE] = {0};
static domain_info_t* domain_info_arena_ptr[DOMAIN_INFO_ARENA_SIZE] = {0};
static int domain_info_arena_index = 0;

static void __attribute__((constructor))
domain_info_arena_init(void)
{
	for (size_t i = 0; i < DOMAIN_INFO_ARENA_SIZE; i++) {
		domain_info_arena_ptr[i] = &_domain_info_arena[i];
	}
}

static domain_info_t*
domain_info_allocate(void)
{
	assert(domain_info_arena_index >= 0);

	if (domain_info_arena_index >= DOMAIN_INFO_ARENA_SIZE) {
		return NULL;
	}

	domain_info_t *domain_info = domain_info_arena_ptr[domain_info_arena_index];
	domain_info_arena_index++;

	memset(domain_info, 0, sizeof(domain_info_t));

	return domain_info;
}

static void
domain_info_deallocate(domain_info_t *domain_info)
{
	assert(domain_info != NULL);
	assert(domain_info_arena_index > 0);

	domain_info_arena_index--;
	domain_info_arena_ptr[domain_info_arena_index] = domain_info;
}



#define    DOMAIN_INFO_CACHE_DEFAULT_TTL  (1 * 60)       /*  1 minute   */
#define    DOMAIN_INFO_CACHE_GCINTERVAL   (5 * 60)       /*  5 minutes  */

typedef struct {
	size_t size;
	domain_info_t *cache;
	time_t lastgc;
} domain_info_cache_t;
#define    DOMAIN_INFO_CACHE_INITIALIZER         \
        { .size = 0, .cache = NULL, .lastgc = 0 }



static domain_info_t*
domain_info_cache_lookup(
	domain_info_cache_t *domain_info_cache,
	const char* domain
)
{
	domain_info_t *domain_info = NULL;

	const char *ptr = domain;
	for (;;) {
		HASH_FIND_STR(domain_info_cache->cache, ptr, domain_info);
		if (domain_info != NULL) {
			return domain_info;
		}

		ptr = strchr(ptr, '.');
		if (ptr == NULL) {
			return NULL;
		}
		ptr++;   /*  skip over '.'  */
	}

	return NULL;
}

static void
domain_info_cache_delete(
	domain_info_cache_t *domain_info_cache,
	domain_info_t *domain_info
)
{
	HASH_DEL(domain_info_cache->cache, domain_info);
}

static void
domain_info_cache_insert(
	domain_info_cache_t *domain_info_cache,
	domain_info_t *domain_info
)
{
	time_t now = time(NULL);

	if (domain_info_cache->lastgc < (now - DOMAIN_INFO_CACHE_GCINTERVAL)) {
		/*  run gc  */
		logger_debug("[gc] start");
		const size_t before = HASH_COUNT(domain_info_cache->cache);
		domain_info_t *d, *tmp;
		HASH_ITER(hh, domain_info_cache->cache, d, tmp) {
			if (d->expire < now || rand() < 0.2) {
				logger_debug("[gc] %s", d->domain);
				domain_info_cache_delete(domain_info_cache, d);
				domain_info_free(d);
			}
		}
		domain_info_cache->lastgc = now;
		const size_t after = HASH_COUNT(domain_info_cache->cache);

		logger_debug("[gc] end; %zu domains gc'd", before - after);
	}

	HASH_ADD_STR(domain_info_cache->cache, domain, domain_info);
}


void
domain_lookup(
	domain_response_t *domain_response,
	const domain_request_t *domain_request
)
{
	static domain_info_cache_t domain_info_cache =
		DOMAIN_INFO_CACHE_INITIALIZER;

	const time_t now = time(NULL);

	domain_info_t *domain_info = domain_info_cache_lookup(
		&domain_info_cache, domain_request->domain
	);
	if (domain_info != NULL) {
		if (domain_info->expire > now) {
			domain_response->category = domain_info->category;
			domain_response->application = domain_info->application;
			return;
		}

		domain_info_cache_delete(&domain_info_cache, domain_info);
		domain_info_free(domain_info);
		domain_info = NULL;
	}

	__attribute__((cleanup(json_object_put1)))
	json_object *root = json_object_new_object();

	domain_request_to_json_object(root, domain_request);
	const char* domain_request_payload =
		json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN);


	__attribute__((cleanup(curl_json_buffer_cleanup)))
	curl_json_buffer_t b = {
		.tok = json_tokener_new(),
		.obj = NULL
	};

	if (!domain_lookup_curl(&b, domain_request_payload) || b.obj == NULL) {
		return;
	}



 	json_object *obj = b.obj;


	int category = 0;
	int application = 0;
	size_t ttl = 0;

	json_object *j_category = json_object_object_get(obj, "category");
	if (j_category != NULL) {
		category = json_object_get_int(j_category);
	}

	json_object *j_application = json_object_object_get(obj, "application");
	if (j_category != NULL) {
		application = json_object_get_int(j_application);
	}

	json_object *j_ttl = json_object_object_get(obj, "ttl");
	if (j_ttl != NULL) {
		ttl = json_object_get_int64(j_ttl);
		if (ttl == 0) {
			ttl = DOMAIN_INFO_CACHE_DEFAULT_TTL;
		}
	}

	json_object *j_cache_domains = json_object_object_get(obj, "cache_domains");
	if (j_cache_domains != NULL) {
		/*  insert these domains instead of the domain we queried  */
		int length = json_object_array_length(j_cache_domains);

		for (int i = 0; i < length; i++) {
			json_object *j_domain = json_object_array_get_idx(j_cache_domains, i);
			if (j_domain == NULL) {
				continue;
			}

			const char* domain = json_object_get_string(j_domain);
			if (domain == NULL) {
				continue;
			}

			domain_info = domain_info_create(
				domain,
				application,
				category,
				now + ttl
			);
			if (domain_info == NULL) {
				continue;
			}

			domain_info_cache_insert(&domain_info_cache, domain_info);
		}
	}
	else {    /*  cache domain we queried  */
		domain_info = domain_info_create(
			domain_request->domain,
			application,
			category,
			now + ttl
		);
		if (domain_info == NULL) {
			return;
		}

		domain_info_cache_insert(&domain_info_cache, domain_info);
	}
}
