#include <stdio.h>
#include <stdlib.h>

#include "defs.h"
#include "config.h"
#include "utils.h"
#include "logger.h"

void nfqueue_main(void);

int main(int argc, char* argv[])
{
	if (argc != 2) {
		die("usage: %s /path/to/config", argv[0]);
	}

	const char *config_path = argv[1];
	config_load(config_path);

	logger_init(NULL);
	logger_set_loglevel(LOGLEVEL_DEBUG);

	nfqueue_main();
}
