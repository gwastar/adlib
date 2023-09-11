#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compiler.h"
#include "random.h"
#include "utils.h"

#define UINT128_MAX ((uint128_t){.low = UINT64_MAX, .high = UINT64_MAX });

#ifdef HAVE_INT128
__extension__ typedef unsigned __int128 _u128_t;
#endif

typedef union uint128 {
#ifdef BYTE_ORDER_IS_LITTLE_ENDIAN
	struct {
		uint64_t low;
		uint64_t high;
	};
#else
	struct {
		uint64_t high;
		uint64_t low;
	};
#endif
#ifdef HAVE_INT128
	_u128_t u128;
#endif
} uint128_t;

#ifdef HAVE_INT128

static uint128_t uint128_from_u128(_u128_t u128)
{
	return (uint128_t){.u128 = u128};
}

static uint128_t uint128_from_high_low_bits_u128(uint64_t high, uint64_t low)
{
	return uint128_from_u128(((_u128_t)high << 64) | low);
}

static uint128_t uint128_from_low_bits_u128(uint64_t low)
{
	return uint128_from_u128(low);
}

static uint64_t uint128_get_low_bits_u128(uint128_t x)
{
	return (uint64_t)x.u128;
}

static uint64_t uint128_get_high_bits_u128(uint128_t x)
{
	return (uint64_t)(x.u128 >> 64);

}

static uint128_t uint128_add_u128(uint128_t a, uint128_t b)
{
	return uint128_from_u128(a.u128 + b.u128);
}

static uint128_t uint128_sub_u128(uint128_t a, uint128_t b)
{
	return uint128_from_u128(a.u128 - b.u128);
}

static uint128_t uint128_negate_u128(uint128_t x)
{
	return uint128_from_u128(-x.u128);
}

static uint128_t uint128_mul64_u128(uint64_t a, uint64_t b)
{
	return uint128_from_u128((_u128_t)a * b);
}

static uint128_t uint128_mul_u128(uint128_t a, uint128_t b)
{
	return uint128_from_u128(a.u128 * b.u128);
}

static uint128_t uint128_lshift_u128(uint128_t a, size_t shift_amount)
{
	shift_amount &= 127;
	return uint128_from_u128(a.u128 << shift_amount);
}

static uint128_t uint128_rshift_u128(uint128_t a, size_t shift_amount)
{
	shift_amount &= 127;
	return uint128_from_u128(a.u128 >> shift_amount);
}

static uint128_t uint128_and_u128(uint128_t a, uint128_t b)
{
	return uint128_from_u128(a.u128 & b.u128);
}

static uint128_t uint128_or_u128(uint128_t a, uint128_t b)
{
	return uint128_from_u128(a.u128 | b.u128);
}

static uint128_t uint128_xor_u128(uint128_t a, uint128_t b)
{
	return uint128_from_u128(a.u128 ^ b.u128);
}

static uint128_t uint128_not_u128(uint128_t x)
{
	return uint128_from_u128(~x.u128);
}

static int uint128_cmp_u128(uint128_t a, uint128_t b)
{
	return (a.u128 < b.u128) ? -1 : (a.u128 > b.u128);
}

#endif // HAVE_INT128

static uint128_t uint128_from_high_low_bits_generic(uint64_t high, uint64_t low)
{
	return (uint128_t){.high = high, .low = low};
}

static uint128_t uint128_from_high_low_bits(uint64_t high, uint64_t low)
{
#ifdef HAVE_INT128
	return uint128_from_high_low_bits_u128(high, low);
#else
	return uint128_from_high_low_bits_generic(high, low);
#endif
}

static uint128_t uint128_from_low_bits_generic(uint64_t low)
{
	return uint128_from_high_low_bits(0, low);
}

static uint128_t uint128_from_low_bits(uint64_t low)
{
#ifdef HAVE_INT128
	return uint128_from_low_bits_u128(low);
#else
	return uint128_from_low_bits_generic(low);
#endif
}

static uint128_t uint128_zero(void)
{
	return uint128_from_low_bits(0);
}

static uint128_t uint128_one(void)
{
	return uint128_from_low_bits(1);
}

static uint128_t uint128_max(void)
{
	return uint128_from_high_low_bits(UINT64_MAX, UINT64_MAX);
}

