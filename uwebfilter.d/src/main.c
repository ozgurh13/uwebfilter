#include <stdbool.h>
#include <string.h>
#include <pthread.h>

#include "utils.h"

static pthread_t config_tid;
void* config_watch(void *);

void* tcp443_main(void *);
static pthread_t tcp443_tid;

void* tcp80_main(void *);
static pthread_t tcp80_tid;

void* udp53_main(void *);
static pthread_t udp53_tid;

int main(int argc, char* argv[])
{
	if (argc > 1) {
		if (strcmp(argv[1], "-d") == 0) {
			debug_set(true);
		}
	}

	/*  config watcher  */
	pthread_create(&config_tid, NULL, config_watch, NULL);
	pthread_detach(config_tid);

	/*  HTTPS: TCP dport 443  */
	pthread_create(&tcp443_tid, NULL, tcp443_main, NULL);
	pthread_detach(tcp443_tid);

	/*  HTTP: UDP dport 53  */
	pthread_create(&udp53_tid, NULL, udp53_main, NULL);
	pthread_detach(udp53_tid);

	/*  HTTP: TCP dport 80  */
	pthread_create(&tcp80_tid, NULL, tcp80_main, NULL);
	pthread_detach(tcp80_tid);

	pthread_exit(NULL);
}
