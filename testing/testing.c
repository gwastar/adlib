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
#include <linux/close_range.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
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
#include <time.h>
#include <unistd.h>
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

void test_log(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);
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
	int output_fd;
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

#define container_of(ptr, type, member)					\
	((type *)((char *)(1 ? (ptr) : &((type *)0)->member) - offsetof(type, member)))

static struct worker *to_worker(struct list_head *ptr)
{
	return ptr ? container_of(ptr, struct worker, link) : NULL;
}

// static uint64_t splitmix64(uint64_t *state)
// {
// 	uint64_t z = (*state += 0x9e3779b97f4a7c15);
// 	z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
// 	z = (z ^ (z >> 27)) * 0x94d049bb133111eb;
// 	return z ^ (z >> 31);
// }

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

static bool run_random_test(bool (*f)(uint64_t num_values, uint64_t seed), uint64_t num_values, uint64_t seed)
{
	return f(num_values, seed);
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

	if (dup2(worker->output_fd, STDOUT_FILENO) == -1 ||
	    dup2(worker->output_fd, STDERR_FILENO) == -1) {
		perror("dup2 failed");
		exit(EXIT_FAILURE);
	}

	if (close_range(3, ~0U, CLOSE_RANGE_UNSHARE) == -1) {
		perror("close_range failed");
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
		workers[i].output_fd = memfd_create("test output", MFD_CLOEXEC);
		if (workers[i].output_fd == -1) {
			perror("memfd_create failed");
			exit(EXIT_FAILURE);
		}
	}
	return workers;
}

static void queue_simple_test_work(struct list_head *queue, struct test *test, unsigned int num_jobs)
{
	(void)num_jobs;
	struct worker *worker = test_create_workers(test, 1);
	list_push_tail(queue, &worker->link);
}

