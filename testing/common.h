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

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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

extern struct test *tests;
extern size_t num_tests;
extern size_t tests_capacity;

// this closes the fd
void print_test_output(int fd);
int compare_tests(const void *_a, const void *_b);
