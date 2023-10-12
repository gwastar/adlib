#include "compiler.h"
#include "utils.h"

#define UINT128_MAX ((uint128_t){.low = UINT64_MAX, .high = UINT64_MAX })

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

static _attr_always_inline uint128_t _uint128_from_u128(_u128_t u128)
{
	return (uint128_t){.u128 = u128};
}

static _attr_always_inline uint128_t _uint128_from_high_low_bits_u128(uint64_t high, uint64_t low)
{
	return _uint128_from_u128(((_u128_t)high << 64) | low);
}

static _attr_always_inline uint128_t _uint128_from_low_bits_u128(uint64_t low)
{
	return _uint128_from_u128(low);
}

static _attr_always_inline uint64_t _uint128_get_low_bits_u128(uint128_t x)
{
	return (uint64_t)x.u128;
}

static _attr_always_inline uint64_t _uint128_get_high_bits_u128(uint128_t x)
{
	return (uint64_t)(x.u128 >> 64);
}

static _attr_always_inline uint128_t _uint128_add_u128(uint128_t a, uint128_t b)
{
	return _uint128_from_u128(a.u128 + b.u128);
}

static _attr_always_inline uint128_t _uint128_sub_u128(uint128_t a, uint128_t b)
{
	return _uint128_from_u128(a.u128 - b.u128);
}

static _attr_always_inline uint128_t _uint128_negate_u128(uint128_t x)
{
	return _uint128_from_u128(-x.u128);
}

static _attr_always_inline uint128_t _uint128_mul64_u128(uint64_t a, uint64_t b)
{
	return _uint128_from_u128((_u128_t)a * b);
}

static _attr_always_inline uint128_t _uint128_mul_u128(uint128_t a, uint128_t b)
{
	return _uint128_from_u128(a.u128 * b.u128);
}

static _attr_always_inline uint128_t _uint128_lshift_u128(uint128_t a, size_t shift_amount)
{
	shift_amount &= 127;
	return _uint128_from_u128(a.u128 << shift_amount);
}

static _attr_always_inline uint128_t _uint128_rshift_u128(uint128_t a, size_t shift_amount)
{
	shift_amount &= 127;
	return _uint128_from_u128(a.u128 >> shift_amount);
}

static _attr_always_inline uint128_t _uint128_and_u128(uint128_t a, uint128_t b)
{
	return _uint128_from_u128(a.u128 & b.u128);
}

static _attr_always_inline uint128_t _uint128_or_u128(uint128_t a, uint128_t b)
{
	return _uint128_from_u128(a.u128 | b.u128);
}

static _attr_always_inline uint128_t _uint128_xor_u128(uint128_t a, uint128_t b)
{
	return _uint128_from_u128(a.u128 ^ b.u128);
}

static _attr_always_inline uint128_t _uint128_invert_u128(uint128_t x)
{
	return _uint128_from_u128(~x.u128);
}

static _attr_always_inline int _uint128_cmp_u128(uint128_t a, uint128_t b)
{
	return (a.u128 < b.u128) ? -1 : (a.u128 > b.u128);
}

#endif // HAVE_INT128

#if !defined(HAVE_INT128) || defined(__ADLIB_TESTS__)

static _attr_always_inline uint128_t _uint128_from_high_low_bits_generic(uint64_t high, uint64_t low)
{
	return (uint128_t){.high = high, .low = low};
}

static _attr_always_inline uint128_t _uint128_from_low_bits_generic(uint64_t low)
{
	return _uint128_from_high_low_bits_generic(0, low);
}

static _attr_always_inline uint64_t _uint128_get_low_bits_generic(uint128_t x)
{
	return x.low;
}

static _attr_always_inline uint64_t _uint128_get_high_bits_generic(uint128_t x)
{
	return x.high;
}

