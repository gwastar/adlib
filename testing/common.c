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

#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "common.h"
#include "testing.h"

struct test *tests;
size_t num_tests;
size_t tests_capacity;

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

void print_test_output(int fd)
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

int compare_tests(const void *_a, const void *_b)
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
