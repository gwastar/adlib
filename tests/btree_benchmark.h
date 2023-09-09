#pragma once

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "btree.h"
#include "random.h"

// #define STRING_MAP

#ifdef STRING_MAP
DEFINE_BTREE_MAP(btree, char *, uint32_t, NULL, NULL, 128, strcmp(a, b))
#else
DEFINE_BTREE_SET(btree, int64_t, NULL, 127, (a < b) ? -1 : (a > b))
#endif

#ifdef STRING_MAP
static char **keys;
static size_t num_keys;
#define __KEY_SIZE 15
#define KEY_SIZE (__KEY_SIZE + 1)
#define KEY_FORMAT __KEY_FORMAT(__KEY_SIZE)
#define __KEY_FORMAT(S) ___KEY_FORMAT(S)
#define ___KEY_FORMAT(S) "%0" #S "zu"

static void init_keys(size_t N)
{
	num_keys = N;
	keys = malloc(N * sizeof(keys[0]));
	for (size_t i = 0; i < N; i++) {
		keys[i] = malloc(KEY_SIZE);
		snprintf(keys[i], KEY_SIZE, KEY_FORMAT, i);
	}
}

static void destroy_keys(void)
{
	for (size_t i = 0; i < num_keys; i++) {
		free(keys[i]);
	}
	free(keys);
	num_keys = 0;
	keys = NULL;
}

static char *get_key(size_t x)
{
	assert(x < num_keys);
	return keys[x];
}

static char **random_keys;
static size_t num_random_keys;

static void init_random_keys(uint32_t *random_numbers, size_t N)
{
	num_random_keys = N;
	random_keys = malloc(N * sizeof(random_keys[0]));
	for (size_t i = 0; i < N; i++) {
		random_keys[i] = malloc(KEY_SIZE);
		snprintf(random_keys[i], KEY_SIZE, KEY_FORMAT, (size_t)random_numbers[i]);
	}
}

static void destroy_random_keys(void)
{
	for (size_t i = 0; i < num_random_keys; i++) {
		free(random_keys[i]);
	}
	free(random_keys);
	num_random_keys = 0;
	random_keys = NULL;
}

static char *get_random_key(size_t i)
{
	assert(i < num_random_keys);
	return random_keys[i];
}
#else
static uint32_t *random_keys;
static size_t num_keys;
static size_t num_random_keys;
static void init_keys(size_t N)
{
	num_keys = N;
}
static void destroy_keys(void)
{
	num_keys = 0;
}
static btree_key_t get_key(size_t x)
{
	assert(x < num_keys);
	return (btree_key_t)x;
}
static void init_random_keys(uint32_t *random_numbers, size_t N)
{
	num_random_keys = N;
	random_keys = random_numbers;
}
static void destroy_random_keys(void)
{
	random_keys = NULL;
	num_random_keys = 0;
}
static btree_key_t get_random_key(size_t i)
{
	assert(i < num_random_keys);
	return (btree_key_t)random_keys[i];
}
#endif

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
	return (a < b) ? -1 : (a > b);
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

static size_t next_pow2(size_t x)
{
	x--;
	x |= x >> 1;
	x |= x >> 2;
	x |= x >> 4;
	x |= x >> 8;
	x |= x >> 16;
	x |= x >> (sizeof(x) * 4);
	x++;
	return x;
}

#define DO_NOT_OPTIMIZE(var) asm volatile("" :: "g" (var) : "memory")

