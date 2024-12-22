#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include <time.h>

#include "logger.h"


#define   LOGGER_STREAM_DEFAULT         stderr

static struct {
	bool active;
	logger_loglevel_t loglevel;
	FILE *stream;
} logger = {
	.active = false,
	.loglevel = LOGLEVEL_INFO
};


void
logger_init(const char* filename)
{
	if (filename == NULL) {
		logger.stream = LOGGER_STREAM_DEFAULT;
	}
	else {
		logger.stream = fopen(filename, "a");
	}

	logger.active = true;
}

void
logger_close(void)
{
	if (logger.stream != LOGGER_STREAM_DEFAULT) {
		fclose(logger.stream);
	}
	logger.active = false;
}

void
logger_set_loglevel(const logger_loglevel_t loglevel)
{
	logger.loglevel = loglevel;
}


static void
logger_write(
	const logger_loglevel_t loglevel,
	const char* logfmt,
	const char* format,
	va_list args
)
{
	if (logger.active && loglevel >= logger.loglevel) {
		time_t now = time(NULL);
		char timefmt[20] = {0};
		struct tm *tm = localtime(&now);
		strftime(timefmt, 20, "%Y.%m.%d %H:%M:%S", tm);

		/*  ``YYYY.MM.DD HH:MM:SS [LOGLEVEL] | MESSAGE``  */
		fprintf(logger.stream, "%s", timefmt);
		fprintf(logger.stream, "%s", logfmt);
		vfprintf(logger.stream, format, args);
		fprintf(logger.stream, "\n");

		fflush(logger.stream);
	}
}


static const char* logfmt_DEBUG    = " [DEBUG]    | ";
static const char* logfmt_INFO     = " [INFO]     | ";
static const char* logfmt_WARNING  = " [WARNING]  | ";
static const char* logfmt_ERROR    = " [ERROR]    | ";
static const char* logfmt_CRITICAL = " [CRITICAL] | ";


#define    MAKE_LOGGER(name, loglevel, logfmt)                      \
void                                                                \
logger_##name(const char* restrict format, ...)                     \
{                                                                   \
        va_list args;                                               \
        va_start(args, format);                                     \
        logger_write(loglevel, logfmt, format, args);               \
        va_end(args);                                               \
}


MAKE_LOGGER(debug,    LOGLEVEL_DEBUG,    logfmt_DEBUG)
MAKE_LOGGER(info,     LOGLEVEL_INFO,     logfmt_INFO)
MAKE_LOGGER(warning,  LOGLEVEL_WARNING,  logfmt_WARNING)
MAKE_LOGGER(error,    LOGLEVEL_ERROR,    logfmt_ERROR)
MAKE_LOGGER(critical, LOGLEVEL_CRITICAL, logfmt_CRITICAL)