static uint64_t uint128_get_low_bits_generic(uint128_t x)
{
	return x.low;
}

static uint64_t uint128_get_low_bits(uint128_t x)
{
#ifdef HAVE_INT128
	return uint128_get_low_bits_u128(x);
#else
	return uint128_get_low_bits_generic(x);
#endif
}

static uint64_t uint128_get_high_bits_generic(uint128_t x)
{
	return x.high;
}

static uint64_t uint128_get_high_bits(uint128_t x)
{
#ifdef HAVE_INT128
	return uint128_get_high_bits_u128(x);
#else
	return uint128_get_high_bits_generic(x);
#endif
}

static uint128_t uint128_add_generic(uint128_t a, uint128_t b)
{
	uint64_t low;
	bool overflow = add_overflow(a.low, b.low, &low);
	uint64_t high = a.high + b.high + overflow;
	return uint128_from_high_low_bits(high, low);
}

static uint128_t uint128_add(uint128_t a, uint128_t b)
{
#ifdef HAVE_INT128
	return uint128_add_u128(a, b);
#else
	return uint128_add_generic(a, b);
#endif
}

static uint128_t uint128_sub_generic(uint128_t a, uint128_t b)
{
	uint64_t low, high;
	bool overflow = sub_overflow(a.low, b.low, &low);
	high = a.high - b.high - overflow;
	return uint128_from_high_low_bits(high, low);
}

static uint128_t uint128_sub(uint128_t a, uint128_t b)
{
#ifdef HAVE_INT128
	return uint128_sub_u128(a, b);
#else
	return uint128_sub_generic(a, b);
#endif
}

static uint128_t uint128_negate_generic(uint128_t x)
{
	return uint128_sub(uint128_zero(), x);
}

static uint128_t uint128_negate(uint128_t x)
{
#ifdef HAVE_INT128
	return uint128_negate_u128(x);
#else
	return uint128_negate_generic(x);
#endif
}

static uint128_t uint128_mul64_generic(uint64_t a, uint64_t b)
{
	// a * b = (2^32 * a_h + a_l) * (2^32 * b_h + b_l)
	//       = 2^64 * a_h * b_h + 2^32 * a_h * b_l + 2^32 * a_l * b_h + a_l * b_l
	uint64_t a_l = (uint32_t)a;
	uint64_t a_h = a >> 32;
	uint64_t b_l = (uint32_t)b;
	uint64_t b_h = b >> 32;
	uint64_t l = a_l * b_l;
	uint64_t m1 = a_h * b_l;
	uint64_t m2 = a_l * b_h;
	uint64_t m = (l >> 32) + m1;
	uint64_t m_h = m >> 32;
	m = (uint32_t)m + m2;
	m_h += m >> 32;
	uint64_t high = a_h * b_h + m_h;
	uint64_t low = (uint32_t)l + (m << 32);
	return uint128_from_high_low_bits(high, low);
}

static uint128_t uint128_mul64(uint64_t a, uint64_t b)
{
#ifdef HAVE_INT128
	return uint128_mul64_u128(a, b);
#else
	return uint128_mul64_generic(a, b);
#endif
}

static uint128_t uint128_mul_generic(uint128_t a, uint128_t b)
{
	// a * b = (2^64 * a_h + a_l) * (2^64 * b_h + b_l)
	//       = 2^128 * a_h * b_h + 2^64 * a_h * b_l + 2^64 * a_l * b_h + a_l * b_l
	uint128_t r = uint128_mul64(a.low, b.low);
	r.high += a.high * b.low + a.low * b.high;
	return r;
}

static uint128_t uint128_mul(uint128_t a, uint128_t b)
{
#ifdef HAVE_INT128
	return uint128_mul_u128(a, b);
#else
	return uint128_mul_generic(a, b);
#endif
}

static uint128_t uint128_lshift_generic(uint128_t a, size_t shift_amount)
{
	shift_amount &= 127;
	if (shift_amount >= 64) {
		a.high = a.low << (shift_amount - 64);
		a.low = 0;
	} else {
		a.high <<= shift_amount;
		a.high |= shift_amount == 0 ? 0 : a.low >> (64 - shift_amount);
		a.low <<= shift_amount;
	}
	return a;
}

