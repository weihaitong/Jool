#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <syslog.h>
#include "log.h"
#include "common/types.h"
#include "common/xlat.h"
#include "usr/joold/modsocket.h"
#include "usr/joold/netsocket.h"

int xlat_type(void)
{
	return XT_NAT64;
}

static void cancel_thread(pthread_t thread)
{
	int error;

	error = pthread_cancel(thread);
	if (!error)
		pthread_join(thread, NULL);
	/*
	 * else:
	 * Well, `man 3 pthread_cancel` just `exit(EXIT_FAILURE)`s when
	 * `pthread_cancel()` fails.
	 * Let's instead be good citizens by closing the sockets anyway.
	 */
}

int main(int argc, char **argv)
{
	pthread_t mod2net_thread;
	pthread_t net2mod_thread;
	int error;

	openlog("joold", 0, LOG_DAEMON);

	error = netsocket_setup(argc, argv);
	if (error)
		goto end;
	error = modsocket_setup();
	if (error) {
		netsocket_teardown();
		goto end;
	}

	error = pthread_create(&mod2net_thread, NULL, modsocket_listen, NULL);
	if (error) {
		log_perror("Module-to-network thread initialization", error);
		goto clean;
	}
	error = pthread_create(&net2mod_thread, NULL, netsocket_listen, NULL);
	if (error) {
		log_perror("Network-to-module thread initialization", error);
		cancel_thread(mod2net_thread);
		goto clean;
	}

	pthread_join(net2mod_thread, NULL);
	pthread_join(mod2net_thread, NULL);
	/* Fall through. */

clean:
	modsocket_teardown();
	netsocket_teardown();
	/* Fall through. */

end:
	closelog();
	if (error)
		fprintf(stderr, "joold error: %d\n", error);
	return error;
}