int main(void)
{
#ifdef STRING_MAP
	const size_t N = 1 << 16;
#else
	const size_t N = 1 << 18;
#endif

#define ITERATIONS 15
	double inorder_fast_insert[ITERATIONS];
	double inorder_insert[ITERATIONS];
	double revorder_insert[ITERATIONS];
	double random_insert[ITERATIONS];
	double inorder_find[ITERATIONS];
	double revorder_find[ITERATIONS];
	double randorder_find[ITERATIONS];
	double random_find[ITERATIONS];
	double iteration[ITERATIONS];
	double inorder_deletion[ITERATIONS];
	double revorder_deletion[ITERATIONS];
	double randorder_deletion[ITERATIONS];
	double destruction[ITERATIONS];
	double inorder_mixed[ITERATIONS];
	double revorder_mixed[ITERATIONS];
	double random_mixed[ITERATIONS];

	uint32_t *random_numbers = malloc(2 * N * sizeof(random_numbers[0]));
	uint32_t *random_numbers2 = random_numbers + N;
	{
		struct random_state rng;
		random_state_init(&rng, 0xdeadbeef);
		for (size_t i = 0; i < 2 * N; i++) {
			random_numbers[i] = random_next_u32(&rng);
		}
	}

	init_keys(2 * N);
	init_random_keys(random_numbers, 2 * N);

	for (size_t k = 0; k < ITERATIONS; k++) {
		struct btree btree;
		btree_init(&btree);

		struct timespec start_tp, end_tp;

		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp);
		for (size_t i = 0; i < N; i++) {
			btree_key_t key = get_key(i);
#ifdef STRING_MAP
			btree_insert(&btree, key, i);
#else
			btree_insert(&btree, key);
#endif
		}
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);
		inorder_insert[k] = ns_elapsed(start_tp, end_tp);

		btree_destroy(&btree);

		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp);
		for (size_t i = 0; i < N; i++) {
			btree_key_t key = get_key(i);
#ifdef STRING_MAP
			btree_insert_sequential(&btree, key, i);
#else
			btree_insert_sequential(&btree, key);
#endif
		}
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);
		inorder_fast_insert[k] = ns_elapsed(start_tp, end_tp);

		btree_destroy(&btree);

		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp);
		for (size_t i = 0; i < N; i++) {
			size_t x = N - 1 - i;
			btree_key_t key = get_key(x);
#ifdef STRING_MAP
			btree_insert(&btree, key, x);
#else
			btree_insert(&btree, key);
#endif
		}
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);
		revorder_insert[k] = ns_elapsed(start_tp, end_tp);

		btree_destroy(&btree);

		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp);
		for (size_t i = 0; i < N; i++) {
			btree_key_t key = get_random_key(i);
#ifdef STRING_MAP
			btree_insert(&btree, key, random_numbers[i]);
#else
			btree_insert(&btree, key);
#endif
		}
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);
		random_insert[k] = ns_elapsed(start_tp, end_tp);


		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp);
		for (size_t i = 0; i < N; i++) {
			btree_key_t key = get_random_key(i);
			bool found = btree_find(&btree, key);
			DO_NOT_OPTIMIZE(found);
		}
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);
		inorder_find[k] = ns_elapsed(start_tp, end_tp);

		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp);
		for (size_t i = 0; i < N; i++) {
			btree_key_t key = get_random_key(N - 1 - i);
			bool found = btree_find(&btree, key);
			DO_NOT_OPTIMIZE(found);
		}
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);
		revorder_find[k] = ns_elapsed(start_tp, end_tp);

		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp);
		for (size_t i = 0; i < N; i++) {
			btree_key_t key = get_random_key(random_numbers2[i] % N);
			bool found = btree_find(&btree, key);
			DO_NOT_OPTIMIZE(found);
		}
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);
		randorder_find[k] = ns_elapsed(start_tp, end_tp);

		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp);
		for (size_t i = 0; i < N; i++) {
			btree_key_t key = get_random_key(N + i);
			bool found = btree_find(&btree, key);
			DO_NOT_OPTIMIZE(found);
		}
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);
		random_find[k] = ns_elapsed(start_tp, end_tp);

		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp);
		{
			btree_iter_t iter;
#ifdef STRING_MAP
			btree_key_t prev_key, key;
			btree_value_t *value = btree_iter_start_leftmost(&iter, &btree, &prev_key);
			while ((value = btree_iter_next(&iter, &key))) {
				assert(btree_info.cmp(&prev_key, &key) < 0);
				prev_key = key;
			}
#else
			const btree_key_t *prev_key, *key;
			prev_key = btree_iter_start_leftmost(&iter, &btree);
			while ((key = btree_iter_next(&iter))) {
				assert(btree_info.cmp(prev_key, key) < 0);
				prev_key = key;
			}
#endif
		}
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);
		iteration[k] = ns_elapsed(start_tp, end_tp);

		struct btree copy = {._impl = _btree_debug_copy(&btree._impl, &btree_info)};

		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp);
		for (size_t i = 0; i < N; i++) {
			btree_key_t key = get_random_key(i);
#ifdef STRING_MAP
			bool found = btree_delete(&btree, key, &key, NULL);
#else
			bool found = btree_delete(&btree, key, &key);
#endif
			DO_NOT_OPTIMIZE(found);
		}
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);
		inorder_deletion[k] = ns_elapsed(start_tp, end_tp);

		btree = copy;
		copy = (struct btree){._impl = _btree_debug_copy(&btree._impl, &btree_info)};

		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp);
		for (size_t i = 0; i < N; i++) {
			btree_key_t key = get_random_key(N - 1 - i);
#ifdef STRING_MAP
			bool found = btree_delete(&btree, key, &key, NULL);
#else
			bool found = btree_delete(&btree, key, &key);
#endif
			DO_NOT_OPTIMIZE(found);
		}
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);
		revorder_deletion[k] = ns_elapsed(start_tp, end_tp);

		btree = copy;
		copy = (struct btree){._impl = _btree_debug_copy(&btree._impl, &btree_info)};

		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp);
		for (size_t i = 0; i < N; i++) {
			btree_key_t key = get_random_key(random_numbers2[i] % N);
#ifdef STRING_MAP
			bool found = btree_delete(&btree, key, &key, NULL);
#else
			bool found = btree_delete(&btree, key, &key);
#endif
			DO_NOT_OPTIMIZE(found);
		}
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);
		randorder_deletion[k] = ns_elapsed(start_tp, end_tp);

		btree_destroy(&btree);
		btree = copy;

		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp);
		btree_destroy(&btree);
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);
		destruction[k] = ns_elapsed(start_tp, end_tp);

		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp);
		for (size_t i = 0; i < N / 4; i++) {
			const size_t INSERTIONS = 5;
			const size_t FINDS = 10;
			const size_t DELETIONS = 2;
			for (size_t j = 0; j < INSERTIONS; j++) {
				size_t z = (i * INSERTIONS + j) % (2 * N);
				btree_key_t key = get_key(z);
#ifdef STRING_MAP
				btree_insert(&btree, key, z);
#else
				btree_insert(&btree, key);
#endif
			}
			size_t n = next_pow2((i + 1) * INSERTIONS);
			if (n > 2 * N) {
				n = 2 * N;
			}
			for (size_t j = 0; j < FINDS; j++) {
				btree_key_t key = get_key((i * FINDS + j) & (n - 1));
				bool found = btree_find(&btree, key);
				DO_NOT_OPTIMIZE(found);
			}
			for (size_t j = 0; j < DELETIONS; j++) {
				btree_key_t key = get_key((i * DELETIONS + 2 * j) & (n - 1));
#ifdef STRING_MAP
				btree_delete(&btree, key, &key, NULL);
#else
				btree_delete(&btree, key, &key);
#endif
			}
		}
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);
		inorder_mixed[k] = ns_elapsed(start_tp, end_tp);

		btree_destroy(&btree);

		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp);
		for (size_t i = 0; i < N / 4; i++) {
			const size_t INSERTIONS = 5;
			const size_t FINDS = 10;
			const size_t DELETIONS = 2;
			for (size_t j = 0; j < INSERTIONS; j++) {
				size_t z = 2 * N - 1 - ((i * INSERTIONS + j) % (2 * N));
				btree_key_t key = get_key(z);
#ifdef STRING_MAP
				btree_insert(&btree, key, z);
#else
				btree_insert(&btree, key);
#endif
			}
			size_t n = next_pow2((i + 1) * INSERTIONS);
			if (n > 2 * N) {
				n = 2 * N;
			}
			for (size_t j = 0; j < FINDS; j++) {
				btree_key_t key = get_key(2 * N - 1 - ((i * FINDS + j) & (n - 1)));
				bool found = btree_find(&btree, key);
				DO_NOT_OPTIMIZE(found);
			}
			for (size_t j = 0; j < DELETIONS; j++) {
				btree_key_t key = get_key(2 * N - 1 - ((i * DELETIONS + 2 * j) & (n - 1)));
#ifdef STRING_MAP
				btree_delete(&btree, key, &key, NULL);
#else
				btree_delete(&btree, key, &key);
#endif
			}
		}
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);
		revorder_mixed[k] = ns_elapsed(start_tp, end_tp);

		btree_destroy(&btree);

		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp);
		for (size_t i = 0; i < N / 4; i++) {
			const size_t INSERTIONS = 5;
			const size_t FINDS = 10;
			const size_t DELETIONS = 2;
			for (size_t j = 0; j < INSERTIONS; j++) {
				size_t z = (i * INSERTIONS + j) % (2 * N);
				btree_key_t key = get_random_key(z);
#ifdef STRING_MAP
				btree_insert(&btree, key, random_numbers[z]);
#else
				btree_insert(&btree, key);
#endif
			}
			size_t n = next_pow2((i + 1) * INSERTIONS);
			if (n > 2 * N) {
				n = 2 * N;
			}
			for (size_t j = 0; j < FINDS; j++) {
				btree_key_t key = get_random_key((i * FINDS + j) & (n - 1));
				bool found = btree_find(&btree, key);
				DO_NOT_OPTIMIZE(found);
			}
			for (size_t j = 0; j < DELETIONS; j++) {
				btree_key_t key = get_random_key((i * DELETIONS + 2 * j) & (n - 1));
#ifdef STRING_MAP
				btree_delete(&btree, key, &key, NULL);
#else
				btree_delete(&btree, key, &key);
#endif
			}
		}
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);
		random_mixed[k] = ns_elapsed(start_tp, end_tp);

		btree_destroy(&btree);
	}
	// TODO print more statistics or maybe just print minimum instead
	double t;
	t = get_median(inorder_insert, ITERATIONS);
	printf("%-32s %8.1f ns\n", "in-order insertion", t / N);
	t = get_median(inorder_fast_insert, ITERATIONS);
	printf("%-32s %8.1f ns\n", "in-order insertion (fastpath)", t / N);
	t = get_median(revorder_insert, ITERATIONS);
	printf("%-32s %8.1f ns\n", "reverse-order insertion", t / N);
	t = get_median(random_insert, ITERATIONS);
	printf("%-32s %8.1f ns\n", "random insertion", t / N);
	t = get_median(inorder_find, ITERATIONS);
	printf("%-32s %8.1f ns\n", "in-order find", t / N);
	t = get_median(revorder_find, ITERATIONS);
	printf("%-32s %8.1f ns\n", "reverse-order find", t / N);
	t = get_median(randorder_find, ITERATIONS);
	printf("%-32s %8.1f ns\n", "random-order find", t / N);
	t = get_median(random_find, ITERATIONS);
	printf("%-32s %8.1f ns\n", "random find", t / N);
	t = get_median(iteration, ITERATIONS);
	printf("%-32s %8.1f ns\n", "iteration", t / N);
	t = get_median(inorder_deletion, ITERATIONS);
	printf("%-32s %8.1f ns\n", "in-order deletion", t / N);
	t = get_median(revorder_deletion, ITERATIONS);
	printf("%-32s %8.1f ns\n", "reverse-order deletion", t / N);
	t = get_median(randorder_deletion, ITERATIONS);
	printf("%-32s %8.1f ns\n", "random-order deletion", t / N);
	t = get_median(destruction, ITERATIONS);
	printf("%-32s %8.1f ns\n", "destruction", t / N);
	t = get_median(inorder_mixed, ITERATIONS);
	printf("%-32s %8.1f ns\n", "in-order mixed", t / (N / 4));
	t = get_median(revorder_mixed, ITERATIONS);
	printf("%-32s %8.1f ns\n", "reverse-order mixed", t / (N / 4));
	t = get_median(random_mixed, ITERATIONS);
	printf("%-32s %8.1f ns\n", "random-order mixed", t / (N / 4));

	destroy_keys();
	destroy_random_keys();
	free(random_numbers);
}
