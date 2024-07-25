#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include "random.h"
#include "testing.h"

static bool check_stats(double *numbers, size_t n, double min, double max, bool discrete)
{
	double mean = 0;
	for (size_t i = 0; i < n; i++) {
		mean = (i * mean + numbers[i]) / (i + 1);
	}
	double stddev = 0;
	for (size_t i = 0; i < n; i++) {
		double x = numbers[i] - mean;
		stddev = (i * stddev + x * x) / (i + 1);
	}
	stddev = sqrt(stddev);

	double target_mean = 0.5 * max + 0.5 * min;
	double target_stddev;
	if (discrete) {
		double d = max - min + 1;
		target_stddev = sqrt((d * d - 1) / 12.0);
	} else {
		double d = max - min;
		target_stddev = sqrt(d * d / 12.0);
	}

	double dev_mean = fabs(1.0 - mean / target_mean);
	double dev_stddev = fabs(1.0 - stddev / target_stddev);

	test_log("target: mean = %g, stddev = %g\n", target_mean, target_stddev);
	test_log("actual: mean = %g, stddev = %g\n", mean, stddev);
	test_log("deviation: mean = %g%%, stddev = %g%%\n", dev_mean, dev_stddev);

	CHECK(dev_mean < 0.001);
	CHECK(dev_stddev < 0.001);
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
	CHECK(check_stats(numbers, N, min_value, max_value, true));

	// for (size_t i = 0; i < N; i++) {
	// 	uint64_t x = random_next_u64_in_range2(rng, min_value, max_value);
	// 	CHECK(min_value <= x && x <= max_value);
	// 	numbers[i] = x;
	// }
	// CHECK(check_stats(numbers, N, min_value, max_value, true));

	for (size_t i = 0; i < N; i++) {
		uint32_t x = random_next_u32_in_range(rng, (uint32_t)min_value, (uint32_t)max_value);
		CHECK((uint32_t)min_value <= x && x <= (uint32_t)max_value);
		numbers[i] = x;
	}
	CHECK(check_stats(numbers, N, (uint32_t)min_value, (uint32_t)max_value, true));

	for (size_t i = 0; i < N; i++) {
		double x = random_next_double_in_range(rng, min_value, max_value);
		CHECK(min_value <= x && x <= max_value);
		numbers[i] = x;
	}
	CHECK(check_stats(numbers, N, min_value, max_value, false));

	for (size_t i = 0; i < N; i++) {
		float x = random_next_float_in_range(rng, min_value, max_value);
		CHECK(min_value <= x && x <= max_value);
		numbers[i] = x;
	}
	CHECK(check_stats(numbers, N, min_value, max_value, false));
	return true;
}

RANDOM_TEST(random, random_seed, 2)
{
	const size_t N = 32 * 1024 * 1024;

	struct random_state rng;
	random_state_init(&rng, random_seed);

	double *numbers = calloc(N, sizeof(numbers[0]));

	CHECK(test_range_functions(0, 100, numbers, &rng, N));
	CHECK(test_range_functions(0, 128, numbers, &rng, N));
	CHECK(test_range_functions(12345, 67890, numbers, &rng, N));
	CHECK(test_range_functions((uint64_t)UINT32_MAX + 1, UINT64_MAX - 1, numbers, &rng, N));
	CHECK(test_range_functions(0, 128, numbers, &rng, N));
	CHECK(test_range_functions(9, 9 + 1024 - 1, numbers, &rng, N));
	CHECK(test_range_functions(0, UINT32_MAX, numbers, &rng, N));
	CHECK(test_range_functions(0, UINT64_MAX, numbers, &rng, N));

	for (size_t i = 0; i < N; i++) {
		numbers[i] = random_next_u64(&rng);
	}
	CHECK(check_stats(numbers, N, 0, (double)UINT64_MAX, true));

	for (size_t i = 0; i < N; i++) {
		numbers[i] = random_next_u32(&rng);
	}
	CHECK(check_stats(numbers, N, 0, UINT32_MAX, true));

	for (size_t i = 0; i < N; i++) {
		numbers[i] = random_next_uniform_double(&rng);
		CHECK(0 <= numbers[i] && numbers[i] <= 1);
	}
	CHECK(check_stats(numbers, N, 0, 1, false));

	for (size_t i = 0; i < N; i++) {
		numbers[i] = random_next_uniform_float(&rng);
		CHECK(0 <= numbers[i] && numbers[i] <= 1);
	}
	CHECK(check_stats(numbers, N, 0, 1, false));

	for (uint64_t i = 0; i < N; i += 4) {
		struct random_state state;
		random_state_init(&state, i);
		numbers[i + 0] = state.s[0];
		numbers[i + 1] = state.s[1];
		numbers[i + 2] = state.s[2];
		numbers[i + 3] = state.s[3];
	}
	CHECK(check_stats(numbers, N / 4 * 4, 0, (double)UINT64_MAX, true));

	size_t nfalse = 0;
	size_t ntrue = 0;
	for (size_t i = 0; i < N; i++) {
		if (random_next_bool(&rng)) {
			ntrue++;
		} else {
			nfalse++;
		}
	}
	test_log("ntrue = %zu, nfalse = %zu\n", ntrue, nfalse);
	CHECK(fabs((double)ntrue - nfalse) / N < 0.001);

	free(numbers);

	return true;
}
