// SPDX-License-Identifier: GPL-2.0-or-later
#define _GNU_SOURCE
#include <errno.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <linux/k22info.h>

#define K22TREE_SYSCALL 467

static void usage(const char *prog)
{
	fprintf(stderr,
		"Usage: %s [options]\n"
		"\n"
		"Options:\n"
		"  -n, --num-entries N   Set initial value for *ne (default: 100)\n"
		"  -b, --buffer-size N   Allocate buffer with N entries (default: 100)\n"
		"  -i, --iterations N    Repeat syscall N times (default: 1)\n"
		"      --null-buf        Pass NULL for buf\n"
		"      --null-ne         Pass NULL for ne pointer\n"
		"      --quiet           Only print final status per call\n"
		"  -h, --help            Show this help\n"
		"\n"
		"Examples:\n"
		"  %s -n -1 --null-buf\n"
		"  %s -n 1000 -b 10 -i 5\n",
		prog, prog, prog);
}

int main(int argc, char *argv[])
{
	static const struct option long_opts[] = {
		{ "num-entries", required_argument, NULL, 'n' },
		{ "buffer-size", required_argument, NULL, 'b' },
		{ "iterations", required_argument, NULL, 'i' },
		{ "null-buf", no_argument, NULL, 1 },
		{ "null-ne", no_argument, NULL, 2 },
		{ "quiet", no_argument, NULL, 3 },
		{ "help", no_argument, NULL, 'h' },
		{ 0, 0, 0, 0 }
	};

	int opt;
	int long_index = 0;
	int initial_ne = 100;
	int buffer_entries = 100;
	int iterations = 1;
	bool null_buf = false;
	bool null_ne = false;
	bool quiet = false;

	while ((opt = getopt_long(argc, argv, "n:b:i:h", long_opts, &long_index)) != -1) {
		switch (opt) {
		case 'n':
			initial_ne = atoi(optarg);
			break;
		case 'b':
			buffer_entries = atoi(optarg);
			break;
		case 'i':
			iterations = atoi(optarg);
			if (iterations < 1)
				iterations = 1;
			break;
		case 'h':
			usage(argv[0]);
			return EXIT_SUCCESS;
		case 1:
			null_buf = true;
			break;
		case 2:
			null_ne = true;
			break;
		case 3:
			quiet = true;
			break;
		default:
			usage(argv[0]);
			return EXIT_FAILURE;
		}
	}

	if (!quiet) {
		printf("k22syscall_harness configuration:\n");
		printf("  initial *ne      : %d\n", initial_ne);
		printf("  buffer entries   : %d%s\n", buffer_entries,
		       null_buf ? " (ignored, --null-buf)" : "");
		printf("  iterations       : %d\n", iterations);
		printf("  buf pointer      : %s\n", null_buf ? "NULL" : "allocated");
		printf("  ne pointer       : %s\n", null_ne ? "NULL" : "provided");
	}

	struct k22info *buf = NULL;
	size_t buf_size_bytes = 0;

	if (!null_buf && buffer_entries > 0) {
		buf_size_bytes = (size_t)buffer_entries * sizeof(*buf);
		buf = malloc(buf_size_bytes);
		if (!buf) {
			perror("malloc");
			return EXIT_FAILURE;
		}
	}

	for (int iter = 0; iter < iterations; iter++) {
		int ne_value = initial_ne;
		int *ne_ptr = null_ne ? NULL : &ne_value;

		errno = 0;
		long ret = syscall(K22TREE_SYSCALL, buf, ne_ptr);
		int call_errno = errno;

		if (!quiet) {
			printf("[iteration %d] syscall returned %ld", iter + 1, ret);
			if (ret == -1)
				printf(" (errno=%d: %s)", call_errno, strerror(call_errno));
			printf("\n");

			if (!null_ne)
				printf("[iteration %d] updated *ne = %d\n", iter + 1, ne_value);
		} else {
			if (ret == -1)
				printf("iteration %d: error %d (%s)\n", iter + 1, call_errno,
				       strerror(call_errno));
			else
				printf("iteration %d: ret=%ld, ne=%d\n", iter + 1, ret,
				       null_ne ? -1 : ne_value);
		}
	}

	free(buf);
	return EXIT_SUCCESS;
}

