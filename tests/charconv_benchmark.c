#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "charconv.h"

static double elapsed(struct timespec *start, struct timespec *end)
{
	long s = end->tv_sec - start->tv_sec;
	long ns = end->tv_nsec - start->tv_nsec;
	return ns + 1000000000.0 * s;
}

static void to_chars_benchmark(void)
{
	printf("%-10.10s \u2502 %-10.10s \u2502 %-10.10s\n",
	       "to_chars", "int32_t", "int64_t");
	for (unsigned int i = 0; i < 3 * 12 - 1; i++) {
		if (i % 13 == 11) {
			fputs("\u253c", stdout);
		} else {
			fputs("\u2500", stdout);
		}
	}
	putchar('\n');
	const size_t n = 1u << 23;
	struct timespec start, end;
	for (unsigned int base = 2; base <= 36; base++) {
		printf("base %-5u \u2502 ", base);
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start);
		for (size_t i = 0; i < n; i++) {
			char buf[128];
			size_t len = to_chars(buf, sizeof(buf), (int32_t)i, base);
			asm volatile("" :: "r"(len), "m"(buf) : "memory");
		}
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end);
		double t = elapsed(&start, &end);
		printf("%7.2f ns \u2502", t / n);

		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start);
		for (size_t i = 0; i < n; i++) {
			char buf[128];
			size_t len = to_chars(buf, sizeof(buf), (int64_t)i, base);
			asm volatile("" :: "r"(len), "m"(buf) : "memory");
		}
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end);
		t = elapsed(&start, &end);
		printf("%7.2f ns\n", t / n);
	}
}

static void from_chars_benchmark(void)
{
	printf("%-10.10s \u2502 %-10.10s \u2502 %-10.10s\n",
	       "from_chars", "int32_t", "int64_t");
	for (unsigned int i = 0; i < 3 * 12 - 1; i++) {
		if (i % 13 == 11) {
			fputs("\u253c", stdout);
		} else {
			fputs("\u2500", stdout);
		}
	}
	putchar('\n');
	const size_t n = 1u << 23;
	const size_t maxsize = 64;
	char *mem = malloc(maxsize * n);
	char **strings = malloc(n * sizeof(strings[0]));
	for (size_t i = 0; i < n; i++) {
		strings[i] = mem + maxsize * i;
	}
	struct timespec start, end;
	for (unsigned int base = 0; base <= 36; base++) {
		if (base == 1) {
			continue;
		}
		printf("base %-5u \u2502 ", base);
		for (size_t i = 0; i < n; i++) {
			size_t len = to_chars(strings[i], maxsize - 1, i, base);
			assert(len < maxsize);
			strings[i][len] = '\0';
		}

		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start);
		for (size_t i = 0; i < n; i++) {
			int32_t val;
			struct from_chars_result res = from_chars(strings[i], maxsize, &val, base);
			if (!res.ok) {
				printf("\n%s %zu\n", strings[i], res.nchars);
			}
			assert(res.ok && val == (int32_t)i);
		}
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end);
		double t = elapsed(&start, &end);
		printf("%7.2f ns \u2502", t / n);

		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start);
		for (size_t i = 0; i < n; i++) {
			int64_t val;
			struct from_chars_result res = from_chars(strings[i], maxsize, &val, base);
			assert(res.ok && val == (int64_t)i);
		}
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end);
		t = elapsed(&start, &end);
		printf("%7.2f ns\n", t / n);
	}
	free(mem);
	free(strings);
}

int main(void)
{
	to_chars_benchmark();
	putchar('\n');
	from_chars_benchmark();
}
