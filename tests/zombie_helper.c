#define _GNU_SOURCE
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <unistd.h>

static volatile sig_atomic_t keep_running = 1;

static void handle_signal(int sig)
{
	(void)sig;
	keep_running = 0;
}

int main(void)
{
	if (signal(SIGTERM, handle_signal) == SIG_ERR ||
	    signal(SIGINT, handle_signal) == SIG_ERR) {
		perror("signal");
		return EXIT_FAILURE;
	}

	pid_t child = fork();
	if (child < 0) {
		perror("fork");
		return EXIT_FAILURE;
	}

	if (child == 0) {
		if (prctl(PR_SET_NAME, "k22_zombie") == -1)
			perror("prctl");
		_exit(0);
	}

	printf("parent_pid=%d child_pid=%d\n", getpid(), child);
	fflush(stdout);

	while (keep_running)
		sleep(1);

	/* Do not wait() on child so it remains a zombie until parent exits. */
	return EXIT_SUCCESS;
}

