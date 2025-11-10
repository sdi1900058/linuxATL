// SPDX-License-Identifier: GPL-2.0-or-later
#define _GNU_SOURCE
#include <errno.h>
#include <getopt.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <linux/k22info.h>

#define K22TREE_SYSCALL 467

struct stress_args {
	int thread_id;
	int iterations;
	int num_entries;
	bool quiet;
};

static atomic_ulong success_calls = 0;
static atomic_ulong error_calls = 0;

static void *stress_worker(void *data)
{
	const struct stress_args *args = data;
	struct k22info *buf = NULL;
	size_t buf_size = (size_t)args->num_entries * sizeof(*buf);

	if (args->num_entries > 0) {
		buf = malloc(buf_size);
		if (!buf) {
			perror("malloc");
			return NULL;
		}
	}

	for (int iter = 0; iter < args->iterations; iter++) {
		int ne_value = args->num_entries;
		errno = 0;
		long ret = syscall(K22TREE_SYSCALL, buf, &ne_value);
		int call_errno = errno;

		if (ret == -1) {
			atomic_fetch_add_explicit(&error_calls, 1, memory_order_relaxed);
			if (!args->quiet) {
				fprintf(stderr,
					"[thread %d] iteration %d: error %d (%s)\n",
					args->thread_id, iter + 1, call_errno,
					strerror(call_errno));
			}
		} else {
			atomic_fetch_add_explicit(&success_calls, 1, memory_order_relaxed);
			if (!args->quiet) {
				printf("[thread %d] iteration %d: ret=%ld, ne=%d\n",
				       args->thread_id, iter + 1, ret, ne_value);
			}
		}
	}

	free(buf);
	return NULL;
}

static void usage(const char *prog)
{
	fprintf(stderr,
		"Usage: %s [options]\n"
		"\n"
		"Options:\n"
		"  -t, --threads N       Number of threads (default: 8)\n"
		"  -i, --iterations N    Syscalls per thread (default: 100)\n"
		"  -n, --num-entries N   Buffer size (default: 1024)\n"
		"      --quiet           Suppress per-call logging\n"
		"  -h, --help            Show this help message\n",
		prog);
}

int main(int argc, char *argv[])
{
	static const struct option long_opts[] = {
		{ "threads", required_argument, NULL, 't' },
		{ "iterations", required_argument, NULL, 'i' },
		{ "num-entries", required_argument, NULL, 'n' },
		{ "quiet", no_argument, NULL, 1 },
		{ "help", no_argument, NULL, 'h' },
		{ 0, 0, 0, 0 }
	};

	int opt;
	int long_index = 0;
	int threads = 8;
	int iterations = 100;
	int num_entries = 1024;
	bool quiet = false;

	while ((opt = getopt_long(argc, argv, "t:i:n:h", long_opts, &long_index)) != -1) {
		switch (opt) {
		case 't':
			threads = atoi(optarg);
			if (threads < 1)
				threads = 1;
			break;
		case 'i':
			iterations = atoi(optarg);
			if (iterations < 1)
				iterations = 1;
			break;
		case 'n':
			num_entries = atoi(optarg);
			break;
		case 'h':
			usage(argv[0]);
			return EXIT_SUCCESS;
		case 1:
			quiet = true;
			break;
		default:
			usage(argv[0]);
			return EXIT_FAILURE;
		}
	}

	printf("k22tree stress configuration:\n");
	printf("  threads    : %d\n", threads);
	printf("  iterations : %d per thread\n", iterations);
	printf("  num_entries: %d\n", num_entries);
	printf("  quiet mode : %s\n", quiet ? "yes" : "no");

	pthread_t *thread_ids = calloc(threads, sizeof(*thread_ids));
	struct stress_args *args = calloc(threads, sizeof(*args));
	if (!thread_ids || !args) {
		perror("calloc");
		free(thread_ids);
		free(args);
		return EXIT_FAILURE;
	}

	for (int i = 0; i < threads; i++) {
		args[i].thread_id = i;
		args[i].iterations = iterations;
		args[i].num_entries = num_entries;
		args[i].quiet = quiet;

		int ret = pthread_create(&thread_ids[i], NULL, stress_worker, &args[i]);
		if (ret != 0) {
			fprintf(stderr, "pthread_create failed (%d): %s\n", ret, strerror(ret));
			threads = i;
			break;
		}
	}

	for (int i = 0; i < threads; i++) {
		if (thread_ids[i])
			pthread_join(thread_ids[i], NULL);
	}

	unsigned long success = atomic_load_explicit(&success_calls, memory_order_relaxed);
	unsigned long errors = atomic_load_explicit(&error_calls, memory_order_relaxed);

	printf("Stress summary:\n");
	printf("  successful calls: %lu\n", success);
	printf("  failed calls     : %lu\n", errors);

	free(thread_ids);
	free(args);
	return (errors == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

