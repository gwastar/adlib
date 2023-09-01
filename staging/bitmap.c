#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include "bitmap.h"

static void print_bitmap(const void *bitmap, size_t nbits)
{
	bm_foreach(bitmap, nbits, it) {
		printf("%zu", it);
	}
	putchar('\n');
}

static void init_bitmap_contents(void *bitmap, size_t nbits, const char *contents)
{
	assert(strlen(contents) == nbits);
	for (size_t i = 0; i < nbits; i++) {
		assert(contents[i] == '0' || contents[i] == '1');
		bm_set_bit_val(bitmap, i, contents[i] == '1');
	}
}

static void assert_bitmap_contents(const void *bitmap, size_t nbits, const char *contents)
{
	assert(strlen(contents) == nbits);
	for (size_t i = 0; i < nbits; i++) {
		char bit = bm_test_bit(bitmap, i) ? '1' : '0';
		assert(contents[i] == bit);
	}
}

int main(void)
{
#if 0
#define NBITS 123456789
	static DECLARE_EMPTY_BITMAP(bm, NBITS);

	srand(time(NULL));
	for (size_t i = 0; i < NBITS; i++) {
		if (rand() & 1) {
			bm_set_bit(bm, i);
		}
	}


	size_t i = 0;
	for (size_t bit = bm_find_first_zero(bm, NBITS);
	     bit < NBITS;
	     bit = bm_find_next_zero(bm, bit, NBITS)) {
		bm_set_bit(bm, bit);
		i++;
	}

	i = 0;
	bm_foreach_zero(bm, NBITS, cur) {
		i++;
	}
	assert(i == 0);

	i = 0;
	for (size_t bit = bm_find_first_set(bm, NBITS);
	     bit < NBITS;
	     bit = bm_find_next_set(bm, bit, NBITS)) {
		assert(bit == i);
		bm_clear_bit(bm, bit);
		i++;
	}

	i = 0;
	bm_foreach_set(bm, NBITS, cur) {
		i++;
	}
	assert(i == 0);

	i = 0;
	bm_foreach_zero(bm, NBITS, cur) {
		assert(cur == i);
		bm_set_bit(bm, cur);
		i++;
	}

	i = 0;
	bm_foreach_set(bm, NBITS, cur) {
		assert(cur == i);
		bm_clear_bit(bm, cur);
		i++;
	}

	for (i = 0; i < 12345; i++) {
		bm_set_bit(bm, rand() % NBITS);
	}

	i = 0;
	bm_foreach_zero(bm, NBITS, cur) {
		assert(!bm_test_bit(bm, cur));
		i++;
	}

	bm_foreach_set(bm, NBITS, cur) {
		assert(bm_test_bit(bm, cur));
		i++;
	}

	assert(i == NBITS);
#else
#define NBITS 15
#define N 11
	DECLARE_EMPTY_BITMAP(bm1, NBITS);
	DECLARE_EMPTY_BITMAP(bm2, NBITS);

	init_bitmap_contents(bm1, NBITS, "111010111001010");
	init_bitmap_contents(bm2, NBITS, "011110101100011");
	bm_and(bm1, bm2, N);
	bm_and(bm2, bm1, N);
	assert_bitmap_contents(bm1, NBITS, "011010101001010");
	assert_bitmap_contents(bm2, NBITS, "011010101000011");

	init_bitmap_contents(bm1, NBITS, "000000000001010");
	init_bitmap_contents(bm2, NBITS, "111111111110011");
	bm_and(bm1, bm2, N);
	bm_and(bm2, bm1, N);
	assert_bitmap_contents(bm1, NBITS, "000000000001010");
	assert_bitmap_contents(bm2, NBITS, "000000000000011");

	init_bitmap_contents(bm1, NBITS, "111111111111010");
	init_bitmap_contents(bm2, NBITS, "111111111110011");
	bm_and(bm1, bm2, N);
	bm_and(bm2, bm1, N);
	assert_bitmap_contents(bm1, NBITS, "111111111111010");
	assert_bitmap_contents(bm2, NBITS, "111111111110011");


	init_bitmap_contents(bm1, NBITS, "111010111001010");
	init_bitmap_contents(bm2, NBITS, "011110101100011");
	bm_or(bm1, bm2, N);
	bm_or(bm2, bm1, N);
	assert_bitmap_contents(bm1, NBITS, "111110111101010");
	assert_bitmap_contents(bm2, NBITS, "111110111100011");

	init_bitmap_contents(bm1, NBITS, "000000000001010");
	init_bitmap_contents(bm2, NBITS, "111111111110011");
	bm_or(bm1, bm2, N);
	bm_or(bm2, bm1, N);
	assert_bitmap_contents(bm1, NBITS, "111111111111010");
	assert_bitmap_contents(bm2, NBITS, "111111111110011");

	init_bitmap_contents(bm1, NBITS, "000000000001010");
	init_bitmap_contents(bm2, NBITS, "000000000000011");
	bm_or(bm1, bm2, N);
	bm_or(bm2, bm1, N);
	assert_bitmap_contents(bm1, NBITS, "000000000001010");
	assert_bitmap_contents(bm2, NBITS, "000000000000011");


	init_bitmap_contents(bm1, NBITS, "111010111001010");
	init_bitmap_contents(bm2, NBITS, "011110101100011");
	bm_xor(bm1, bm2, N);
	bm_xor(bm2, bm1, N);
	assert_bitmap_contents(bm1, NBITS, "100100010101010");
	assert_bitmap_contents(bm2, NBITS, "111010111000011");

	init_bitmap_contents(bm1, NBITS, "000000000001010");
	init_bitmap_contents(bm2, NBITS, "111111111110011");
	bm_xor(bm1, bm2, N);
	bm_xor(bm2, bm1, N);
	assert_bitmap_contents(bm1, NBITS, "111111111111010");
	assert_bitmap_contents(bm2, NBITS, "000000000000011");

	init_bitmap_contents(bm1, NBITS, "000000000001010");
	init_bitmap_contents(bm2, NBITS, "000000000000011");
	bm_xor(bm1, bm2, N);
	bm_xor(bm2, bm1, N);
	assert_bitmap_contents(bm1, NBITS, "000000000001010");
	assert_bitmap_contents(bm2, NBITS, "000000000000011");

	init_bitmap_contents(bm1, NBITS, "000000000001010");
	bm_not(bm1, N);
	assert_bitmap_contents(bm1, NBITS, "111111111111010");
	bm_not(bm1, N);
	assert_bitmap_contents(bm1, NBITS, "000000000001010");

	print_bitmap(bm1, NBITS);
#endif
}
