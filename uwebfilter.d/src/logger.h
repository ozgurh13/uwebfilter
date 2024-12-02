#ifndef __LOGGER_H__
#define __LOGGER_H__

typedef enum {
	LOGLEVEL_DEBUG,
	LOGLEVEL_INFO,
	LOGLEVEL_WARNING,
	LOGLEVEL_ERROR,
	LOGLEVEL_CRITICAL
} logger_loglevel_t;

void logger_init(const char* filename);
void logger_close(void);

void logger_set_loglevel(const logger_loglevel_t loglevel);

void logger_debug(const char* restrict format, ...);
void logger_info(const char* restrict format, ...);
void logger_warning(const char* restrict format, ...);
void logger_error(const char* restrict format, ...);
void logger_critical(const char* restrict format, ...);

#endif