static void queue_range_test_work(struct list_head *queue, struct test *test, unsigned int num_jobs)
{
	uint64_t start = test->range_test.start;
	uint64_t end = test->range_test.end;
	uint64_t n = end - start;
	if (num_jobs - 1 > n) {
		num_jobs = n + 1;
	}
	uint64_t per_thread = n / num_jobs;
	uint64_t rem = n % num_jobs + 1;
	uint64_t cur = start - 1;
	struct worker *workers = test_create_workers(test, num_jobs);
	for (unsigned int t = 0; t < num_jobs; t++) {
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

static void queue_random_test_work(struct list_head *queue, struct test *test, unsigned int num_jobs)
{
	uint64_t n = test->random_test.num_values;
	if (num_jobs > n) {
		num_jobs = n;
	}
	uint64_t per_thread = n / num_jobs;
	uint64_t rem = n % num_jobs;
	uint64_t total = 0;
	uint64_t seed = global_seed; // we just use the same seed for every random test
	struct worker *workers = test_create_workers(test, num_jobs);
	for (unsigned int t = 0; t < num_jobs; t++) {
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

static void queue_test_work(struct list_head *queue, struct test *test, unsigned int num_jobs)
{
	assert(test->enabled);
	switch (test->type) {
	case TEST_TYPE_SIMPLE:
		queue_simple_test_work(queue, test, num_jobs);
		break;
	case TEST_TYPE_RANGE:
		queue_range_test_work(queue, test, num_jobs);
		break;
	case TEST_TYPE_RANDOM:
		queue_random_test_work(queue, test, num_jobs);
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

static uint64_t fd_get_size(int fd)
{
	struct stat stat;
	if (fstat(fd, &stat) != 0) {
		perror("fstat failed");
		exit(EXIT_FAILURE);
	}
	return stat.st_size;
}

// this closes the fd
static void print_test_output(int fd)
{
	if (lseek(fd, 0, SEEK_SET) != 0) {
		perror("lseek failed");
		exit(EXIT_FAILURE);
	}
	FILE *f = fdopen(fd, "r");
	if (!f) {
		perror("fdopen failed");
		exit(EXIT_FAILURE);
	}
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
			fputs("    ", stdout);
		}
		fputs(buf, stdout);
		newline = buf[strlen(buf) - 1] == '\n';
	}
	if (!newline) {
		putchar('\n');
	}
	fclose(f);
}

#define RED "\033[1;31m"
#define GREEN "\033[1;32m"
#define CYAN "\033[1;36m"
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
	printf(CLEAR_LINE "[%s/%s] %s (", test->file, test->name, test_passed ? PASSED : FAILED);
	print_time(runtime, stdout);
	puts(")");
	for (size_t j = 0; j < test->num_workers; j++) {
		struct worker *work = &test->workers[j];
		if (work->passed || fd_get_size(work->output_fd) == 0) {
			close(work->output_fd);
			continue;
		}
		printf("  " CYAN "[worker %zu]" RESET "\n", j + 1);
		print_test_output(work->output_fd);
	}
	return test_passed;
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

static bool run(struct list_head *queue, size_t num_jobs)
{
	assert(num_jobs > 0);

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
		while (workers_running < num_jobs) {
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

RANGE_TEST(selftest3, UINT64_MAX - 1, UINT64_MAX)
{
	CHECK(UINT64_MAX - 1 <= value && value <= UINT64_MAX);
	return true;
}

RANDOM_TEST(selftest4, 2)
{
	(void)random_seed;
#if 0
	if (random_seed % 2 == 0) {
		fputs("Hello world", stdout);
		return false;
	}
#endif
	return true;
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

static void usage(char *executable_path)
{
	printf("Usage: %s [-j <num_jobs>] [-s <seed>] [-f/-F <filename>] [-t/-T <name>] [-n]\n",
	       executable_path);
	puts("");
	puts("-j    specify the number of jobs to run in parallel");
	puts("-s    set a fixed seed for the random tests, otherwise the seed is random");
	puts("-f    run all tests in files that contain this substring");
	puts("-F    same as -f but the entire filename needs to match");
	puts("-t    run all tests whose name contains this substring");
	puts("-T    same as -t but the entire name needs to match");
	puts("-n    combine this with -f/-F/-t/-T to reject matching tests instead");
	puts("");
	puts("Test filters like -f, -F, -t, -T can be specified multiple times. They are");
	puts("evaluated in order. So '-t selftest -nT selftest2' would run all selftests");
	puts("except 'selftest2', but '-nT selftest2 -t selftest' would run all selftests.");
}

int main(int argc, char **argv)
{
	if (!tests) {
		fputs("no tests were registered\n", stderr);
		return EXIT_SUCCESS;
	}
	unsigned int num_jobs = 0;
	struct match_arg {
		const char *string;
		bool file;
		bool exact;
		bool reject;
	} *match_args = calloc(argc, sizeof(match_args[0]));
	size_t num_match_args = 0;
	bool seed_initialized = false;
	bool reject = false;
	for (;;) {
		int rv = getopt(argc, argv, "hj:f:F:t:T:s:n");
		if (rv == -1) {
			break;
		}
		switch (rv) {
		default:
			usage(argv[0]);
			return EXIT_FAILURE;
		case 'j':
			num_jobs = atoi(optarg);
			break;
		case 'n':
			break;
		case 'f':
		case 'F':
		case 't':
		case 'T':
			match_args[num_match_args++] = (struct match_arg) {
				.string = optarg,
				.file = rv == 'f' || rv == 'F',
				.exact = rv == 'F' || rv == 'T',
				.reject = reject,
			};
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
		reject = rv == 'n';
	}
	argv += optind;
	argc -= optind;

	if (num_jobs == 0) {
		num_jobs = get_nprocs();
	}

	if (!seed_initialized) {
		if (getrandom(&global_seed, sizeof(global_seed), 0) == -1) {
			perror("getrandom failed");
			exit(EXIT_FAILURE);
		}
	}

	size_t num_enabled = 0;
	for (size_t i = 0; i < num_tests; i++) {
		tests[i].enabled = num_match_args == 0;
		for (size_t j = 0; j < num_match_args; j++) {
			struct match_arg *m = &match_args[j];
			const char *s = m->file ? tests[i].file : tests[i].name;
			bool match = m->exact ? strcmp(s, m->string) == 0 : !!strstr(s, m->string);
			if (!match) {
				continue;
			}
			tests[i].enabled = !m->reject;
		}
		if (tests[i].enabled) {
			num_enabled++;
		}
	}

	qsort(tests, num_tests, sizeof(tests[0]), compare_tests);
	num_tests = num_enabled;

	struct list_head queue = list_head_init(&queue);
	for (size_t i = 0; i < num_tests; i++) {
		queue_test_work(&queue, &tests[i], num_jobs);
	}

	bool success = run(&queue, num_jobs);

	free_workers();
	free(tests);
	free(match_args);

	return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
