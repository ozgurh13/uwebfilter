#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <pthread.h>

#include <curl/curl.h>
#include <json-c/json.h>

#include "config.h"
#include "uthash.h"


typedef struct {
	UT_hash_handle hh;

	char domain[DOMAIN_LENGTH];
	int category;

	time_t expire;
} domain_category_t;

static struct {
	pthread_rwlock_t rwlock;
	domain_category_t *map;
	volatile time_t last_gc;
} domain_category_cache = {
	PTHREAD_RWLOCK_INITIALIZER,
	NULL,
	0
};


#define  MAX_DOMAIN_CACHE_SIZE             1024
#define  DOMAIN_CACHE_TTL                (60 * 5)    /*  5 minutes  */
#define  DOMAIN_CACHE_GC_INTERVAL        (60 * 1)    /*  1 minutes  */

static int lookup_category_for_domain_curl(char* domain);
int
lookup_category_of_domain(char domain[DOMAIN_LENGTH])
{
	if (is_domain_from_user(domain)) {
		return CUSTOM_DOMAIN;
	}

	int category = 0;
	time_t now = time(NULL);
	time_t last_gc;
	size_t domain_cache_size;
	bool found = false;

	domain_category_t *domain_category = NULL;

	pthread_rwlock_rdlock(&domain_category_cache.rwlock);
	{
		HASH_FIND_STR(domain_category_cache.map, domain, domain_category);
		domain_cache_size = HASH_COUNT(domain_category_cache.map);
		last_gc = domain_category_cache.last_gc;

		if (domain_category != NULL && domain_category->expire > now) {
			category = domain_category->category;
			found = true;
		}
	}
	pthread_rwlock_unlock(&domain_category_cache.rwlock);

	if (found) {
		goto out;
	}

	category = lookup_category_for_domain_curl(domain);

	/*  check if gc needs to run  */
	if ( domain_cache_size > MAX_DOMAIN_CACHE_SIZE
	  || last_gc < (now - DOMAIN_CACHE_GC_INTERVAL)
	   ) {
		/*  note: if two threads enter here at the same time, gc will run twice  */
		pthread_rwlock_wrlock(&domain_category_cache.rwlock);
		{
			domain_category_t *d, *tmp;
			HASH_ITER(hh, domain_category_cache.map, d, tmp) {
				/*  delete expired entries and a few random  */
				if (d->expire < now || (rand() % 10) < 3) {
					HASH_DEL(domain_category_cache.map, d);
					free(d);
				}
			}
			domain_category_cache.last_gc = now;
		}
		pthread_rwlock_unlock(&domain_category_cache.rwlock);
	}

	domain_category = calloc(1, sizeof(domain_category_t));
	if (domain_category == NULL) {
		perror("calloc");
		goto out;
	}

	memcpy(domain_category->domain, domain, DOMAIN_LENGTH);
	domain_category->category = category;
	domain_category->expire = now + DOMAIN_CACHE_TTL;

	domain_category_t *domain_category_old = NULL;
	pthread_rwlock_wrlock(&domain_category_cache.rwlock);
	{
		/*  REPLACE instead of ADD, in case of duplicate key  */
		HASH_REPLACE_STR(
			domain_category_cache.map,
			domain,
			domain_category,
			domain_category_old
		);
	}
	pthread_rwlock_unlock(&domain_category_cache.rwlock);
	free(domain_category_old);

out:
	return category;
}



typedef struct {
#define  CURL_BUFFER_SIZE   512
	char data[CURL_BUFFER_SIZE];
	size_t size;
} CurlR;

static bool lookup_domain_curl(CurlR *r, const char* domain);
static int
lookup_category_for_domain_curl(char* domain)
{
	int category = 0;

	CurlR r = {0};
	if (!lookup_domain_curl(&r, domain)) {
		puts("failed to lookup domain using CURL");
		return category;
	}

	json_object *root = json_tokener_parse(r.data);
	if (root == NULL) {
		puts("json_tokener_parse returned NULL");
		return category;
	}

	/*
	 * response JSON: { category : int }
	 */
	json_object *j_category = json_object_object_get(root, "category");
	if (j_category != NULL) {
		category = json_object_get_int(j_category);
	}

	json_object_put(root);

	return category;
}




#define  CURL_RECONNECT_INTERVAL   300
static CURL* curl_cloud_server_connect();
static CURL*
get_curl_connection()
{
	/*  make CURL and last_connect thread local  */
	static __thread CURL *curl = NULL;
	static __thread time_t last_connect = 0;

	time_t now = time(NULL);

	if (last_connect < (now - CURL_RECONNECT_INTERVAL)) {
		if (curl != NULL) {
			curl_easy_cleanup(curl);
		}
		curl = curl_cloud_server_connect();
		if (curl == NULL) {
			exit(EXIT_FAILURE);
		}
		last_connect = now;
	}

	return curl;
}



static json_object*
to_json_object(const char* domain)
{
	json_object *root = json_object_new_object();
	json_object_object_add(root, "domain", json_object_new_string(domain));
	return root;
}

static bool
lookup_domain_curl(CurlR *r, const char* domain)
{
	/*  if curl fails repeatedly, don't use it for a while  */
	static _Atomic size_t error_count = 0;
	static _Atomic time_t curl_use_allowed_time = 0;

#define  ERROR_THRESHOLD_COUNT   5
#define  CURL_WAIT_TIME         30   /*  seconds  */

	if (error_count > ERROR_THRESHOLD_COUNT) {
		time_t now = time(NULL);
		if (now < curl_use_allowed_time) {
			return false;
		}

		/*  allowed to use again  */
		error_count = 0;
	}

	CURL *curl = get_curl_connection();

	json_object *root = to_json_object(domain);
	const char *data = json_object_to_json_string(root);

	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, r);

	CURLcode res = curl_easy_perform(curl);
	json_object_put(root);
	if (res != CURLE_OK) {
		error_count++;
		fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
		if (error_count > ERROR_THRESHOLD_COUNT) {
			const time_t now = time(NULL);
			curl_use_allowed_time = now + CURL_WAIT_TIME;
		}
		return false;
	}

	error_count = 0;
	return true;
}




static size_t
curl_callback(void *ptr, size_t size, size_t nmemb, void *param)
{
	CurlR *r = param;

	size_t index = r->size;
	size_t n = (size * nmemb);

	size_t min(size_t s1, size_t s2) { return s1 < s2 ? s1 : s2; }

	size_t newsize = min(n, CURL_BUFFER_SIZE);
	size_t copysize = newsize - index;

	memcpy(r->data + index, ptr, copysize);

	r->size += copysize;

	return copysize;
}


static struct curl_slist *headers = NULL;
static CURL*
curl_cloud_server_connect()
{
	const char *serveraddr = "http://localhost:3500/query";

	CURL *curl = curl_easy_init();
	if (curl == NULL) {
		perror("curl_easy_init");
		return NULL;
	}

	curl_easy_setopt(curl, CURLOPT_URL, serveraddr);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, 102400L);
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
	curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 50L);
	curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, (long)CURL_HTTP_VERSION_2TLS);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
	curl_easy_setopt(curl, CURLOPT_FTP_SKIP_PASV_IP, 1L);
	curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_callback);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 1000L);

	return curl;
}


static void __attribute__((constructor))
init()
{
	srand(time(NULL));
	curl_global_init(CURL_GLOBAL_ALL);
	headers = curl_slist_append(headers, "content-type: application/json");
}
