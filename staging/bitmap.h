#ifndef __bitmap_include__
#define __bitmap_include__

#include <stdbool.h>

#ifndef BITS_PER_LONG
#define BITS_PER_LONG (sizeof(long) * 8)
#endif

#define bm_required_size(nbits) (((nbits) + BITS_PER_LONG - 1) / BITS_PER_LONG * sizeof(long))

#define DECLARE_BITMAP(name, nbits) \
	unsigned long name[bm_required_size(nbits) / sizeof(long)]
#define DECLARE_EMPTY_BITMAP(name, nbits) DECLARE_BITMAP(name, nbits) = {0}

#define bm_foreach_zero(bitmap, nbits, itername) for (size_t itername = bm_find_first_zero(bitmap, nbits); itername < nbits; itername = bm_find_next_zero(bitmap, itername + 1, nbits))
#define bm_foreach_set(bitmap, nbits, itername) for (size_t itername = bm_find_first_set(bitmap, nbits); itername < nbits; itername = bm_find_next_set(bitmap, itername + 1, nbits))
// _bm_loop_counter_##__LINE__
#define bm_foreach(bitmap, nbits, itername) for (size_t counter = 0, itername; counter < nbits && (itername = bm_test_bit(bitmap, counter), true); counter++)

static inline size_t _ffs(unsigned long word)
{
	if (word == 0) {
		return BITS_PER_LONG;
	}
#if 1
	return __builtin_ctzl(word);
#else
	for (size_t i = 0; i < BITS_PER_LONG; i++) {
		if (word & (1UL << i)) {
			return i;
		}
	}
	return BITS_PER_LONG;
#endif
}

static size_t bm_find_next(const void *bitmap, size_t start, size_t nbits, unsigned long xor_mask)
{
	if (start >= nbits) {
		return nbits;
	}
	const unsigned long *bm = bitmap;
	unsigned long word = bm[start / BITS_PER_LONG];
	word ^= xor_mask;
	word &= ~0UL << (start & (BITS_PER_LONG - 1));
	start &= ~(BITS_PER_LONG - 1);
	while (word == 0) {
		start += BITS_PER_LONG;
		if (start >= nbits) {
			return nbits;
		}
		word = bm[start / BITS_PER_LONG];
		word ^= xor_mask;
	}
	start += _ffs(word);
	return start >= nbits ? nbits : start;
}

static inline size_t bm_find_next_zero(const void *bitmap, size_t start, size_t nbits)
{
	return bm_find_next(bitmap, start, nbits, ~0UL);
}

static inline size_t bm_find_first_zero(const void *bitmap, size_t nbits)
{
	return bm_find_next_zero(bitmap, 0, nbits);
}

static inline size_t bm_find_next_set(const void *bitmap, size_t start, size_t nbits)
{
	return bm_find_next(bitmap, start, nbits, 0);
}

static inline size_t bm_find_first_set(const void *bitmap, size_t nbits)
{
	return bm_find_next_set(bitmap, 0, nbits);
}

static inline void bm_set_bit_val(void *bitmap, size_t bit, bool val)
{
	unsigned long *bm = bitmap;
	bm[bit / BITS_PER_LONG] &= ~(1UL << (bit & (BITS_PER_LONG - 1)));
	bm[bit / BITS_PER_LONG] |= (unsigned long)val << (bit & (BITS_PER_LONG - 1));
}

static inline void bm_set_bit(void *bitmap, size_t bit)
{
	bm_set_bit_val(bitmap, bit, 1);
}

static inline void bm_clear_bit(void *bitmap, size_t bit)
{
	bm_set_bit_val(bitmap, bit, 0);
}

static inline bool bm_test_bit(const void *bitmap, size_t bit)
{
	const unsigned long *bm = bitmap;
	return (bm[bit / BITS_PER_LONG] >> (bit & (BITS_PER_LONG - 1))) & 1;
}

static void bm_and(void *bitmap, const void *other_bitmap, size_t nbits)
{
	unsigned long *bm = bitmap;
	const unsigned long *other = other_bitmap;

	size_t i;
	for (i = 0; i < nbits / BITS_PER_LONG; i++) {
		bm[i] &= other[i];
	}

	unsigned long mask = ~0UL << (nbits & (BITS_PER_LONG - 1));
	bm[i] &= other[i] | mask;
}

static void bm_or(void *bitmap, const void *other_bitmap, size_t nbits)
{
	unsigned long *bm = bitmap;
	const unsigned long *other = other_bitmap;

	size_t i;
	for (i = 0; i < nbits / BITS_PER_LONG; i++) {
		bm[i] |= other[i];
	}

	unsigned long mask = ~0UL << (nbits & (BITS_PER_LONG - 1));
	bm[i] |= other[i] & ~mask;
}

static void bm_xor(void *bitmap, const void *other_bitmap, size_t nbits)
{
	unsigned long *bm = bitmap;
	const unsigned long *other = other_bitmap;

	size_t i;
	for (i = 0; i < nbits / BITS_PER_LONG; i++) {
		bm[i] ^= other[i];
	}

	unsigned long mask = ~0UL << (nbits & (BITS_PER_LONG - 1));
	bm[i] ^= other[i] & ~mask;
}

static void bm_not(void *bitmap, size_t nbits)
{
	unsigned long *bm = bitmap;

	size_t i;
	for (i = 0; i < nbits / BITS_PER_LONG; i++) {
		bm[i] = ~bm[i];
	}

	unsigned long mask = ~0UL << (nbits & (BITS_PER_LONG - 1));
	bm[i] ^= ~mask;
}

#endif
