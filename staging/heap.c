#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "heap.h"

static size_t comparisons;

static
__attribute__((noinline))
// __attribute__((noipa))
bool int_less_than(int *a, int *b)
{
	comparisons++;
	return *a < *b;
}

DEFINE_MINHEAP(intheap, int, int_less_than(a, b))

static void test(int *arr, size_t n)
{
	for (size_t i = 0; i < n; i++) {
		int r = rand();
		arr[i] = r % ((3 * (n + 1)) / 4);
		if (r < 0) {
			arr[i] = -arr[i];
		}
	}
	intheap_heapify(arr, n);
	assert(intheap_is_heap(arr, n));
	int last = INT_MIN;
	for (size_t i = 0; i < n; i++) {
		int min = intheap_extract_min(arr, n - i);
		assert(last <= min);
		arr[n - 1 - i] = min;
	}
	for (size_t i = 1; i < n; i++) {
		assert(arr[i - 1] >= arr[i]);
	}
	for (size_t i = 0; i < n; i++) {
		intheap_insert(arr, i + 1);
		if (i % 16 == 0) {
			assert(intheap_is_heap_until(arr, i + 1));
		}
	}
	assert(intheap_is_heap(arr, n));
	for (size_t i = 0; i < n; i++) {
		int old = arr[i];
		arr[i] = rand();
		if (arr[i] > old) {
			intheap_increase_key(arr, n, i);
		} else {
			intheap_decrease_key(arr, n, i);
		}
	}
	assert(intheap_is_heap(arr, n));
}

static double elapsed(struct timespec *start, struct timespec *end)
{
	long s = end->tv_sec - start->tv_sec;
	long ns = end->tv_nsec - start->tv_nsec;
	return ns / 1000000000.0 + s;
}

int main(void)
{
#if 0
	struct timespec tp;
	clock_gettime(CLOCK_MONOTONIC, &tp);
	srand((unsigned int)tp.tv_nsec ^ (unsigned int)tp.tv_sec);
#define N (8 * 1024)
	int *arr = malloc(N * sizeof(arr[0]));
	for (size_t i = 0; i < N; i++) {
		test(arr, i);
		if (i % 512 == 0) {
			fprintf(stderr, "%zu\r", i);
		}
	}
	free(arr);
#else
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
	printf("heapify %.3fs\n", elapsed(&start, &end));
	printf("%zu comparisons\n", comparisons);

	comparisons = 0;
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start);
	size_t n2 = n / 2;
	for (size_t i = 0; i < n2; i++) {
		int old = arr[i];
		arr[i] = rand();
		if (arr[i] > old) {
			intheap_increase_key(arr, n2, i);
		} else {
			intheap_decrease_key(arr, n2, i);
		}
	}
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end);
	printf("change key %.3fs\n", elapsed(&start, &end));
	printf("%zu comparisons\n", comparisons);

	srand(12345);
	for (size_t i = 0; i < n; i++) {
		arr[i] = rand();
	}
	comparisons = 0;
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start);
	for (size_t i = 0; i < n; i++) {
		intheap_insert(arr, i + 1);
	}
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end);
	printf("insert %.3fs\n", elapsed(&start, &end));
	printf("%zu comparisons\n", comparisons);

	comparisons = 0;
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start);
	for (size_t i = 1; i <= n; i *= 2) {
		i = intheap_is_heap_until(arr, i);
	}
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end);
	printf("is heap %.3fs\n", elapsed(&start, &end));
	printf("%zu comparisons\n", comparisons);

	comparisons = 0;
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start);
	size_t n3 = n / 16;
	for (size_t i = 0; i < n3; i++) {
		arr[n3 - 1 - i] = intheap_extract_min(arr, n3 - i);
	}
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end);
	printf("extract %.3fs\n", elapsed(&start, &end));
	printf("%zu comparisons\n", comparisons);

	free(arr);
#endif
}
