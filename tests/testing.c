/*
 * Copyright (C) 2024 Fabian Hügel
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/random.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <threads.h>
#include <time.h>
#include <unistd.h>
#include "compiler.h"
#include "macros.h"
#include "testing.h"

static uint64_t global_seed;

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
	bool (*f)(uint64_t);
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
	struct worker *workers;
	size_t num_workers;
	size_t num_workers_completed;
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
			  bool (*f)(uint64_t), bool should_succeed)
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
	fprintf(stderr, "CHECK failed: %s:%u: %s: %s\n", file, line, func, cond);
}

struct list_head {
	struct list_head *prev;
	struct list_head *next;
};

static struct list_head list_head_init(struct list_head *list)
{
	list->prev = list;
	list->next = list;
	return *list;
}

static bool list_empty(struct list_head *list)
{
	return list->next == list;
}

static void list_push_tail(struct list_head *list, struct list_head *item)
{
	item->prev = list->prev;
	item->next = list;
	list->prev->next = item;
	list->prev = item;
}

static void list_remove_item(struct list_head *item)
{
	item->prev->next = item->next;
	item->next->prev = item->prev;
	item->prev = (struct list_head *)(uintptr_t)0xdeadbeef;
	item->next = (struct list_head *)(uintptr_t)0xdeadbeef;
}

static struct list_head *list_pop_head(struct list_head *list)
{
	if (list_empty(list)) {
		return NULL;
	}
	struct list_head *item = list->next;
	list_remove_item(item);
	return item;
}

#define list_foreach(list, itername)					\
	for (struct list_head *itername = (list)->next, *___itername__##next = itername->next; itername != (list); itername = ___itername__##next, ___itername__##next = itername->next)

struct worker {
	struct list_head link;
	struct test *test;
	pid_t pid;
	int stdout_fd;
	int stderr_fd;
	bool passed;
	uint64_t runtime_ns;
	union {
		struct {
			uint64_t start;
			uint64_t end;
		} range;
		struct {
			uint64_t num_values;
			uint64_t seed;
		} random;
	};
};

static struct worker *to_worker(struct list_head *ptr)
{
	return ptr ? container_of(ptr, struct worker, link) : NULL;
}

static uint64_t splitmix64(uint64_t *state)
{
	uint64_t z = (*state += 0x9e3779b97f4a7c15);
	z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
	z = (z ^ (z >> 27)) * 0x94d049bb133111eb;
	return z ^ (z >> 31);
}

static void splitmix64_skip(uint64_t *state, uint64_t skip)
{
	*state += skip * 0x9e3779b97f4a7c15;
}

static bool run_simple_test(bool (*f)(void))
{
	return f();
}

static bool run_range_test(bool (*f)(uint64_t start, uint64_t end), uint64_t start, uint64_t end)
{
	return f(start, end);
}

static bool run_random_test(bool (*f)(uint64_t random), uint64_t num_values, uint64_t seed)
{
	for (uint64_t i = 0; i < num_values; i++) {
		uint64_t r = splitmix64(&seed);
		if (!f(r)) {
			fprintf(stderr, "test failed with random value: %" PRIu64 "\n", r);
			return false;
		}
	}
	return true;
}

static bool run_test(struct worker *worker)
{
	const struct test *test = worker->test;
	bool success = false;
	switch (worker->test->type) {
	case TEST_TYPE_SIMPLE:
		success = run_simple_test(test->simple_test.f);
		break;
	case TEST_TYPE_RANGE:
		success = run_range_test(test->range_test.f, worker->range.start, worker->range.end);
		break;
	case TEST_TYPE_RANDOM:
		success = run_random_test(test->random_test.f, worker->random.num_values, worker->random.seed);
		break;
	}
	return success;
}

static void worker_start(struct worker *worker)
{
	pid_t pid = fork();
	if (pid == -1) {
		perror("fork failed");
		exit(EXIT_FAILURE);
	}

	if (pid != 0) {
		worker->pid = pid;
		return;
	}

	if (dup2(worker->stdout_fd, STDOUT_FILENO) == -1 ||
	    dup2(worker->stderr_fd, STDERR_FILENO) == -1) {
		perror("dup2 failed");
		exit(EXIT_FAILURE);
	}
	bool success = run_test(worker);
	exit(success ? EXIT_SUCCESS : EXIT_FAILURE);
}

static struct worker *test_create_workers(struct test *test, size_t n)
{
	struct worker *workers = calloc(n, sizeof(*workers));
	if (!workers) {
		perror("calloc");
		exit(EXIT_FAILURE);
	}
	test->workers = workers;
	test->num_workers = n;
	for (size_t i = 0; i < n; i++) {
		workers[i].test = test;
		workers[i].stdout_fd = memfd_create("test stdout", MFD_CLOEXEC);
		if (workers[i].stdout_fd == -1) {
			perror("memfd_create failed");
			exit(EXIT_FAILURE);
		}
		workers[i].stderr_fd = memfd_create("test stderr", MFD_CLOEXEC);
		if (workers[i].stderr_fd == -1) {
			perror("memfd_create failed");
			exit(EXIT_FAILURE);
		}
	}
	return workers;
}

static void queue_simple_test_work(struct list_head *queue, struct test *test, unsigned int nthreads)
{
	(void)nthreads;
	struct worker *worker = test_create_workers(test, 1);
	list_push_tail(queue, &worker->link);
}

static void queue_range_test_work(struct list_head *queue, struct test *test, unsigned int nthreads)
{
	uint64_t start = test->range_test.start;
	uint64_t end = test->range_test.end;
	uint64_t n = end - start;
	if (nthreads - 1 > n) {
		nthreads = n + 1;
	}
	uint64_t per_thread = n / nthreads;
	uint64_t rem = n % nthreads + 1;
	uint64_t cur = start - 1;
	struct worker *workers = test_create_workers(test, nthreads);
	for (unsigned int t = 0; t < nthreads; t++) {
		workers[t].range.start = cur + 1;
		cur += per_thread;
		if (rem) {
			cur++;
			rem--;
		}
		workers[t].range.end = cur;
		list_push_tail(queue, &workers[t].link);
	}
	assert(cur == end);
}

static void queue_random_test_work(struct list_head *queue, struct test *test, unsigned int nthreads)
{
	uint64_t n = test->random_test.num_values;
	if (nthreads > n) {
		nthreads = n;
	}
	uint64_t per_thread = n / nthreads;
	uint64_t rem = n % nthreads;
	uint64_t total = 0;
	uint64_t seed = global_seed; // we just use the same seed for every random test
	struct worker *workers = test_create_workers(test, nthreads);
	for (unsigned int t = 0; t < nthreads; t++) {
		uint64_t num_values = per_thread;
		if (rem) {
			num_values++;
			rem--;
		}
		workers[t].random.num_values = num_values;
		total += num_values;
		workers[t].random.seed = seed;
		splitmix64_skip(&seed, num_values);
		list_push_tail(queue, &workers[t].link);
	}
	assert(total == n);
}

static void queue_test_work(struct list_head *queue, struct test *test, unsigned int nthreads)
{
	assert(test->enabled);
	switch (test->type) {
	case TEST_TYPE_SIMPLE:
		queue_simple_test_work(queue, test, nthreads);
		break;
	case TEST_TYPE_RANGE:
		queue_range_test_work(queue, test, nthreads);
		break;
	case TEST_TYPE_RANDOM:
		queue_random_test_work(queue, test, nthreads);
		break;
	}
	assert(test->num_workers != 0);
}

static void free_workers(void)
{
	for (size_t i = 0; i < num_tests; i++) {
		free(tests[i].workers);
	}
}

static void print_time(uint64_t ns, FILE *file)
{
	double t = ns;
	const char *unit = "ns";
	if (t > 1000000000) {
		t *= 1.0 / 1000000000;
		unit = "s";
	} else if (t > 1000000) {
		t *= 1.0 / 1000000;
		unit = "ms";
	} else if (t > 1000) {
		t *= 1.0 / 1000;
		unit = "us";
	}
	fprintf(file, "%.1f %s", t, unit);
}

static void print_test_output(const char *name, int fd)
{
	struct stat stat;
	if (fstat(fd, &stat) != 0) {
		perror("fstat failed");
		exit(EXIT_FAILURE);
	}
	if (stat.st_size == 0) {
		return;
	}
	if (lseek(fd, 0, SEEK_SET) != 0) {
		perror("lseek failed");
		exit(EXIT_FAILURE);
	}
	FILE *f = fdopen(fd, "r");
	if (!f) {
		perror("fdopen failed");
		exit(EXIT_FAILURE);
	}
	printf("    %s:\n", name);
	bool newline = true;
	for (;;) {
		char buf[4096];
		if (!fgets(buf, sizeof(buf), f)) {
			if (ferror(f)) {
				perror("fgets failed");
				exit(EXIT_FAILURE);
			}
			break;
		}
		if (newline) {
			fputs("      ", stdout);
		}
		fputs(buf, stdout);
		newline = buf[strlen(buf) - 1] == '\n';
	}
	putchar('\n');
	fclose(f);
}

#define RED "\033[31m"
#define GREEN "\033[32m"
#define CLEAR_LINE "\033[2K\r"
#define RESET "\033[0m"

#define PASSED GREEN "passed" RESET
#define FAILED RED "FAILED" RESET

static bool print_test_results(struct test *test)
{
	uint64_t runtime = 0;
	bool test_passed = true;
	for (size_t j = 0; j < test->num_workers; j++) {
		struct worker *work = &test->workers[j];
		runtime += work->runtime_ns;
		test_passed &= work->passed;
	}
	printf("\r[%s/%s] %s (", test->file, test->name, test_passed ? PASSED : FAILED);
	print_time(runtime, stdout);
	puts(")");
	if (test_passed) {
		return true;
	}
	for (size_t j = 0; j < test->num_workers; j++) {
		struct worker *work = &test->workers[j];
		if (work->passed) {
			continue;
		}
		switch (test->type) {
		case TEST_TYPE_SIMPLE:
			break;
		case TEST_TYPE_RANGE: {
			printf("  " FAILED " on range: [%" PRIu64 ", %" PRIu64 "]\n",
			       work->range.start, work->range.end);
			break;
		}
		case TEST_TYPE_RANDOM: {
			break;
		}
		}
		print_test_output("STDOUT", work->stdout_fd);
		print_test_output("STDERR", work->stderr_fd);
	}
	return false;
}

static uint64_t timeval_to_ns(struct timeval tv)
{
	return tv.tv_usec * UINT64_C(1000) + tv.tv_sec * UINT64_C(1000000000);
}

static uint64_t ns_elapsed(struct timespec start, struct timespec end)
{
	uint64_t start_ns = start.tv_sec * UINT64_C(1000000000) + start.tv_nsec;
	uint64_t end_ns = end.tv_sec * UINT64_C(1000000000) + end.tv_nsec;
	return end_ns - start_ns;
}

static struct worker *wait_for_workers(struct list_head *running)
{
	assert(!list_empty(running));
	int status;
	struct rusage rusage;
	pid_t pid = wait4(-1, &status, 0, &rusage);
	if (pid == -1) {
		perror("wait4 failed");
		exit(EXIT_FAILURE);
	}
	struct worker *worker = NULL;
	list_foreach(running, cur) {
		struct worker *cur_worker = to_worker(cur);
		if (cur_worker->pid == pid) {
			worker = cur_worker;
			break;
		}
	}
	assert(worker);
	bool success = WIFEXITED(status) && WEXITSTATUS(status) == 0;
	worker->passed = worker->test->should_succeed ? success : !success;
	worker->runtime_ns = timeval_to_ns(rusage.ru_utime) + timeval_to_ns(rusage.ru_stime);
	return worker;
}

static bool run(struct list_head *queue, size_t nthreads)
{
	assert(nthreads > 0);

	struct timespec start, end;
	{
		int ret = clock_gettime(CLOCK_MONOTONIC, &start);
		assert(ret == 0);
	}

	size_t workers_running = 0;
	struct list_head running = list_head_init(&running);

	size_t num_completed = 0;
	size_t completed_cursor = 0;
	size_t num_failed = 0;

	while (completed_cursor < num_tests) {
		fprintf(stderr, CLEAR_LINE "%zu/%zu", num_completed, num_tests);
		while (workers_running < nthreads) {
			struct worker *worker = to_worker(list_pop_head(queue));
			if (!worker) {
				break;
			}
			worker_start(worker);
			list_push_tail(&running, &worker->link);
			workers_running++;
		}
		struct worker *worker = wait_for_workers(&running);
		workers_running--;
		list_remove_item(&worker->link);
		struct test *test = worker->test;
		test->num_workers_completed++;
		if (test->num_workers_completed < test->num_workers) {
			continue;
		}
		num_completed++;
		while (completed_cursor < num_tests) {
			test = &tests[completed_cursor];
			if (test->num_workers_completed < test->num_workers) {
				break;
			}
			if (!print_test_results(test)) {
				num_failed++;
			}
			completed_cursor++;
		}
	}

	{
		int ret = clock_gettime(CLOCK_MONOTONIC, &end);
		assert(ret == 0);
	}

	printf("Summary: %s%zu/%zu failed" RESET " (",
	       num_failed == 0 ? GREEN : RED, num_failed, num_tests);
	print_time(ns_elapsed(start, end), stdout);
	printf(") (random seed: %" PRIu64 ")\n", global_seed);

	return num_failed == 0;
}

NEGATIVE_SIMPLE_TEST(selftest1)
{
	raise(SIGABRT);
	return true;
}

NEGATIVE_SIMPLE_TEST(selftest2)
{
	return false;
}

RANGE_TEST(selftest3, 13, 17)
{
	CHECK(start <= end);
	CHECK(13 <= start && end <= 17);
	// fprintf(stderr, "%lu-%lu\n", (unsigned long)start, (unsigned long)end);
	return true;
}

RANDOM_TEST(selftest4, 9)
{
	(void)random_seed;
	return true;
	// fprintf(stderr, "%lu\n", (unsigned long)random_seed);
	// return false;
}

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

int main(int argc, char **argv)
{
	if (!tests) {
		fputs("no tests were registered", stderr);
		return 0;
	}
	unsigned int nthreads = 0;
	const char *accept_file_substr = NULL;
	const char *accept_file_exact = NULL;
	const char *accept_name_substr = NULL;
	const char *accept_name_exact = NULL;
	const char *reject_file_substr = NULL;
	const char *reject_file_exact = NULL;
	const char *reject_name_substr = NULL;
	const char *reject_name_exact = NULL;
	bool seed_initialized = false;
	bool reject = false;
	for (;;) {
		int rv = getopt(argc, argv, "j:f:F:t:T:s:n");
		if (rv == -1) {
			break;
		}
		switch (rv) {
		case '?':
			fprintf(stderr, "Usage: %s TODO\n", argv[0]);
			return 1;
		case 'j':
			nthreads = atoi(optarg);
			break;
		case 'n':
			reject = true;
			break;
		case 'f':
			if (reject) {
				reject_file_substr = optarg;
			} else {
				accept_file_substr = optarg;
			}
			break;
		case 'F':
			if (reject) {
				reject_file_exact = optarg;
			} else {
				accept_file_exact = optarg;
			}
			break;
		case 't':
			if (reject) {
				reject_name_substr = optarg;
			} else {
				accept_name_substr = optarg;
			}
			break;
		case 'T':
			if (reject) {
				reject_name_exact = optarg;
			} else {
				accept_name_exact = optarg;
			}
			break;
		case 's':
			errno = 0;
			char *endptr;
			global_seed = strtoull(optarg, &endptr, 0);
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
		if (rv != 'n') {
			reject = false;
		}
	}
	argv += optind;
	argc -= optind;

	if (nthreads == 0) {
		nthreads = get_nprocs();
	}

	if (!seed_initialized) {
		if (getrandom(&global_seed, sizeof(global_seed), 0) == -1) {
			perror("getrandom failed");
			exit(EXIT_FAILURE);
		}
	}

	struct list_head queue = list_head_init(&queue);

	size_t num_enabled = 0;
	for (size_t i = 0; i < num_tests; i++) {
		tests[i].enabled = false;
		if (accept_file_substr && !strstr(tests[i].file, accept_file_substr)) {
			continue;
		}
		if (accept_file_exact && strcmp(tests[i].file, accept_file_exact) != 0) {
			continue;
		}
		if (accept_name_substr && !strstr(tests[i].name, accept_name_substr)) {
			continue;
		}
		if (accept_name_exact && strcmp(tests[i].name, accept_name_exact) != 0) {
			continue;
		}
		if (reject_file_substr && strstr(tests[i].file, reject_file_substr)) {
			continue;
		}
		if (reject_file_exact && strcmp(tests[i].file, reject_file_exact) == 0) {
			continue;
		}
		if (reject_name_substr && strstr(tests[i].name, reject_name_substr)) {
			continue;
		}
		if (reject_name_exact && strcmp(tests[i].name, reject_name_exact) == 0) {
			continue;
		}
		tests[i].enabled = true;
		num_enabled++;
	}

	qsort(tests, num_tests, sizeof(tests[0]), compare_tests);
	num_tests = num_enabled;
	for (size_t i = 0; i < num_tests; i++) {
		queue_test_work(&queue, &tests[i], nthreads);
	}

	bool success = run(&queue, nthreads);

	free_workers();
	free(tests);

	return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