static _attr_always_inline uint128_t _uint128_add_generic(uint128_t a, uint128_t b)
{
	uint64_t low;
	bool overflow = add_overflow(a.low, b.low, &low);
	uint64_t high = a.high + b.high + overflow;
	return _uint128_from_high_low_bits_generic(high, low);
}

static _attr_always_inline uint128_t _uint128_sub_generic(uint128_t a, uint128_t b)
{
	uint64_t low, high;
	bool overflow = sub_overflow(a.low, b.low, &low);
	high = a.high - b.high - overflow;
	return _uint128_from_high_low_bits_generic(high, low);
}

static _attr_always_inline uint128_t _uint128_negate_generic(uint128_t x)
{
	return _uint128_sub_generic(_uint128_from_low_bits_generic(0), x);
}

static _attr_unused uint128_t _uint128_mul64_generic(uint64_t a, uint64_t b)
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
	return _uint128_from_high_low_bits_generic(high, low);
}

static _attr_always_inline uint128_t _uint128_mul_generic(uint128_t a, uint128_t b)
{
	// a * b = (2^64 * a_h + a_l) * (2^64 * b_h + b_l)
	//       = 2^128 * a_h * b_h + 2^64 * a_h * b_l + 2^64 * a_l * b_h + a_l * b_l
	uint128_t r = _uint128_mul64_generic(a.low, b.low);
	r.high += a.high * b.low + a.low * b.high;
	return r;
}

static _attr_always_inline uint128_t _uint128_lshift_generic(uint128_t a, size_t shift_amount)
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

static _attr_always_inline uint128_t _uint128_rshift_generic(uint128_t a, size_t shift_amount)
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


static _attr_always_inline uint128_t _uint128_and_generic(uint128_t a, uint128_t b)
{
	a.high &= b.high;
	a.low &= b.low;
	return a;
}

static _attr_always_inline uint128_t _uint128_or_generic(uint128_t a, uint128_t b)
{
	a.high |= b.high;
	a.low |= b.low;
	return a;
}

static _attr_always_inline uint128_t _uint128_xor_generic(uint128_t a, uint128_t b)
{
	a.high ^= b.high;
	a.low ^= b.low;
	return a;
}

static _attr_always_inline uint128_t _uint128_invert_generic(uint128_t x)
{
	return _uint128_from_high_low_bits_generic(~x.high, ~x.low);
}

static _attr_always_inline int _uint128_cmp_generic(uint128_t a, uint128_t b)
{
	uint64_t x = a.high;
	uint64_t y = b.high;
	if (x == y) {
		x = a.low;
		y = b.low;
	}
	return (x < y) ? -1 : (x > y);
}

#endif // !defined(HAVE_INT128) || defined(__ADLIB_TESTS__)

static _attr_always_inline uint128_t uint128_from_high_low_bits(uint64_t high, uint64_t low)
{
#ifdef HAVE_INT128
	return _uint128_from_high_low_bits_u128(high, low);
#else
	return _uint128_from_high_low_bits_generic(high, low);
#endif
}

static _attr_always_inline uint128_t uint128_from_low_bits(uint64_t low)
{
#ifdef HAVE_INT128
	return _uint128_from_low_bits_u128(low);
#else
	return _uint128_from_low_bits_generic(low);
#endif
}

static _attr_always_inline uint128_t uint128_zero(void)
{
	return uint128_from_low_bits(0);
}

static _attr_always_inline uint128_t uint128_one(void)
{
	return uint128_from_low_bits(1);
}

static _attr_always_inline uint128_t uint128_max_value(void)
{
	return uint128_from_high_low_bits(UINT64_MAX, UINT64_MAX);
}

static _attr_always_inline uint64_t uint128_get_low_bits(uint128_t x)
{
#ifdef HAVE_INT128
	return _uint128_get_low_bits_u128(x);
#else
	return _uint128_get_low_bits_generic(x);
#endif
}

