#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <setjmp.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>
#include <unistd.h>
#include "testing.h"

// TODO maybe share some of this code with testing.c

struct simple_test {
	bool (*f)(void);
};

struct range_test {
	uint64_t start;
	uint64_t end;
	bool (*f)(uint64_t start, uint64_t end);
};

struct random_test {
	uint64_t num_values;
	bool (*f)(uint64_t num_values, uint64_t seed);
};

struct test {
	enum {
		TEST_TYPE_SIMPLE,
		TEST_TYPE_RANGE,
		TEST_TYPE_RANDOM,
	} type;
	bool enabled;
	bool should_succeed;
	const char *file;
	const char *name;
	union {
		struct simple_test simple_test;
		struct range_test range_test;
		struct random_test random_test;
	};
};

static struct test *tests;
static size_t num_tests;
static size_t tests_capacity;

static void add_test(struct test test)
{
	if (tests_capacity == num_tests) {
		tests_capacity *= 2;
		if (tests_capacity < 8) {
			tests_capacity = 8;
		}
		tests = realloc(tests, tests_capacity * sizeof(tests[0]));
		if (!tests) {
			perror("realloc failed");
			exit(EXIT_FAILURE);
		}
	}
	const char *file = strrchr(test.file, '/');
	if (file) {
		test.file = file + 1;
	}
	tests[num_tests++] = test;
}

void register_simple_test(const char *file, const char *name,
			  bool (*f)(void), bool should_succeed)
{
	add_test((struct test){
			.type = TEST_TYPE_SIMPLE,
			.file = file,
			.name = name,
			.should_succeed = should_succeed,
			.simple_test = (struct simple_test){
				.f = f,
			}
		});
}

void register_range_test(const char *file, const char *name, uint64_t start, uint64_t end,
			 bool (*f)(uint64_t start, uint64_t end), bool should_succeed)
{
	assert(start <= end);
	add_test((struct test){
			.type = TEST_TYPE_RANGE,
			.file = file,
			.name = name,
			.should_succeed = should_succeed,
			.range_test = (struct range_test){
				.start = start,
				.end = end,
				.f = f,
			}
		});
}

void register_random_test(const char *file, const char *name, uint64_t num_values,
			  bool (*f)(uint64_t, uint64_t), bool should_succeed)
{
	add_test((struct test){
			.type = TEST_TYPE_RANDOM,
			.file = file,
			.name = name,
			.should_succeed = should_succeed,
			.random_test = (struct random_test){
				.num_values = num_values,
				.f = f,
			}
		});
}

void check_failed(const char *func, const char *file, unsigned int line, const char *cond)
{
	test_log("[%s:%u: %s] CHECK failed: %s\n", file, line, func, cond);
}

#if 0
void test_log(const char *fmt, ...)
{
	(void)fmt;
	// TODO print into an fmemopen file?
}
#else
void test_log(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);
}
#endif

static int compare_tests(const void *_a, const void *_b)
{
	const struct test *a = _a;
	const struct test *b = _b;
	if (a->enabled && !b->enabled) {
		return -1;
	}
	if (!a->enabled && b->enabled) {
		return 1;
	}
	if (!a->enabled && !b->enabled) {
		return 0; // whatever
	}
	int r = strcmp(a->file, b->file);
	if (r != 0) {
		return r;
	}
	return strcmp(a->name, b->name); // TODO maybe sort by order in file instead (using __COUNTER__)?
}

static sigjmp_buf jump_buffer;
static bool in_test;

static void crash_handler(int sig)
{
	if (in_test) {
		siglongjmp(jump_buffer, sig);
	}
	signal(sig, SIG_DFL);
	raise(sig);
}

int main(int argc, char **argv)
{
	if (!tests) {
		fputs("no tests were registered", stderr);
		return 0;
	}
	bool seed_initialized = false;
	uint64_t seed = 0;
	for (;;) {
		int rv = getopt(argc, argv, "s:");
		if (rv == -1) {
			break;
		}
		switch (rv) {
		case '?':
			fprintf(stderr, "Usage: %s [-s <seed>]\n", argv[0]);
			return 1;
		case 's':
			errno = 0;
			char *endptr;
			seed = strtoull(optarg, &endptr, 0);
			if (endptr == optarg && errno == 0) {
				errno = EINVAL;
			}
			if (errno != 0) {
				perror("failed to parse random seed");
				exit(EXIT_FAILURE);
			}
			seed_initialized = true;
			break;
		}
	}
	argv += optind;
	argc -= optind;

	if (!seed_initialized) {
		if (getrandom(&seed, sizeof(seed), 0) == -1) {
			perror("getrandom failed");
			exit(EXIT_FAILURE);
		}
	}

	size_t num_enabled = 0;
	for (size_t i = 0; i < num_tests; i++) {
		struct test *test = &tests[i];
		if (argc == 0) {
			test->enabled = true;
			num_enabled++;
			continue;
		}
		for (int i = 0; i < argc; i++) {
			if (strcmp(test->name, argv[i]) == 0) {
				test->enabled = true;
				num_enabled++;
				break;
			}
		}
	}
	qsort(tests, num_tests, sizeof(tests[0]), compare_tests);
	num_tests = num_enabled;

	struct sigaction sa;
	sa.sa_handler = crash_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	if (sigaction(SIGABRT, &sa, NULL) == -1 ||
	    sigaction(SIGBUS, &sa, NULL) == -1 ||
	    sigaction(SIGSEGV, &sa, NULL) == -1 ||
	    sigaction(SIGILL, &sa, NULL) == -1 ||
	    sigaction(SIGFPE, &sa, NULL) == -1) {
		perror("sigaction failed");
		exit(EXIT_FAILURE);
	}

#define RED "\033[1;31m"
#define GREEN "\033[1;32m"
#define CYAN "\033[1;36m"
#define CLEAR_LINE "\033[2K\r"
#define RESET "\033[0m"

#define PASSED GREEN "passed" RESET
#define FAILED RED "FAILED" RESET

	size_t num_failed = 0;
	for (size_t i = 0; i < num_tests; i++) {
		fprintf(stderr, CLEAR_LINE "%zu/%zu\r", i, num_tests);

		struct test *test = &tests[i];

		bool success = false;
		int retval = sigsetjmp(jump_buffer, 1);
		if (retval == 0) {
			in_test = true;
			switch (test->type) {
			case TEST_TYPE_SIMPLE:
				success = test->simple_test.f();
				break;
			case TEST_TYPE_RANGE:
				success = test->range_test.f(test->range_test.start, test->range_test.end);
				break;
			case TEST_TYPE_RANDOM:
				success = test->random_test.f(test->random_test.num_values, seed);
				break;
			}
			in_test = false;
		} else {
			// printf("caught signal %d\n", retval);
		}
		bool passed = success == test->should_succeed;
		if (!passed) {
			num_failed++;
		}
		printf(CLEAR_LINE "[%s/%s] %s\n", test->file, test->name, passed ? PASSED : FAILED);
	}
	printf("Summary: %s%zu/%zu failed" RESET " (random seed: %" PRIu64 ")\n",
	       num_failed == 0 ? GREEN : RED, num_failed, num_tests, seed);

	free(tests);

	return num_failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
