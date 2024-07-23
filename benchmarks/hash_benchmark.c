#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "hash.h"
#include "random.h"

static struct random_state global_random_state;

static void random_fill_buffer(void *buf, size_t size)
{
	uint8_t *p = buf;
	while (size > 8) {
		uint64_t r = random_next_u64(&global_random_state);
		memcpy(p, &r, 8);
		p += 8;
		size -= 8;
	}
	uint64_t r = random_next_u64(&global_random_state);
	memcpy(p, &r, size);
}

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

#define STRINGHASH_BENCHMARK(hash_call)					\
	do {								\
		const unsigned int max_shift = 24;			\
		uint8_t *input = malloc(1u << max_shift);		\
		assert(input);						\
		random_fill_buffer(input, 1u << max_shift);		\
		for (unsigned int shift = 0; shift <= max_shift; shift++) { \
			const size_t inlen = 1u << shift;		\
			double times[5];				\
			const unsigned int n = sizeof(times) / sizeof(times[0]); \
			unsigned int d_shift = max_shift - shift;	\
			const unsigned int iterations = (1u << d_shift) / (d_shift + 1) * 3; \
			for (unsigned int k = 0; k < n; k++) {		\
				struct timespec start_tp, end_tp;	\
				clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp); \
				for (unsigned int i = 0; i < iterations; i++) {	\
					typeof(hash_call) h = (hash_call); \
					asm volatile("" :: "g" (input), "g"(h) : "memory"); \
				}					\
				clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp); \
				times[k] = (ns_elapsed(start_tp, end_tp) - overhead) / iterations; \
			}						\
			double t = get_median(times, n);		\
			printf("[%s] inlen=2^%2u: %16.2f ns %8.2f ns/B\n", \
			       __func__ + strlen("benchmark_"), shift, t, t / inlen); \
		}							\
		free(input);						\
		putchar('\n');						\
	} while (0)

#define INTHASH_BENCHMARK(hash_call)					\
	do {								\
		double times[5];					\
		const unsigned int n = sizeof(times) / sizeof(times[0]); \
		const unsigned int iterations = 1 << 24;		\
		for (unsigned int k = 0; k < n; k++) {			\
			uint64_t input = random_next_u64(&global_random_state);	\
			struct timespec start_tp, end_tp;		\
			clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp); \
			for (unsigned int i = 0; i < iterations; i++, input++) { \
				typeof(hash_call) h = (hash_call);	\
				asm volatile("" :: "g" (input), "g"(h) : "memory"); \
			}						\
			clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp); \
			times[k] = (ns_elapsed(start_tp, end_tp) - overhead) / iterations; \
		}							\
		double t = get_median(times, n);			\
		printf("[%s]: %16.2f ns\n", __func__ + strlen("benchmark_"), t); \
	} while (0)


static void benchmark_siphash24_64(void)
{
	uint8_t key[16];
	random_fill_buffer(key, sizeof(key));
	STRINGHASH_BENCHMARK(siphash24_64(input, inlen, key));
}

static void benchmark_siphash24_128(void)
{
	uint8_t key[16];
	random_fill_buffer(key, sizeof(key));
	STRINGHASH_BENCHMARK(siphash24_128(input, inlen, key));
}

static void benchmark_siphash13_64(void)
{
	uint8_t key[16];
	random_fill_buffer(key, sizeof(key));
	STRINGHASH_BENCHMARK(siphash13_64(input, inlen, key));
}

static void benchmark_siphash13_128(void)
{
	uint8_t key[16];
	random_fill_buffer(key, sizeof(key));
	STRINGHASH_BENCHMARK(siphash13_128(input, inlen, key));
}

static void benchmark_halfsiphash24_32(void)
{
	uint8_t key[8];
	random_fill_buffer(key, sizeof(key));
	STRINGHASH_BENCHMARK(halfsiphash24_32(input, inlen, key));
}

