#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include "heap.h"
#include "random.h"
#include "testing.h"

DEFINE_BINHEAP(intminheap, int, *a < *b)
DEFINE_BINHEAP(intmaxheap, int, *a > *b)

static bool random_test(int *arr, size_t n, struct random_state *rng)
{
	for (size_t i = 0; i < n; i++) {
		arr[i] = (int)random_next_u64_in_range(rng, 0, (3 * (n + 1)) / 4);
	}
	intminheap_heapify(arr, n);
	CHECK(intminheap_is_heap(arr, n));
	int last = INT_MIN;
	for (size_t i = 0; i < n; i++) {
		int min = intminheap_extract_first(arr, n - i);
		CHECK(last <= min);
		arr[n - 1 - i] = min;
		CHECK(intminheap_is_heap(arr, n - i - 1));
	}
	for (size_t i = 1; i < n; i++) {
		CHECK(arr[i - 1] >= arr[i]);
	}
	intminheap_heapify(arr, n);
	CHECK(intminheap_is_heap(arr, n));
	for (size_t i = 0; i < n; i++) {
		int min = arr[0];
		intminheap_delete_first(arr, n - i);
		arr[n - 1 - i] = min;
		CHECK(intminheap_is_heap(arr, n - i - 1));
	}
	for (size_t i = 1; i < n; i++) {
		CHECK(arr[i - 1] >= arr[i]);
	}
	CHECK(intmaxheap_is_heap(arr, n));
	intmaxheap_heapify(arr, n);
	// heapify should not change a sorted array
	for (size_t i = 1; i < n; i++) {
		CHECK(arr[i - 1] >= arr[i]);
	}
	for (size_t i = 0; i < n; i++) {
		intminheap_insert(arr, i);
		CHECK(intminheap_is_heap_until(arr, n) >= i + 1);
	}
	CHECK(intminheap_is_heap(arr, n));
	intminheap_sort(arr, n);
	for (size_t i = 1; i < n; i++) {
		CHECK(arr[i - 1] >= arr[i]);
	}
	intminheap_heapify(arr, n);
	for (size_t i = 0; i < n; i++) {
		int old = arr[i];
		arr[i] = (int)random_next_u64(rng);
		if (arr[i] > old) {
			intminheap_sift_down(arr, n, i);
		} else {
			intminheap_sift_up(arr, n, i);
		}
	}
	CHECK(intminheap_is_heap(arr, n));
	for (size_t i = 0; i < n; i++) {
		int idx = (int)random_next_u64_in_range(rng, 0, n - i - 1);
		intminheap_delete(arr, n - i, idx);
		CHECK(intminheap_is_heap(arr, n - i));
	}
	return true;
}

RANDOM_TEST(heap_random, random_seed, 4)
{
#define N 2048
	struct random_state rng;
	random_state_init(&rng, random_seed);
	int arr[N] = {0};
	for (size_t i = 0; i <= N; i++) {
		CHECK(random_test(arr, i, &rng));

		// make sure we don't touch any elements outside of the array
		for (size_t j = i; j < N; j++) {
			CHECK(arr[j] == 0);
		}
	}
	return true;
}

SIMPLE_TEST(heap)
{
	intminheap_heapify(NULL, 0);
	intminheap_sort(NULL, 0);
	CHECK(intminheap_is_heap(NULL, 0));
	CHECK(intminheap_is_heap_until(NULL, 0) == 0);

	CHECK(intminheap_is_heap_until((int[]){1, 2, 3, 4}, 4) == 4);
	CHECK(intminheap_is_heap_until((int[]){1, 2, 3, 0}, 4) == 3);
	CHECK(intminheap_is_heap_until((int[]){1, 2, 1, 4}, 4) == 4);
	CHECK(intminheap_is_heap_until((int[]){1, 2, 0, 4}, 4) == 2);
	CHECK(intminheap_is_heap_until((int[]){1, 0, 3, 4}, 4) == 1);
	CHECK(intminheap_is_heap_until((int[]){1, 1, 1, 1}, 4) == 4);
	CHECK(intminheap_is_heap_until((int[]){4, 3, 2, 1}, 4) == 1);
	CHECK(intminheap_is_heap_until((int[]){1}, 1) == 1);
	CHECK(intminheap_is_heap_until((int[]){1, 0}, 1) == 1);
	CHECK(intminheap_is_heap((int[]){1, 0}, 1));
	CHECK(!intminheap_is_heap((int[]){1, 0}, 2));

	return true;
}
