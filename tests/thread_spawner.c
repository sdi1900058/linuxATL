// SPDX-License-Identifier: GPL-2.0-or-later
#define _GNU_SOURCE
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define NUM_THREADS 4
#define SLEEP_INTERVAL_US 50000

static volatile sig_atomic_t keep_running = 1;

static void handle_signal(int sig)
{
	(void)sig;
	keep_running = 0;
}

static void *worker(void *arg)
{
	(void)arg;

	while (keep_running) {
		usleep(SLEEP_INTERVAL_US);
	}

	return NULL;
}

int main(void)
{
	pthread_t threads[NUM_THREADS];
	int ret;
	int i;

	if (signal(SIGTERM, handle_signal) == SIG_ERR ||
	    signal(SIGINT, handle_signal) == SIG_ERR) {
		perror("signal");
		return EXIT_FAILURE;
	}

	printf("thread_spawner parent PID: %d\n", getpid());
	fflush(stdout);

	for (i = 0; i < NUM_THREADS; i++) {
		ret = pthread_create(&threads[i], NULL, worker, NULL);
		if (ret != 0) {
			fprintf(stderr, "pthread_create failed: %d\n", ret);
			keep_running = 0;
			break;
		}
	}

	while (keep_running)
		usleep(SLEEP_INTERVAL_US);

	for (i = 0; i < NUM_THREADS; i++) {
		if (threads[i])
			pthread_join(threads[i], NULL);
	}

	return EXIT_SUCCESS;
}