static void benchmark_halfsiphash24_64(void)
{
	uint8_t key[8];
	random_fill_buffer(key, sizeof(key));
	STRINGHASH_BENCHMARK(halfsiphash24_64(input, inlen, key));
}

static void benchmark_halfsiphash13_32(void)
{
	uint8_t key[8];
	random_fill_buffer(key, sizeof(key));
	STRINGHASH_BENCHMARK(halfsiphash13_32(input, inlen, key));
}

static void benchmark_halfsiphash13_64(void)
{
	uint8_t key[8];
	random_fill_buffer(key, sizeof(key));
	STRINGHASH_BENCHMARK(halfsiphash13_64(input, inlen, key));
}

static void benchmark_murmurhash3_x86_32(void)
{
	uint32_t seed = random_next_u32(&global_random_state);
	STRINGHASH_BENCHMARK(murmurhash3_x86_32(input, inlen, seed));
}

static void benchmark_murmurhash3_x86_64(void)
{
	uint32_t seed = random_next_u32(&global_random_state);
	STRINGHASH_BENCHMARK(murmurhash3_x86_64(input, inlen, seed));
}

static void benchmark_murmurhash3_x86_128(void)
{
	uint32_t seed = random_next_u32(&global_random_state);
	STRINGHASH_BENCHMARK(murmurhash3_x86_128(input, inlen, seed));
}

static void benchmark_murmurhash3_x64_64(void)
{
	uint32_t seed = random_next_u32(&global_random_state);
	STRINGHASH_BENCHMARK(murmurhash3_x64_64(input, inlen, seed));
}

static void benchmark_murmurhash3_x64_128(void)
{
	uint32_t seed = random_next_u32(&global_random_state);
	STRINGHASH_BENCHMARK(murmurhash3_x64_128(input, inlen, seed));
}

static void benchmark_hash_int32(void)
{
	INTHASH_BENCHMARK(hash_int32(input));
}

static void benchmark_hash_int64(void)
{
	INTHASH_BENCHMARK(hash_int64(input));
}

static void benchmark_fibonacci_hash32(void)
{
	INTHASH_BENCHMARK(fibonacci_hash32(input, 24));
}

static void benchmark_fibonacci_hash64(void)
{
	INTHASH_BENCHMARK(fibonacci_hash64(input, 48));
}

static void benchmark_hash_combine_int32(void)
{
	INTHASH_BENCHMARK(hash_combine_int32(input, ~input));
}

static void benchmark_hash_combine_int64(void)
{
	INTHASH_BENCHMARK(hash_combine_int64(input, ~input));
}

int main(int argc, char **argv)
{
	random_state_init(&global_random_state, 0xdeadbeef);
	measure_overhead();

	struct benchmark {
		const char *name;
		void (*run)(void);
	} benchmarks[] = {
#define B(name) { #name, benchmark_##name }
		B(siphash24_64),
		B(siphash24_128),
		B(siphash13_64),
		B(siphash13_128),
		B(halfsiphash24_32),
		B(halfsiphash24_64),
		B(halfsiphash13_32),
		B(halfsiphash13_64),
		B(murmurhash3_x86_32),
		B(murmurhash3_x86_64),
		B(murmurhash3_x86_128),
		B(murmurhash3_x64_64),
		B(murmurhash3_x64_128),
		B(hash_int32),
		B(hash_int64),
		B(fibonacci_hash32),
		B(fibonacci_hash64),
		B(hash_combine_int32),
		B(hash_combine_int64),
	};

	for (size_t i = 0; i < sizeof(benchmarks) / sizeof(benchmarks[0]); i++) {
		bool skip = false;
		for (int j = 1; j < argc; j++) {
			skip = strcmp(argv[j], benchmarks[i].name) != 0;
			if (!skip) {
				break;
			}
		}
		if (skip) {
			continue;
		}
		benchmarks[i].run();
	}
}
