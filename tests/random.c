#include <assert.h>
#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>
#include "random.h"
#include "testing.h"

static bool check_stats(double *numbers, size_t n, double min, double max)
{
	double mean = 0;
	for (size_t i = 0; i < n; i++) {
		mean = (i * mean + numbers[i]) / (i + 1);
	}
	double stddev = 0;
	for (size_t i = 0; i < n; i++) {
		double x = numbers[i] - mean;
		stddev += x * x;
	}
	stddev = sqrt(stddev / n);

	double target_mean = (max - min) / 2.0;
	double target_stddev = sqrt((max - min) * (max - min) / 12.0);

	double dev_mean = fabs(target_mean - mean) / target_mean;
	double dev_stddev = fabs(target_stddev - stddev) / target_stddev;

	// printf("target: mean = %g, stddev = %g\n", target_mean, target_stddev);
	// printf("actual: mean = %g, stddev = %g\n", mean, stddev);
	// printf("deviation: mean = %g%%, stddev = %g%%\n", dev_mean, dev_stddev);

	CHECK(dev_mean < 0.001);
	CHECK(dev_stddev < 0.02);
	return true;
}

static bool test_range_functions(uint64_t min_value, uint64_t max_value, double *numbers,
				 struct random_state *rng, size_t N)
{
	for (size_t i = 0; i < N; i++) {
		uint64_t x = random_next_u64_in_range(rng, min_value, max_value);
		CHECK(min_value <= x && x <= max_value);
		numbers[i] = x;
	}
	CHECK(check_stats(numbers, N, min_value, max_value));

	for (size_t i = 0; i < N; i++) {
		uint64_t x = random_next_u64_in_range2(rng, min_value, max_value);
		CHECK(min_value <= x && x <= max_value);
		numbers[i] = x;
	}
	CHECK(check_stats(numbers, N, min_value, max_value));

	for (size_t i = 0; i < N; i++) {
		uint32_t x = random_next_u32_in_range(rng, min_value, max_value);
		CHECK(min_value <= x && x <= max_value);
		numbers[i] = x;
	}
	CHECK(check_stats(numbers, N, min_value, max_value));

	for (size_t i = 0; i < N; i++) {
		double x = random_next_double_in_range(rng, min_value, max_value);
		CHECK(min_value <= x && x <= max_value);
		numbers[i] = x;
	}
	CHECK(check_stats(numbers, N, min_value, max_value));

	for (size_t i = 0; i < N; i++) {
		float x = random_next_float_in_range(rng, min_value, max_value);
		CHECK(min_value <= x && x <= max_value);
		numbers[i] = x;
	}
	CHECK(check_stats(numbers, N, min_value, max_value));
	return true;
}

RANDOM_TEST(random, 2, 0, UINT64_MAX)
{
	const size_t N = 32 * 1024 * 1024;

	struct random_state rng;
	random_state_init(&rng, random);

	double *numbers = calloc(N, sizeof(numbers[0]));

	test_range_functions(0, 100, numbers, &rng, N);
	test_range_functions(0, 128, numbers, &rng, N);
	test_range_functions(12345, 67890, numbers, &rng, N);
	test_range_functions((uint64_t)UINT32_MAX + 1, UINT64_MAX - 1, numbers, &rng, N);
	test_range_functions(0, 128, numbers, &rng, N);
	test_range_functions(999, 999 + 1023, numbers, &rng, N);
	test_range_functions(0, UINT32_MAX, numbers, &rng, N);
	test_range_functions(0, UINT64_MAX, numbers, &rng, N);

	for (size_t i = 0; i < N; i++) {
		numbers[i] = random_next_u64(&rng);
	}
	CHECK(check_stats(numbers, N, 0, (double)UINT64_MAX));

	for (size_t i = 0; i < N; i++) {
		numbers[i] = random_next_u32(&rng);
	}
	CHECK(check_stats(numbers, N, 0, UINT32_MAX));

	for (size_t i = 0; i < N; i++) {
		numbers[i] = random_next_uniform_double(&rng);
		CHECK(0 <= numbers[i] && numbers[i] <= 1);
	}
	CHECK(check_stats(numbers, N, 0, 1));

	for (size_t i = 0; i < N; i++) {
		numbers[i] = random_next_uniform_float(&rng);
		CHECK(0 <= numbers[i] && numbers[i] <= 1);
	}
	CHECK(check_stats(numbers, N, 0, 1));

	for (uint64_t i = 0; i < N; i += 4) {
		struct random_state state;
		random_state_init(&state, i);
		numbers[i + 0] = state.s[0];
		numbers[i + 1] = state.s[1];
		numbers[i + 2] = state.s[2];
		numbers[i + 3] = state.s[3];
	}
	CHECK(check_stats(numbers, N / 4 * 4, 0, (double)UINT64_MAX));

	size_t nfalse = 0;
	size_t ntrue = 0;
	for (size_t i = 0; i < N; i++) {
		if (random_next_bool(&rng)) {
			ntrue++;
		} else {
			nfalse++;
		}
	}
	// printf("ntrue = %zu, nfalse = %zu\n", ntrue, nfalse);
	CHECK(fabs((double)ntrue - nfalse) / N < 0.001);

	free(numbers);

	return true;
}
