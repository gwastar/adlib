#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include "array.h"
#include "config.h"
#include "compiler.h"
#include "testing.h"

#ifdef HAVE_MALLOC_USABLE_SIZE
#include <malloc.h>
#endif

// TODO test fortify failures

static _attr_unused void print_array(int *arr, bool print_reverse)
{
	size_t len = array_length(arr);
	size_t limit = array_capacity(arr);
	test_log("{\n\tlen = %zu,\n\tlimit = %zu,\n\tval = {", len, limit);
	size_t i = 0;
	array_foreach(arr, cur) {
		test_log("%i", *cur);
		if (i != len - 1) {
			test_log(", ");
		}
		i++;
	}

	if (print_reverse) {
		test_log("},\n\treverse = {");
		array_foreach_reverse(arr, cur) {
			test_log("%i", *cur);
			i--;
			if (i != 0) {
				test_log(", ");
			}
		}
		assert(i == 0);
	}

	test_log("}\n}\n");
}

static bool check_array_content(size_t length, int *arr, ...)
{
	CHECK(((uintptr_t)arr) % _Alignof(max_align_t) == sizeof(_arr) % _Alignof(max_align_t));
	CHECK(array_length(arr) == length);
	CHECK(array_capacity(arr) >= length);
#ifdef HAVE_MALLOC_USABLE_SIZE
	CHECK(malloc_usable_size((char *)arr - sizeof(_arr)) >= array_capacity(arr) * sizeof(arr[0]));
#endif

	va_list args;
	va_start(args, arr);
	array_foreach(arr, cur) {
		CHECK(*cur == va_arg(args, int));
	}
	va_end(args);
	return true;
}

static int cmp(const void *a, const void *b)
{
	return *(const int *)a - *(const int *)b;
}

static void increment(int *i)
{
	(*i)++;
}

static int global_sum = 0;
static void sum(int i)
{
	global_sum += i;
}

static void add_a_one(array_t(int) *some_array)
{
	array_t(int) *arr = some_array;
	array_add(*arr, 1);
}

