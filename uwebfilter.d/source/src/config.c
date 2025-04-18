#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <json-c/json.h>

#include "defs.h"
#include "utils.h"

#include "uthash.h"


/*  default values  */
static const char* configdefault_serveraddr = "https://localhost:3500/query";



typedef int category_t;
typedef int application_t;


typedef struct {
	UT_hash_handle hh;
	char domain[DOMAIN_LENGTH];
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
		domain_length > DOMAIN_LENGTH ? (DOMAIN_LENGTH - 1) : domain_length;
	strncpy(d->domain, domain, copy_length);

	return d;
}

static void __attribute__((unused))
domain_free(domain_t *d)
{
	free(d);
}



static struct {
	char serveraddr[SERVERADDR_LENGTH];

	size_t categories_length;
	category_t *categories;

	size_t applications_length;
	application_t *applications;

	domain_t *domains_blacklist;
	domain_t *domains_whitelist;

	struct {
		char auth[SERVERADDR_LENGTH];
		char addr[SERVERADDR_LENGTH];
		char user[USERNAME_LENGTH];
		char pass[PASSWORD_LENGTH];
	} cloudlogging;
} config = {
	.serveraddr = {0},

	.categories_length = 0,
	.categories = NULL,

	.applications_length = 0,
	.applications = NULL,

	.domains_blacklist = NULL,
	.domains_whitelist = NULL,

	.cloudlogging.auth = {0},
	.cloudlogging.addr = {0},
	.cloudlogging.user = {0},
	.cloudlogging.pass = {0}
};


const char*
config_get_serveraddr(void)
{
	return config.serveraddr;
}

const char*
config_get_cloudlogging_auth(void)
{
	return config.cloudlogging.auth;
}
const char*
config_get_cloudlogging_addr(void)
{
	return config.cloudlogging.addr;
}
const char*
config_get_cloudlogging_user(void)
{
	return config.cloudlogging.user;
}
const char*
config_get_cloudlogging_pass(void)
{
	return config.cloudlogging.pass;
}


static void config_load_serveraddr(json_object *j_serveraddr);
static void config_load_categories(json_object *j_categories);
static void config_load_applications(json_object *j_applications);
static void config_load_domains_blacklist(json_object *j_domains);
static void config_load_domains_whitelist(json_object *j_domains);
static void config_load_cloudlogging(json_object *j_cloudlogging);

void
config_load(const char* config_path)
{
	json_object *root = json_object_from_file(config_path);
	if (root == NULL) {
		die(
			"error reading config\n"
			"make sure it exists (%s) and isn't malformed",
			config_path
		);
	}

	config_load_serveraddr(
		json_object_object_get(root, "serveraddr")
	);

	config_load_categories(
		json_object_object_get(root, "categories")
	);

	config_load_applications(
		json_object_object_get(root, "applications")
	);

	config_load_domains_blacklist(
		json_object_object_get(root, "domains_blacklist")
	);

	config_load_domains_whitelist(
		json_object_object_get(root, "domains_whitelist")
	);

	config_load_cloudlogging(
		json_object_object_get(root, "cloudlogging")
	);

	json_object_put(root);
}

static void
config_load_serveraddr(json_object *j_serveraddr)
{
	const char* serveraddr = configdefault_serveraddr;

	if (j_serveraddr != NULL) {
		const char* addr = json_object_get_string(j_serveraddr);
		if (addr != NULL) {
			serveraddr = addr;
		}
	}

	const size_t serveraddr_length = strlen(serveraddr);
	const size_t copy_length =
		(serveraddr_length > SERVERADDR_LENGTH) ? (SERVERADDR_LENGTH - 1) : serveraddr_length;
	memcpy(config.serveraddr, serveraddr, copy_length);
}

static void
config_load_categories(json_object *j_categories)
{
	if (j_categories == NULL) {
		return;
	}

	int categories_length = json_object_array_length(j_categories);
	if (categories_length <= 0) {
		categories_length = 0;
	}

	category_t *categories = calloc(categories_length, sizeof(category_t));
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

		category_t category = json_object_get_int(j_category);
		categories[k++] = category;
	}

	config.categories_length = k;
	config.categories = categories;
}

static void
config_load_applications(json_object *j_applications)
{
	if (j_applications == NULL) {
		return;
	}

	int applications_length = json_object_array_length(j_applications);
	if (applications_length <= 0) {
		applications_length = 0;
	}

	application_t *applications = calloc(applications_length, sizeof(application_t));
	if (applications == NULL) {
		perror("calloc");
		return;
	}

	size_t k = 0;
	for (int i = 0; i < applications_length; i++) {
		json_object *j_application = json_object_array_get_idx(j_applications, i);
		if (j_application == NULL) {
			continue;
		}

		application_t application = json_object_get_int(j_application);
		applications[k++] = application;
	}

	config.applications_length = k;
	config.applications = applications;
}

static void
config_load_domains_blacklist(json_object *j_domains_blacklist)
{
	if (j_domains_blacklist == NULL) {
		return;
	}

	int domains_length = json_object_array_length(j_domains_blacklist);
	if (domains_length <= 0) {
		return;
	}

	for (int i = 0; i < domains_length; i++) {
		json_object *j_domain_blacklist = json_object_array_get_idx(j_domains_blacklist, i);
		if (j_domains_blacklist == NULL) {
			continue;
		}

		const char* domain_blacklist = json_object_get_string(j_domain_blacklist);
		domain_t *d = domain_create(domain_blacklist);
		if (d == NULL) {
			continue;
		}

		HASH_ADD_STR(config.domains_blacklist, domain, d);
	}
}

