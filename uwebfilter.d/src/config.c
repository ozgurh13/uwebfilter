#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>

#include <sys/inotify.h>

#include <json-c/json.h>

#include "defs.h"
#include "uthash.h"

static const char* cfg_path = "/home/.../.config/uwebfilterd/config";
static const char* serveraddr_default = "https://localhost:3500/query";

static pthread_rwlock_t serveraddr_rwlock = PTHREAD_RWLOCK_INITIALIZER;
static char serveraddr[SERVERADDR_LENGTH + 1] = {0};
static void
config__serveraddr(json_object *j_serveraddr)
{
	memset(serveraddr, 0, SERVERADDR_LENGTH);

	const char* addr = serveraddr_default;
	if (j_serveraddr != NULL) {
		addr = json_object_get_string(j_serveraddr);
	}

	const size_t addr_length = strlen(addr);
	const size_t copy_length =
		(addr_length < SERVERADDR_LENGTH) ? addr_length : SERVERADDR_LENGTH;
	memcpy(serveraddr, addr, copy_length);
}

void
config_get_serveraddr(char addr[SERVERADDR_LENGTH + 1])
{
	pthread_rwlock_rdlock(&serveraddr_rwlock);
	{
		memcpy(addr, serveraddr, SERVERADDR_LENGTH);
	}
	pthread_rwlock_unlock(&serveraddr_rwlock);
}



static pthread_rwlock_t categories_rwlock = PTHREAD_RWLOCK_INITIALIZER;
static int *categories = NULL;

static void
config__categories(json_object *j_categories)
{
	free(categories);
	categories = NULL;

	if (j_categories == NULL) {
		/*  if this returns NULL, there isn't much we can do  */
		categories = calloc(1, sizeof(int));
		return;
	}

	int categories_length = json_object_array_length(j_categories);
	if (categories_length <= 0) {
		categories_length = 0;
	}

	/*  +1 for a 0 at the end  */
	categories = calloc(categories_length + 1, sizeof(int));
	if (categories == NULL) {
		perror("calloc");
		return;
	}

	size_t k = 0;
	for (int i = 0; i < categories_length; i++) {
		json_object *j_category = json_object_array_get_idx(j_categories, i);
		if (j_category == NULL) {
			continue;
		}

		int category = json_object_get_int(j_category);
		categories[k++] = category;
	}
}



typedef struct {
	UT_hash_handle hh;
	char domain[DOMAIN_LENGTH + 1];
} domain_t;

static domain_t*
domain_create(const char* domain)
{
	domain_t *d = calloc(1, sizeof(domain_t));
	if (d == NULL) {
		perror("calloc");
		return NULL;
	}

	const size_t domain_length = strlen(domain);
	const size_t copy_length =
		(domain_length < DOMAIN_LENGTH) ? domain_length : DOMAIN_LENGTH;
	memcpy(d->domain, domain, copy_length);

	return d;
}

static void
domain_free(domain_t *d)
{
	free(d);
}

static pthread_rwlock_t domains_rwlock = PTHREAD_RWLOCK_INITIALIZER;
static domain_t *domains = NULL;

static void
config__domains(json_object *j_domains)
{
	domain_t *d, *tmp;
	HASH_ITER(hh, domains, d, tmp) {
		HASH_DEL(domains, d);
		domain_free(d);
	}
	domains = NULL;

	if (j_domains == NULL) {
		return;
	}

	int domains_length = json_object_array_length(j_domains);
	if (domains_length <= 0) {
		return;
	}

	for (int i = 0; i < domains_length; i++) {
		json_object *j_domain = json_object_array_get_idx(j_domains, i);
		if (j_domain == NULL) {
			continue;
		}

		const char* domain = json_object_get_string(j_domain);
		d = domain_create(domain);
		if (d == NULL) {
			continue;
		}

		HASH_ADD_STR(domains, domain, d);
	}
}

static void
config_load()
{
	json_object *root = json_object_from_file(cfg_path);
	if (root == NULL) {
		fputs("error reading config\n", stderr);
		exit(EXIT_FAILURE);
	}

	json_object *j_serveraddr = json_object_object_get(root, "serveraddr");
	pthread_rwlock_wrlock(&serveraddr_rwlock);
	{
		config__serveraddr(j_serveraddr);
	}
	pthread_rwlock_unlock(&serveraddr_rwlock);

	json_object *j_categories = json_object_object_get(root, "categories");
	pthread_rwlock_wrlock(&categories_rwlock);
	{
		config__categories(j_categories);
	}
	pthread_rwlock_unlock(&categories_rwlock);

	json_object *j_domains = json_object_object_get(root, "domains");
	pthread_rwlock_wrlock(&domains_rwlock);
	{
		config__domains(j_domains);
	}
	pthread_rwlock_unlock(&domains_rwlock);

	json_object_put(root);
}


void*
config_watch(void *)
{
	config_load();

	int fd = inotify_init();
	if (fd < 0) {
		perror("inotify_init");
		exit(EXIT_FAILURE);
	}

	for (;;) {
		inotify_add_watch(fd, cfg_path, IN_MODIFY | IN_ONESHOT);

#define EVENT_SIZE  sizeof(struct inotify_event)
		char buffer[EVENT_SIZE] = {0};
		int length = read(fd, buffer, EVENT_SIZE);
		if (length < 0) {
			perror("read");
			exit(EXIT_FAILURE);
		}

		struct inotify_event *event = (struct inotify_event *) buffer;
		if (!!(event->mask & IN_MODIFY) || !!(event->mask & IN_IGNORED)) {
			config_load();
		}

		inotify_rm_watch(fd, event->wd);
	}

	close(fd);
}


bool
is_access_to_category_allowed(int category)
{
	if (category == CUSTOM_DOMAIN) {
		return false;
	}

	bool allowed = true;

	pthread_rwlock_rdlock(&categories_rwlock);
	{
		for (int *p = categories; *p != 0; p++) {
			if (*p == category) {
				allowed = false;
				break;
			}
		}
	}
	pthread_rwlock_unlock(&categories_rwlock);

	return allowed;
}

bool
is_domain_from_user(char domain[DOMAIN_LENGTH])
{
	bool is_custom = false;

	pthread_rwlock_rdlock(&domains_rwlock);
	{
		char *ptr = domain;
		for (;;) {
			domain_t *d = NULL;
			HASH_FIND_STR(domains, ptr, d);

			if (d != NULL) {   /*  found a domain we know of  */
				is_custom = true;
				break;
			}

			ptr = strchr(ptr, '.');
			if (ptr == NULL) {
				break;
			}
			ptr++;   /*  skip over '.'  */
		}
	}
	pthread_rwlock_unlock(&domains_rwlock);

	return is_custom;
}