static uint128_t uint128_lshift(uint128_t a, size_t shift_amount)
{
#ifdef HAVE_INT128
	return uint128_lshift_u128(a, shift_amount);
#else
	return uint128_lshift_generic(a, shift_amount);
#endif
}

static uint128_t uint128_rshift_generic(uint128_t a, size_t shift_amount)
{
	shift_amount &= 127;
	if (shift_amount >= 64) {
		a.low = a.high >> (shift_amount - 64);
		a.high = 0;
	} else {
		a.low >>= shift_amount;
		a.low |= shift_amount == 0 ? 0 : a.high << (64 - shift_amount);
		a.high >>= shift_amount;
	}
	return a;
}

static uint128_t uint128_rshift(uint128_t a, size_t shift_amount)
{
#ifdef HAVE_INT128
	return uint128_rshift_u128(a, shift_amount);
#else
	return uint128_rshift_generic(a, shift_amount);
#endif
}

static uint128_t uint128_and_generic(uint128_t a, uint128_t b)
{
	a.high &= b.high;
	a.low &= b.low;
	return a;
}

static uint128_t uint128_and(uint128_t a, uint128_t b)
{
#ifdef HAVE_INT128
	return uint128_and_u128(a, b);
#else
	return uint128_and_generic(a, b);
#endif
}

static uint128_t uint128_or_generic(uint128_t a, uint128_t b)
{
	a.high |= b.high;
	a.low |= b.low;
	return a;
}

static uint128_t uint128_or(uint128_t a, uint128_t b)
{
#ifdef HAVE_INT128
	return uint128_or_u128(a, b);
#else
	return uint128_or_generic(a, b);
#endif
}

static uint128_t uint128_xor_generic(uint128_t a, uint128_t b)
{
	a.high ^= b.high;
	a.low ^= b.low;
	return a;
}

static uint128_t uint128_xor(uint128_t a, uint128_t b)
{
#ifdef HAVE_INT128
	return uint128_xor_u128(a, b);
#else
	return uint128_xor_generic(a, b);
#endif
}

static uint128_t uint128_not_generic(uint128_t x)
{
	return uint128_from_high_low_bits(~x.high, ~x.low);
}

static uint128_t uint128_not(uint128_t x)
{
#ifdef HAVE_INT128
	return uint128_not_u128(x);
#else
	return uint128_not_generic(x);
#endif
}

static int uint128_cmp_generic(uint128_t a, uint128_t b)
{
	uint64_t x = a.high;
	uint64_t y = b.high;
	if (x == y) {
		x = a.low;
		y = b.low;
	}
	return (x < y) ? -1 : (x > y);
}

static int uint128_cmp(uint128_t a, uint128_t b)
{
#ifdef HAVE_INT128
	return uint128_cmp_u128(a, b);
#else
	return uint128_cmp_generic(a, b);
#endif
}

static int uint128_eq(uint128_t a, uint128_t b)
{
	return uint128_cmp(a, b) == 0;
}

static int uint128_ne(uint128_t a, uint128_t b)
{
	return uint128_cmp(a, b) != 0;
}

static int uint128_lt(uint128_t a, uint128_t b)
{
	return uint128_cmp(a, b) < 0;
}

static int uint128_le(uint128_t a, uint128_t b)
{
	return uint128_cmp(a, b) <= 0;
}

static int uint128_gt(uint128_t a, uint128_t b)
{
	return uint128_cmp(a, b) > 0;
}

static int uint128_ge(uint128_t a, uint128_t b)
{
	return uint128_cmp(a, b) >= 0;
}

#define UINT128(h, l) {.high = h, .low = l}

