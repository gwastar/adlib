#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "random.h"

static double ns_elapsed(struct timespec start, struct timespec end)
{
	double s = end.tv_sec - start.tv_sec;
	double ns = end.tv_nsec - start.tv_nsec;
	return ns + 1000000000 * s;
}

static int compare_doubles(const void *_a, const void *_b)
{
	double a = *(const double *)_a;
	double b = *(const double *)_b;
	return a < b ? -1 : (a == b ? 0 : 1);
}

static double get_median(double *values, size_t n)
{
	qsort(values, n, sizeof(values[0]), compare_doubles);
	double x = 0.5 * (n - 1);
	size_t idx0 = (size_t)x;
	size_t idx1 = idx0 + (n != 1);
	double fract = x - idx0;
	return (1 - fract) * values[idx0] + fract * values[idx1];
}

static double overhead;

static void measure_overhead(void)
{
	struct timespec start_tp, end_tp;
	const unsigned int n = 10000;
	double times[n];
	for (unsigned int i = 0; i < n; i++) {
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp);
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);
		times[i] = ns_elapsed(start_tp, end_tp);
	}
	overhead = get_median(times, n);
}

#define BENCHMARK(random_call)						\
	do {								\
		struct random_state rng;				\
		random_state_init(&rng, 0xdeadbeef);			\
		double times[5];					\
		const unsigned int n = sizeof(times) / sizeof(times[0]); \
		const unsigned int iterations = 1 << 26;		\
		for (unsigned int k = 0; k < n; k++) {			\
			struct timespec start_tp, end_tp;		\
			clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp); \
			for (unsigned int i = 0; i < iterations; i++) {	\
				uint64_t r = (random_call);		\
				asm volatile("" :: "g"(rng), "g"(r) : "memory"); \
			}						\
			clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp); \
			times[k] = (ns_elapsed(start_tp, end_tp) - overhead) / iterations; \
		}							\
		/* TODO more stats would be interesting here */		\
		double t = get_median(times, n);			\
		printf("[%s]: %16.2f ns\n", __func__ + strlen("benchmark_"), t); \
	} while (0)

static void benchmark_random64(void)
{
	BENCHMARK(random_next_u64(&rng));
}

static void benchmark_random64_range(void)
{
	BENCHMARK(random_next_u64_in_range(&rng, 0, 100));
}

// static void benchmark_random64_range2(void)
// {
// 	BENCHMARK(random_next_u64_in_range2(&rng, 0, 100));
// }

static void benchmark_random64_range_pow2(void)
{
	BENCHMARK(random_next_u64_in_range(&rng, 0, 127));
}

// static void benchmark_random64_range2_pow2(void)
// {
// 	BENCHMARK(random_next_u64_in_range2(&rng, 0, 127));
// }

static void benchmark_random32_range(void)
{
	BENCHMARK(random_next_u32_in_range(&rng, 0, 100));
}

static void benchmark_random32_range_pow2(void)
{
	BENCHMARK(random_next_u32_in_range(&rng, 0, 127));
}

int main(void)
{
	measure_overhead();

	benchmark_random64();
	benchmark_random64_range();
	// benchmark_random64_range2();
	benchmark_random64_range_pow2();
	// benchmark_random64_range2_pow2();
	benchmark_random32_range();
	benchmark_random32_range_pow2();
}
