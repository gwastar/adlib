#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "charconv.h"
#include "heap.h"

static size_t comparisons;

static double elapsed(struct timespec *start, struct timespec *end)
{
	long s = end->tv_sec - start->tv_sec;
	long ns = end->tv_nsec - start->tv_nsec;
	return ns / 1000000000.0 + s;
}

static
__attribute__((noinline))
// __attribute__((noipa))
bool int_less_than(const int *a, const int *b)
{
	comparisons++;
	return *a < *b;
}

DEFINE_BINHEAP(intheap, int, int_less_than(a, b))

static void intheap_benchmark(void)
{
	printf("[integer heap]\n");

	size_t n = 128 * 1024 * 1024;
	int *arr = malloc(n * sizeof(arr[0]));
	assert(arr);
	srand(12345);
	for (size_t i = 0; i < n; i++) {
		arr[i] = rand();
	}

	struct timespec start, end;
	comparisons = 0;
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start);
	intheap_heapify(arr, n);
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end);
	double t = elapsed(&start, &end);
	printf("heapify       %.2fs %.2fns/n %zu comparisons\n", t, 1000000000 * t / n, comparisons);

	assert(intheap_is_heap(arr, n));

	comparisons = 0;
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start);
	size_t n2 = n / 2;
	for (size_t i = 0; i < n2; i++) {
		int old = arr[i];
		arr[i] += rand() % 2 == 0 ? -1 : 1;
		if (arr[i] > old) {
			intheap_sift_down(arr, n2, i);
		} else {
			intheap_sift_up(arr, n2, i);
		}
	}
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end);
	t = elapsed(&start, &end);
	printf("change key    %.2fs %.2fns/n %zu comparisons\n", t, 1000000000 * t / n2, comparisons);

	srand(12345);
	for (size_t i = 0; i < n; i++) {
		arr[i] = rand();
	}

	comparisons = 0;
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start);
	for (size_t i = 0; i < n; i++) {
		intheap_insert(arr, i);
	}
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end);
	t = elapsed(&start, &end);
	printf("insert        %.2fs %.2fns/n %zu comparisons\n", t, 1000000000 * t / n, comparisons);

	comparisons = 0;
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start);
	for (size_t i = 1; i <= n; i *= 2) {
		size_t i2 = intheap_is_heap_until(arr, i);
		assert(i == i2);
	}
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end);
	t = elapsed(&start, &end);
	printf("is heap       %.2fs %.2fns/n %zu comparisons\n", t, 1000000000 * t / n, comparisons);

	comparisons = 0;
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start);
	size_t n3 = n / 16;
	for (size_t i = 0; i < n3; i++) {
		arr[n3 - 1 - i] = intheap_extract_first(arr, n3 - i);
	}
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end);
	t = elapsed(&start, &end);
	printf("extract       %.2fs %.2fns/n %zu comparisons\n", t, 1000000000 * t / n3, comparisons);

	intheap_heapify(arr, n);
	assert(intheap_is_heap(arr, n));

	comparisons = 0;
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start);
	for (size_t i = 0; i < n; i++) {
		intheap_delete(arr, n - i, (n - i) / 2);
	}
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end);
	t = elapsed(&start, &end);
	printf("delete        %.2fs %.2fns/n %zu comparisons\n", t, 1000000000 * t / n, comparisons);

	free(arr);
}

static
__attribute__((noinline))
// __attribute__((noipa))
bool string_less_than(char * const *a, char * const *b)
{
	comparisons++;
	return strcmp(*a, *b) < 0;
}

DEFINE_BINHEAP(stringheap, char *, string_less_than(a, b))

static void stringheap_benchmark(void)
{
	printf("[string heap]\n");
	size_t n = 32 * 1024 * 1024;
	char **arr = malloc(n * sizeof(arr[0]));
	char *strings = malloc(n * 32);
	assert(arr);
	srand(12345);
	for (size_t i = 0; i < n; i++) {
		arr[i] = strings + i * 32;
		to_chars(arr[i], rand(), 0);
	}

	struct timespec start, end;
	comparisons = 0;
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start);
	stringheap_heapify(arr, n);
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end);
	double t = elapsed(&start, &end);
	printf("heapify       %.2fs %.2fns/n %zu comparisons\n", t, 1000000000 * t / n, comparisons);

	assert(stringheap_is_heap(arr, n));

#if 0
	comparisons = 0;
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start);
	size_t n2 = n / 2;
	for (size_t i = 0; i < n2; i++) {
		char *old = arr[i];
		arr[i] += rand() % 2 == 0 ? -1 : 1;
		if (arr[i] > old) {
			stringheap_sift_down(arr, n2, i);
		} else {
			stringheap_sift_up(arr, n2, i);
		}
	}
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end);
	t = elapsed(&start, &end);
	printf("change key    %.2fs %.2fns/n %zu comparisons\n", t, 1000000000 * t / n2, comparisons);
