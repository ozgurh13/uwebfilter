#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <stdbool.h>

#include "defs.h"


void config_load(const char* config_path);
void config_print(void);

const char* config_get_serveraddr(void);
const char* config_get_cloudlogging_auth(void);
const char* config_get_cloudlogging_addr(void);
const char* config_get_cloudlogging_user(void);
const char* config_get_cloudlogging_pass(void);


/*  is the category blocked by user  */
bool config_is_category_blocked(const int category);
/*  is the application blocked by user  */
bool config_is_application_blocked(const int appid);
/*  is the domain (or any of its subdomains) blacklisted by user  */
bool config_is_domain_blacklisted(const char domain[DOMAIN_LENGTH]);
/*  is the domain (or any of its subdomains) whitelisted by user  */
bool config_is_domain_whitelisted(const char domain[DOMAIN_LENGTH]);

#endif