static void
config_load_domains_whitelist(json_object *j_domains_whitelist)
{
	if (j_domains_whitelist == NULL) {
		return;
	}

	int domains_length = json_object_array_length(j_domains_whitelist);
	if (domains_length <= 0) {
		return;
	}

	for (int i = 0; i < domains_length; i++) {
		json_object *j_domain_whitelist = json_object_array_get_idx(j_domains_whitelist, i);
		if (j_domain_whitelist == NULL) {
			continue;
		}

		const char* domain_whitelist = json_object_get_string(j_domain_whitelist);
		domain_t *d = domain_create(domain_whitelist);
		if (d == NULL) {
			continue;
		}

		HASH_ADD_STR(config.domains_whitelist, domain, d);
	}
}

static void
config_load_cloudlogging(json_object *j_cloudlogging)
{
	if (j_cloudlogging == NULL) {
		return;
	}

	json_object *j_auth = json_object_object_get(j_cloudlogging, "auth");
	if (j_auth != NULL) {
		const char* auth = json_object_get_string(j_auth);
		if (auth != NULL) {
			size_t length = strlen(auth);
			if (length > 255) {
				length = 255;
			}
			memcpy(config.cloudlogging.auth, auth, length);
		}
	}

	json_object *j_addr = json_object_object_get(j_cloudlogging, "addr");
	if (j_addr != NULL) {
		const char* addr = json_object_get_string(j_addr);
		if (addr != NULL) {
			size_t length = strlen(addr);
			if (length > 255) {
				length = 255;
			}
			memcpy(config.cloudlogging.addr, addr, length);
		}
	}

	json_object *j_user = json_object_object_get(j_cloudlogging, "user");
	if (j_user != NULL) {
		const char* user = json_object_get_string(j_user);
		if (user != NULL) {
			size_t length = strlen(user);
			if (length > 63) {
				length = 63;
			}
			memcpy(config.cloudlogging.user, user, length);
		}
	}

	json_object *j_pass = json_object_object_get(j_cloudlogging, "pass");
	if (j_pass != NULL) {
		const char* pass = json_object_get_string(j_pass);
		if (pass != NULL) {
			size_t length = strlen(pass);
			if (length > 63) {
				length = 63;
			}
			memcpy(config.cloudlogging.pass, pass, length);
		}
	}
}

void
config_print(void)
{
	printf("config\n");

	printf(">> serveraddr: %s\n", config.serveraddr);

	printf(">> categories:");
	for (size_t i = 0; i < config.categories_length; i++) {
		printf(" %d", config.categories[i]);
	}
	printf("\n");

	printf(">> applications:");
	for (size_t i = 0; i < config.applications_length; i++) {
		printf(" %d", config.applications[i]);
	}
	printf("\n");

	printf(">> domains_blacklist:");
	const domain_t *d, *tmp;
	HASH_ITER(hh, config.domains_blacklist, d, tmp) {
		printf(" '%s'", d->domain);
	}
	printf("\n");

	printf(">> domains_whitelist:");
	HASH_ITER(hh, config.domains_whitelist, d, tmp) {
		printf(" '%s'", d->domain);
	}
	printf("\n");

	printf(">> cloudlogging {\n");
	printf(">>   auth: %s\n", config.cloudlogging.auth);
	printf(">>   addr: %s\n", config.cloudlogging.addr);
	printf(">>   user: %s\n", config.cloudlogging.user);
	printf(">>   pass: %s\n", config.cloudlogging.pass);
	printf(">> }\n");
}



/*
 * is the category blocked by user
 */
bool
config_is_category_blocked(const int category)
{
	if (unlikely(config.categories == NULL)) {
		return false;
	}

	for (size_t i = 0; i < config.categories_length; i++) {
		if (category == config.categories[i]) {
			return true;
		}
	}

	return false;
}

/*
 * is the application blocked by user
 */
bool
config_is_application_blocked(const int appid)
{
	if (unlikely(config.applications == NULL)) {
		return false;
	}

	for (size_t i = 0; i < config.applications_length; i++) {
		if (appid == config.applications[i]) {
			return true;
		}
	}

	return false;
}



static bool
config_domain_lookup(domain_t *domains, const char domain[DOMAIN_LENGTH])
{
	const char *ptr = domain;
	for (;;) {
		domain_t *d = NULL;
		HASH_FIND_STR(domains, ptr, d);

		if (d != NULL) {    /*  domain exists  */
			return true;
		}

		ptr = strchr(ptr, '.');
		if (ptr == NULL) {
			return false;
		}
		ptr++;   /*  skip over '.'  */
	}

	return false;
}
/*
 * is the domain (or any of its subdomains) blacklisted by user
 */
bool
config_is_domain_blacklisted(const char domain[DOMAIN_LENGTH])
{
	return config_domain_lookup(config.domains_blacklist, domain);
}

/*
 * is the domain (or any of its subdomains) whitelisted by user
 */
bool
config_is_domain_whitelisted(const char domain[DOMAIN_LENGTH])
{
	return config_domain_lookup(config.domains_whitelist, domain);
}