#endif

	for (size_t i = 0; i < n; i++) {
		arr[i] = strings + i * 32;
	}

	comparisons = 0;
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start);
	for (size_t i = 0; i < n; i++) {
		stringheap_insert(arr, i);
	}
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end);
	t = elapsed(&start, &end);
	printf("insert        %.2fs %.2fns/n %zu comparisons\n", t, 1000000000 * t / n, comparisons);

	comparisons = 0;
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start);
	for (size_t i = 1; i <= n; i *= 2) {
		size_t i2 = stringheap_is_heap_until(arr, i);
		assert(i == i2);
	}
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end);
	t = elapsed(&start, &end);
	printf("is heap       %.2fs %.2fns/n %zu comparisons\n", t, 1000000000 * t / n, comparisons);

	comparisons = 0;
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start);
	size_t n3 = n / 16;
	for (size_t i = 0; i < n3; i++) {
		arr[n3 - 1 - i] = stringheap_extract_first(arr, n3 - i);
	}
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end);
	t = elapsed(&start, &end);
	printf("extract       %.2fs %.2fns/n %zu comparisons\n", t, 1000000000 * t / n3, comparisons);

	stringheap_heapify(arr, n);
	assert(stringheap_is_heap(arr, n));

	comparisons = 0;
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start);
	for (size_t i = 0; i < n; i++) {
		stringheap_delete(arr, n - i, (n - i) / 2);
	}
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end);
	t = elapsed(&start, &end);
	printf("delete        %.2fs %.2fns/n %zu comparisons\n", t, 1000000000 * t / n, comparisons);

	free(arr);
	free(strings);
}

struct s {
	int64_t ints[16];
};

static
__attribute__((noinline))
// __attribute__((noipa))
bool struct_less_than(const struct s *a, const struct s *b)
{
	comparisons++;
	int64_t sum_a = 0;
	int64_t sum_b = 0;
	for (size_t i = 0; i < sizeof(a->ints) / sizeof(a->ints[0]); i++) {
		sum_a += a->ints[i];
		sum_b += b->ints[i];
	}
	return sum_a < sum_b;
}

DEFINE_BINHEAP(structheap, struct s, struct_less_than(a, b))

static void structheap_benchmark(void)
{
	printf("[struct heap]\n");

	size_t n = 32 * 1024 * 1024;
	struct s *arr = malloc(n * sizeof(arr[0]));
	assert(arr);
	srand(12345);
	for (size_t i = 0; i < n; i++) {
		for (size_t j = 0; j < sizeof(arr[i].ints) / sizeof(arr[i].ints[0]); j++) {
			arr[i].ints[j] = rand();
		}
	}

	struct timespec start, end;
	comparisons = 0;
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start);
	structheap_heapify(arr, n);
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end);
	double t = elapsed(&start, &end);
	printf("heapify       %.2fs %.2fns/n %zu comparisons\n", t, 1000000000 * t / n, comparisons);

	assert(structheap_is_heap(arr, n));

	comparisons = 0;
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start);
	size_t n2 = n / 2;
	for (size_t i = 0; i < n2; i++) {
		int64_t old = arr[i].ints[0];
		arr[i].ints[0] += rand() % 2 == 0 ? -1 : 1;
		if (arr[i].ints[0] > old) {
			structheap_sift_down(arr, n2, i);
		} else {
			structheap_sift_up(arr, n2, i);
		}
	}
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end);
	t = elapsed(&start, &end);
	printf("change key    %.2fs %.2fns/n %zu comparisons\n", t, 1000000000 * t / n2, comparisons);

	srand(12345);
	for (size_t i = 0; i < n; i++) {
		for (size_t j = 0; j < sizeof(arr[i].ints) / sizeof(arr[i].ints[0]); j++) {
			arr[i].ints[j] = rand();
		}
	}
	comparisons = 0;
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start);
	for (size_t i = 0; i < n; i++) {
		structheap_insert(arr, i);
	}
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end);
	t = elapsed(&start, &end);
	printf("insert        %.2fs %.2fns/n %zu comparisons\n", t, 1000000000 * t / n, comparisons);

	comparisons = 0;
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start);
	for (size_t i = 1; i <= n; i *= 2) {
		size_t i2 = structheap_is_heap_until(arr, i);
		assert(i == i2);
	}
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end);
	t = elapsed(&start, &end);
	printf("is heap       %.2fs %.2fns/n %zu comparisons\n", t, 1000000000 * t / n, comparisons);

	comparisons = 0;
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start);
	size_t n3 = n / 16;
	for (size_t i = 0; i < n3; i++) {
		arr[n3 - 1 - i] = structheap_extract_first(arr, n3 - i);
	}
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end);
	t = elapsed(&start, &end);
	printf("extract       %.2fs %.2fns/n %zu comparisons\n", t, 1000000000 * t / n3, comparisons);

	structheap_heapify(arr, n);
	assert(structheap_is_heap(arr, n));

	comparisons = 0;
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start);
	for (size_t i = 0; i < n; i++) {
		structheap_delete(arr, n - i, (n - i) / 2);
	}
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end);
	t = elapsed(&start, &end);
	printf("delete        %.2fs %.2fns/n %zu comparisons\n", t, 1000000000 * t / n, comparisons);

	free(arr);
}

int main(void)
{
	intheap_benchmark();
	puts("");
	stringheap_benchmark();
	puts("");
	structheap_benchmark();
}