static uint128_t edge_cases[] = {
	UINT128(0, 0),
	UINT128(0, 1),
	UINT128(1, 0),
	UINT128(1, 1),
	UINT128(0, UINT64_MAX),
	UINT128(1, UINT64_MAX),
	UINT128(0, UINT64_MAX - 1),
	UINT128(1, UINT64_MAX - 1),
	UINT128(UINT64_MAX, 0),
	UINT128(UINT64_MAX, 1),
	UINT128(UINT64_MAX - 1, 0),
	UINT128(UINT64_MAX - 1, 1),
	UINT128(UINT64_MAX, UINT64_MAX),
	UINT128(UINT64_MAX - 1, UINT64_MAX),
	UINT128(UINT64_MAX, UINT64_MAX - 1),
	UINT128(UINT64_MAX - 1, UINT64_MAX - 1),
	UINT128(0, UINT32_MAX),
	UINT128(1, UINT32_MAX),
	UINT128(0, UINT32_MAX - 1),
	UINT128(1, UINT32_MAX - 1),
	UINT128(UINT32_MAX, 0),
	UINT128(UINT32_MAX, 1),
	UINT128(UINT32_MAX - 1, 0),
	UINT128(UINT32_MAX - 1, 1),
	UINT128(UINT32_MAX, UINT32_MAX),
	UINT128(UINT32_MAX - 1, UINT32_MAX),
	UINT128(UINT32_MAX, UINT32_MAX - 1),
	UINT128(UINT32_MAX - 1, UINT32_MAX - 1),
	UINT128(UINT64_MAX, UINT32_MAX),
	UINT128(UINT64_MAX - 1, UINT32_MAX),
	UINT128(UINT64_MAX, UINT32_MAX - 1),
	UINT128(UINT64_MAX - 1, UINT32_MAX - 1),
	UINT128(UINT32_MAX, UINT64_MAX),
	UINT128(UINT32_MAX - 1, UINT64_MAX),
	UINT128(UINT32_MAX, UINT64_MAX - 1),
	UINT128(UINT32_MAX - 1, UINT64_MAX - 1),
};

static void print_hexadecimal(void *p, size_t size)
{
	unsigned char *c = p;
	for (size_t i = 0; i < size; i++) {
		printf("%02x", c[i]);
	}
}

#define check_function(name, ...)					\
	static void check_##name(uint128_t a, uint128_t b)		\
	{								\
		typedef typeof(uint128_##name(__VA_ARGS__)) result_t;	\
		result_t r1 = uint128_##name##_generic(__VA_ARGS__);	\
		result_t r2 = uint128_##name##_u128(__VA_ARGS__);	\
		if (memcmp(&r1, &r2, sizeof(r1)) != 0) {		\
			printf("%s: a=%016" PRIx64 "%016" PRIx64 ", b=%016" PRIx64 "%016" PRIx64 "\n", \
			       #name, a.high, a.low, b.high, b.low);	\
			printf("got:      ");				\
			print_hexadecimal(&r1, sizeof(r1));		\
			printf("\nexpected: ");				\
			print_hexadecimal(&r2, sizeof(r2));		\
			printf("\n");					\
			exit(EXIT_FAILURE);				\
		}							\
		result_t r3 = uint128_##name(__VA_ARGS__);		\
		assert(memcmp(&r3, &r1, sizeof(r1)) == 0);		\
	}

#define foreach_function(f)			\
	f(add, a, b)				\
	f(sub, a, b)				\
	f(mul, a, b)				\
	f(mul64, a.low, b.low)			\
	f(lshift, a, b.low)			\
	f(rshift, a, b.low)			\
	f(and, a, b)				\
	f(or, a, b)				\
	f(xor, a, b)				\
	f(cmp, a, b)				\
	f(negate, a)				\
	f(not, a)

foreach_function(check_function)

static void test(void)
{
#define call_check_function(name, ...) check_##name(a, b);

	size_t num_edge_cases = sizeof(edge_cases) / sizeof(edge_cases[0]);
	for (size_t i = 0; i < num_edge_cases; i++) {
		for (size_t j = 0; j < num_edge_cases; j++) {
			uint128_t a = edge_cases[i];
			uint128_t b = edge_cases[j];
			foreach_function(call_check_function);
		}
	}

	struct random_state rng;
	random_state_init(&rng, 0xdeadbeef);
	static const size_t N = 1 << 28;
	for (size_t i = 0; i < N; i++) {
		uint128_t a = uint128_from_high_low_bits(random_next_u64(&rng), random_next_u64(&rng));
		uint128_t b = uint128_from_high_low_bits(random_next_u64(&rng), random_next_u64(&rng));
		foreach_function(call_check_function);
	}
}

int main(void)
{
	test();
}