SIMPLE_TEST(array)
{
	int *arr1 = NULL;
	add_a_one(&arr1);
	// array_push(arr1, 1);
	array_push(arr1, 2);
	array_push(arr1, 3);
	array_push(arr1, 4);
	array_push(arr1, 5);

	array_insert(arr1, 0, 0);
	array_insert(arr1, 0, 0);
	array_insert(arr1, 1, 1);
	array_insert(arr1, 2, 2);
	array_insert(arr1, 3, 3);
	array_insert(arr1, 4, 4);
	array_insert(arr1, 5, 5);

	int *arr2 = array_copy(arr1);
	CHECK(check_array_content(12, arr2, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5));
	array_shrink_to_fit(arr2);

	CHECK(arr2[array_lasti(arr2)] == 5);
	CHECK(arr2[array_lasti(arr2)] == array_last(arr2));

	CHECK(array_index_of(arr1, &arr1[3]) == 3);

	int i;
	i = array_pop(arr1);
	CHECK(i == 5);
	i = array_pop(arr1);
	CHECK(i == 4);
	i = array_pop(arr1);
	CHECK(i == 3);
	i = array_pop(arr1);
	CHECK(i == 2);
	i = array_pop(arr1);
	CHECK(i == 1);
	i = array_pop(arr1);
	CHECK(i == 0);
	CHECK(check_array_content(6, arr1, 0, 1, 2, 3, 4, 5));
	array_shrink_to_fit(arr1);

	array_make_valid(arr1, 1);
	CHECK(check_array_content(6, arr1, 0, 1, 2, 3, 4, 5));
	array_make_valid(arr1, 7);
	arr1[6] = 6;
	arr1[7] = 7;
	CHECK(check_array_content(8, arr1, 0, 1, 2, 3, 4, 5, 6, 7));

	array_addn(arr2, 15);
	size_t len = array_length(arr2);
	array_popn(arr2, 10);
	CHECK(array_length(arr2) == len - 10);

	array_clear(arr2);
	CHECK(array_length(arr2) == 0);
	array_shrink_to_fit(arr2);
	CHECK(array_length(arr2) == 0);
	CHECK(array_capacity(arr2) == 0);

	CHECK(check_array_content(8, arr1, 0, 1, 2, 3, 4, 5, 6, 7));
	array_ordered_deleten(arr1, 2, 1);
	CHECK(check_array_content(7, arr1, 0, 1, 3, 4, 5, 6, 7));
	array_fast_deleten(arr1, 2, 1);
	CHECK(check_array_content(6, arr1, 0, 1, 7, 4, 5, 6));

	array_ordered_delete(arr1, 0);
	CHECK(check_array_content(5, arr1, 1, 7, 4, 5, 6));
	array_fast_delete(arr1, 0);
	CHECK(check_array_content(4, arr1, 6, 7, 4, 5));

	array_resize(arr1, 4);
	CHECK(check_array_content(4, arr1, 6, 7, 4, 5));

	array_free(arr1);
	array_insertn(arr1, 0, 10);
	CHECK(array_capacity(arr1) >= 10);
	array_free(arr1);

	array_reserve(arr1, 1);
	CHECK(array_capacity(arr1) >= 1);
	CHECK(array_length(arr1) == 0);
	array_reserve(arr1, 5);
	CHECK(array_capacity(arr1) >= 5);
	CHECK(array_length(arr1) == 0);
	array_add(arr1, 0);
	array_add(arr1, 0);
	array_add(arr1, 0);
	array_add(arr1, 0);
	array_add(arr1, 0);
	CHECK(check_array_content(5, arr1, 0, 0, 0, 0, 0));
	array_free(arr1);

	int digits[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
	array_add_arrayn(arr1, digits, sizeof(digits) / sizeof(digits[0]));
	arr2 = array_copy(arr1);
	array_add_array(arr1, arr2);
	CHECK(check_array_content(20, arr1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9));
	array_truncate(arr1, 10);
	CHECK(check_array_content(10, arr1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9));
	array_free(arr1);

	array_reverse(arr2);
	CHECK(check_array_content(10, arr2, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0));
	array_sort(arr2, cmp);
	CHECK(check_array_content(10, arr2, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9));

	array_shuffle(arr2, (size_t(*)(void))random);
	array_shuffle(arr2, (size_t(*)(void))random);
	array_sort(arr2, cmp);
	CHECK(check_array_content(10, arr2, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9));
	array_free(arr2);

	array_add_arrayn(arr1, digits, sizeof(digits) / sizeof(digits[0]));
	array_shuffle(arr1, (size_t(*)(void))random);
	array_foreach_value(arr1, v) {
		array_insert_sorted(arr2, v, cmp);
	}
	CHECK(check_array_content(10, arr2, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9));
	array_shuffle(arr1, (size_t(*)(void))random);
	array_foreach_value(arr1, v) {
		array_insert_sorted(arr2, v, cmp);
	}
	CHECK(check_array_content(20, arr2, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9));
	array_foreach(arr1, it) {
		*it += 10;
	}
	array_shuffle(arr1, (size_t(*)(void))random);
	array_foreach_value(arr1, v) {
		array_insert_sorted(arr2, v, cmp);
	}
	CHECK(check_array_content(30, arr2, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9,
				  10, 11, 12, 13, 14, 15, 16, 17, 18, 19));
	array_shuffle(arr1, (size_t(*)(void))random);
	array_foreach_value(arr1, v) {
		array_insert_sorted(arr2, v, cmp);
	}
	CHECK(check_array_content(40, arr2, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9,
				  10, 10, 11, 11, 12, 12, 13, 13, 14, 14, 15, 15, 16, 16, 17, 17, 18, 18, 19, 19));

	array_foreach(arr1, it) {
		*it -= 20;
	}
	array_shuffle(arr1, (size_t(*)(void))random);
	array_foreach_value(arr1, v) {
		array_insert_sorted(arr2, v, cmp);
	}
	CHECK(check_array_content(50, arr2, -10, -9, -8, -7, -6, -5, -4, -3, -2, -1,
				  0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9,
				  10, 10, 11, 11, 12, 12, 13, 13, 14, 14, 15, 15, 16, 16, 17, 17, 18, 18, 19, 19));

	array_shuffle(arr1, (size_t(*)(void))random);
	array_foreach_value(arr1, v) {
		array_insert_sorted(arr2, v, cmp);
	}
	CHECK(check_array_content(60, arr2,
				  -10, -10, -9, -9, -8, -8, -7, -7, -6, -6, -5, -5, -4, -4, -3, -3, -2, -2, -1, -1,
				  0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9,
				  10, 10, 11, 11, 12, 12, 13, 13, 14, 14, 15, 15, 16, 16, 17, 17, 18, 18, 19, 19));

	for (int i = -10; i < 20; i++) {
		CHECK(array_bsearch(arr2, &i, cmp));
	}

	array_free(arr1);
	array_free(arr2);

	array_insert_sorted(arr1, 49, cmp);
	array_insert_sorted(arr1, 50, cmp);
	array_insert_sorted(arr1, 51, cmp);

	array_insert_sorted(arr1, 99, cmp);
	array_insert_sorted(arr1, 101, cmp);
	array_insert_sorted(arr1, 100, cmp);

	array_insert_sorted(arr1, 150, cmp);
	array_insert_sorted(arr1, 149, cmp);
	array_insert_sorted(arr1, 151, cmp);

	array_insert_sorted(arr1, 200, cmp);
	array_insert_sorted(arr1, 201, cmp);
	array_insert_sorted(arr1, 199, cmp);

	array_insert_sorted(arr1, 251, cmp);
	array_insert_sorted(arr1, 249, cmp);
	array_insert_sorted(arr1, 250, cmp);

	array_insert_sorted(arr1, 301, cmp);
	array_insert_sorted(arr1, 300, cmp);
	array_insert_sorted(arr1, 299, cmp);

	CHECK(check_array_content(18, arr1, 49, 50, 51, 99, 100, 101, 149, 150, 151, 199, 200, 201,
				  249, 250, 251, 299, 300, 301));

	for (int i = 0; i < 500; i++) {
		int *p = array_bsearch(arr1, &i, cmp);
		switch (i) {
		case 49:
		case 50:
		case 51:
		case 99:
		case 100:
		case 101:
		case 149:
		case 150:
		case 151:
		case 199:
		case 200:
		case 201:
		case 249:
		case 250:
		case 251:
		case 299:
		case 300:
		case 301:
			CHECK(p);
			CHECK(*p == i);
			break;
		default:
			CHECK(!p);
			break;
		}
	}

	array_free(arr1);

	array_add_arrayn(arr1, digits, sizeof(digits) / sizeof(digits[0]));
	array_fori(arr1, i) {
		CHECK(array_index_of(arr1, &arr1[i]) == i);
		CHECK((size_t)arr1[i] == i);
	}
	array_fori_reverse(arr1, i) {
		CHECK(array_index_of(arr1, &arr1[i]) == i);
		CHECK((size_t)arr1[i] == i);
	}
	array_free(arr1);

	array_add_arrayn(arr1, digits, 10);
	int *dest = array_addn(arr2, 10);
	memcpy(dest, digits, 10 * sizeof(int));
	bool equal = array_equal(arr1, arr2);
	CHECK(equal);
	arr2[0] = 1;
	equal = array_equal(arr1, arr2);
	CHECK(!equal);
	array_ordered_delete(arr2, 0);
	equal = array_equal(arr1, arr2);
	CHECK(!equal);
	array_insert(arr2, 0, 0);
	equal = array_equal(arr1, arr2);
	CHECK(equal);
	array_free(arr1);
	array_free(arr2);

	array_add_arrayn(arr1, digits, sizeof(digits) / sizeof(digits[0]));
	array_fori_reverse(arr1, i) {
		array_add(arr2, arr1[i]);
	}
	array_reverse(arr2);
	CHECK(check_array_content(10, arr2, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9));
	array_fori(arr2, i) {
		CHECK(arr2[i] == (int)i);
	}
	array_clear(arr2);

	{
		array_foreach_reverse(arr1, it) {
			array_add(arr2, *it);
		}
	}

	array_reverse(arr2);
	CHECK(check_array_content(10, arr2, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9));
	{
		size_t i = 0;
		array_foreach(arr2, it) {
			CHECK(*it == (int)i);
			i++;
		}
	}
	array_clear(arr2);

	{
		array_foreach_value(arr1, it) {
			array_add(arr2, it);
		}
		CHECK(check_array_content(10, arr2, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9));
		array_clear(arr2);
		array_foreach_value_reverse(arr1, it) {
			array_add(arr2, it);
		}
		array_reverse(arr1);
		CHECK(array_equal(arr1, arr2));
		CHECK(check_array_content(10, arr1, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0));
		CHECK(check_array_content(10, arr2, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0));
		array_clear(arr1);
		array_foreach_value_reverse(arr2, it) {
			array_add(arr1, it);
		}
		int i = 0;
		array_foreach_value(arr1, it) {
			CHECK(it == i);
			i++;
		}
	}
	array_free(arr2);
	array_free(arr1);

	for (int i = 0; i < 1000; i++) {
		array_add(arr1, i);
	}
	for (int i = 0; i < 1000; i++) {
		int *x = array_bsearch(arr1, &i, cmp);
		CHECK(x);
		CHECK(*x == i);
		size_t idx;
		bool found = array_bsearch_index(arr1, &i, cmp, &idx);
		CHECK(found);
		CHECK(array_index_of(arr1, x) == idx);
	}
	array_free(arr1);

	*array_add1(arr1) = 1;
	array_add1_zero(arr1);
	array_add1_zero(arr1);
	*array_add1(arr1) = 1;
	array_addn_zero(arr1, 2);
	*array_add1(arr1) = 1;
	CHECK(check_array_content(7, arr1, 1, 0, 0, 1, 0, 0, 1));
	array_free(arr1);
	array_addn_zero(arr1, 2);
	array_add(arr1, 1);
	array_addn_zero(arr1, 2);
	array_add(arr1, 1);
	array_addn_zero(arr1, 2);
	CHECK(check_array_content(8, arr1, 0, 0, 1, 0, 0, 1, 0, 0));
	array_clear(arr1);
	array_addn_zero(arr1, 8);
	CHECK(array_length(arr1) == 8);
	CHECK(check_array_content(8, arr1, 0, 0, 0, 0, 0, 0, 0, 0));
	for (size_t i = 0; i < 8; i++) {
		array_addn_zero(arr1, 1);
	}
	CHECK(array_length(arr1) == 16);
	CHECK(check_array_content(16, arr1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0));
	array_free(arr1);
	array_addn_zero(arr1, 123);
	CHECK(array_length(arr1) == 123);
	array_free(arr1);

	*array_insert1(arr1, 0) = 1;
	*array_insert1(arr1, 0) = 1;
	*array_insert1(arr1, 1) = 1;
	array_insert1_zero(arr1, 1);
	array_insert1_zero(arr1, 1);
	array_insert1_zero(arr1, 4);
	array_insert1_zero(arr1, 5);
	CHECK(check_array_content(7, arr1, 1, 0, 0, 1, 0, 0, 1));
	array_free(arr1);
	array_add(arr1, 1);
	array_add(arr1, 1);
	array_insertn_zero(arr1, 0, 2);
	array_insertn_zero(arr1, 3, 2);
	array_insertn_zero(arr1, 6, 2);
	CHECK(check_array_content(8, arr1, 0, 0, 1, 0, 0, 1, 0, 0));
	array_clear(arr1);
	array_insertn_zero(arr1, 0, 8);
	CHECK(array_length(arr1) == 8);
	CHECK(check_array_content(8, arr1, 0, 0, 0, 0, 0, 0, 0, 0));
	for (size_t i = 0; i < 8; i++) {
		array_insertn_zero(arr1, i, 1);
	}
	CHECK(array_length(arr1) == 16);
	CHECK(check_array_content(16, arr1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0));
	array_free(arr1);
	array_insertn_zero(arr1, 0, 123);
	CHECK(array_length(arr1) == 123);
	array_free(arr1);

	for (int i = 0; i < 10; i++) {
		array_add(arr1, i);
	}
	arr2 = array_move(arr1);
	CHECK(arr1 == NULL);
	CHECK(check_array_content(10, arr2, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9));
	arr1 = array_move(arr2);
	CHECK(arr2 == NULL);
	CHECK(check_array_content(10, arr1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9));
	array_swap(arr1, 1, 3);
	CHECK(check_array_content(10, arr1, 0, 3, 2, 1, 4, 5, 6, 7, 8, 9));
	array_swap(arr1, 6, 2);
	CHECK(check_array_content(10, arr1, 0, 3, 6, 1, 4, 5, 2, 7, 8, 9));
	array_swap(arr1, 9, 8);
	CHECK(check_array_content(10, arr1, 0, 3, 6, 1, 4, 5, 2, 7, 9, 8));
	array_swap(arr1, 8, 0);
	CHECK(check_array_content(10, arr1, 9, 3, 6, 1, 4, 5, 2, 7, 0, 8));
	array_swap(arr1, 9, 0);
	CHECK(check_array_content(10, arr1, 8, 3, 6, 1, 4, 5, 2, 7, 0, 9));
	array_swap(arr1, 5, 9);
	CHECK(check_array_content(10, arr1, 8, 3, 6, 1, 4, 9, 2, 7, 0, 5));
	array_free(arr1);

	array_add_arrayn(arr1, digits, 10);
	global_sum = 0;
	array_call_foreach_value(arr1, sum);
	CHECK(global_sum == 45);
	array_call_foreach(arr1, increment);
	CHECK(check_array_content(10, arr1, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10));
	global_sum = 0;
	array_call_foreach_value(arr1, sum);
	CHECK(global_sum == 55);
	array_free(arr1);

	arr1 = array_new(int, 10);
	CHECK(array_capacity(arr1) >= 10);
	CHECK(check_array_content(0, arr1));
	array_free(arr1);

	arr1 = array_new(int, 1);
	CHECK(array_capacity(arr1) >= 1);
	CHECK(check_array_content(0, arr1));
	array_free(arr1);

	arr1 = array_new(int, 0);
	CHECK(!arr1);

	arr1 = NULL;
	array_fori(arr1, i) { assert(false); }
	array_fori_reverse(arr1, i) { assert(false); }
	array_foreach(arr1, i) { assert(false); }
	array_foreach_reverse(arr1, i) { assert(false); }
	array_foreach_value(arr1, i) { (void)i, assert(false); }
	array_foreach_value_reverse(arr1, i) { (void)i, assert(false); }

#if 0
	array_add_arrayn(arr1, digits, sizeof(digits) / sizeof(digits[0]));
	for (size_t i = 0; i < 5; i++) {
		array_shuffle(arr1, (size_t(*)(void))random);
		array_foreach(arr1, cur) {
			test_log("%i ", *cur);
		}
		putchar('\r');
		fflush(stdout);
		usleep(300000);
	}
	putchar('\n');
	array_free(arr1);
#endif
	return true;
}
