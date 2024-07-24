#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "testing.h"

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
