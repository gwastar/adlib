#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "array.h"
#include "hashtable.h"
#include "random.h"
#include "testing.h"

static inline uint32_t integer_hash(uint32_t x)
{
	x ^= x >> 16;
	x *= 0x7feb352d;
	x ^= x >> 15;
	x *= 0x846ca68b;
	x ^= x >> 16;
	return x;
}

static int cmp_int(const void *a, const void *b)
{
	return *(const int *)a - *(const int *)b;
}

DEFINE_HASHTABLE(itable, int, int, 8, (*key == *entry))

RANDOM_TEST(hashset, 2)
{
	struct itable itable;
	itable_init(&itable, 16);
	int *arr = NULL;

	struct random_state rng;
	random_state_init(&rng, random_seed);

	for (unsigned long counter = 0; counter < 100000; counter++) {
		int r = random_next_u32(&rng) % 128;
		if (r < 100) {
			int x = random_next_u32(&rng) % (1 << 20);
			bool found = false;
			array_foreach_value(arr, it) {
				if (it == x) {
					found = true;
					break;
				}
			}
			int *entry = itable_lookup(&itable, x, integer_hash(x));
			if (entry) {
				CHECK(found);
				CHECK(*entry == x);
			} else {
				CHECK(!found);
				entry = itable_insert(&itable, x, integer_hash(x));
				*entry = x;
				array_add(arr, x);
			}
		} else if (array_length(arr) != 0) {
			int key;
			int idx = random_next_u32(&rng) % array_length(arr);
			int x = arr[idx];
			bool removed = itable_remove(&itable, x, integer_hash(x), &key);
			CHECK(removed);
			array_fast_delete(arr, idx);
		}

		if (counter % 4096 == 0) {
			array_foreach_value(arr, i) {
				CHECK(itable_lookup(&itable, i, integer_hash(i)));
			}
			int *arr2 = NULL;
			array_reserve(arr2, array_length(arr));
			for (itable_iter_t iter = itable_iter_start(&itable);
			     !itable_iter_finished(&iter);
			     itable_iter_advance(&iter)) {
				array_add(arr2, *iter.entry);
			}
			array_sort(arr, cmp_int);
			array_sort(arr2, cmp_int);
			CHECK(array_equal(arr, arr2));
			array_free(arr2);
			// fprintf(stderr, "%zu %lu\r", array_length(arr), counter);
		}
	}

	itable_destroy(&itable);
	array_free(arr);

	return true;
}