static _attr_always_inline uint64_t uint128_get_high_bits(uint128_t x)
{
#ifdef HAVE_INT128
	return _uint128_get_high_bits_u128(x);
#else
	return _uint128_get_high_bits_generic(x);
#endif
}

static _attr_always_inline uint128_t uint128_add(uint128_t a, uint128_t b)
{
#ifdef HAVE_INT128
	return _uint128_add_u128(a, b);
#else
	return _uint128_add_generic(a, b);
#endif
}

static _attr_always_inline uint128_t uint128_sub(uint128_t a, uint128_t b)
{
#ifdef HAVE_INT128
	return _uint128_sub_u128(a, b);
#else
	return _uint128_sub_generic(a, b);
#endif
}

static _attr_always_inline uint128_t uint128_negate(uint128_t x)
{
#ifdef HAVE_INT128
	return _uint128_negate_u128(x);
#else
	return _uint128_negate_generic(x);
#endif
}

static _attr_always_inline uint128_t uint128_mul64(uint64_t a, uint64_t b)
{
#ifdef HAVE_INT128
	return _uint128_mul64_u128(a, b);
#else
	return _uint128_mul64_generic(a, b);
#endif
}

static _attr_always_inline uint128_t uint128_mul(uint128_t a, uint128_t b)
{
#ifdef HAVE_INT128
	return _uint128_mul_u128(a, b);
#else
	return _uint128_mul_generic(a, b);
#endif
}

static _attr_always_inline uint128_t uint128_lshift(uint128_t a, size_t shift_amount)
{
#ifdef HAVE_INT128
	return _uint128_lshift_u128(a, shift_amount);
#else
	return _uint128_lshift_generic(a, shift_amount);
#endif
}

static _attr_always_inline uint128_t uint128_rshift(uint128_t a, size_t shift_amount)
{
#ifdef HAVE_INT128
	return _uint128_rshift_u128(a, shift_amount);
#else
	return _uint128_rshift_generic(a, shift_amount);
#endif
}

static _attr_always_inline uint128_t uint128_and(uint128_t a, uint128_t b)
{
#ifdef HAVE_INT128
	return _uint128_and_u128(a, b);
#else
	return _uint128_and_generic(a, b);
#endif
}

static _attr_always_inline uint128_t uint128_or(uint128_t a, uint128_t b)
{
#ifdef HAVE_INT128
	return _uint128_or_u128(a, b);
#else
	return _uint128_or_generic(a, b);
#endif
}

static _attr_always_inline uint128_t uint128_xor(uint128_t a, uint128_t b)
{
#ifdef HAVE_INT128
	return _uint128_xor_u128(a, b);
#else
	return _uint128_xor_generic(a, b);
#endif
}

static _attr_always_inline uint128_t uint128_invert(uint128_t x)
{
#ifdef HAVE_INT128
	return _uint128_invert_u128(x);
#else
	return _uint128_invert_generic(x);
#endif
}

static _attr_always_inline int uint128_cmp(uint128_t a, uint128_t b)
{
#ifdef HAVE_INT128
	return _uint128_cmp_u128(a, b);
#else
	return _uint128_cmp_generic(a, b);
#endif
}

static _attr_always_inline int uint128_eq(uint128_t a, uint128_t b)
{
	return uint128_cmp(a, b) == 0;
}

static _attr_always_inline int uint128_ne(uint128_t a, uint128_t b)
{
	return uint128_cmp(a, b) != 0;
}

static _attr_always_inline int uint128_lt(uint128_t a, uint128_t b)
{
	return uint128_cmp(a, b) < 0;
}

static _attr_always_inline int uint128_le(uint128_t a, uint128_t b)
{
	return uint128_cmp(a, b) <= 0;
}

static _attr_always_inline int uint128_gt(uint128_t a, uint128_t b)
{
	return uint128_cmp(a, b) > 0;
}

static _attr_always_inline int uint128_ge(uint128_t a, uint128_t b)
{
	return uint128_cmp(a, b) >= 0;
}
