#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include "testing.h"
#include "utils.h"

static unsigned int ilog2_reference(uint64_t x)
{
	unsigned int result = 0;
	if (x >= (1llu << 31)) {
		result += 31;
		x >>= 31;
	}
	while (x >= (1u << 12)) {
		result += 12;
		x >>= 12;
	}
	while (x >= (1u << 6)) {
		result += 6;
		x >>= 6;
	}
	while (x >= (1u << 4)) {
		result += 4;
		x >>= 4;
	}
	while (x >= 2) {
		result++;
		x >>= 1;
	}
	return result;
}

RANGE_TEST(ilog2, 0, UINT32_MAX)
{
	uint64_t x = value;
	unsigned int lg2 = ilog2_reference((uint32_t)x);
	if (ilog2((uint32_t)x) != lg2 ||
	    ilog2((int32_t)x) != ilog2((uint64_t)x)) {
		return false;
	}
	return true;
}

RANDOM_TEST(ilog2_rand_64, 1u << 30)
{
	uint64_t x = random_seed;
	unsigned int lg2 = ilog2_reference(x);
	if (ilog2((int64_t)x) != lg2) {
		return false;
	}
	return true;
}

static unsigned int ilog10_reference(uint64_t x)
{
	unsigned int result = 0;
	if (x >= 100000) {
		result += 5;
		x /= 100000;
	}
	while (x >= 10000) {
		result += 4;
		x /= 10000;
	}
	while (x >= 100) {
		result += 2;
		x /= 100;
	}
	while (x >= 10) {
		result++;
		x /= 10;
	}
	return result;
}

RANGE_TEST(ilog10, 0, UINT32_MAX)
{
	uint64_t x = value;
	unsigned int lg10 = ilog10_reference((uint32_t)x);
	if (ilog10((uint32_t)x) != lg10 ||
	    ilog10((int32_t)x) != ilog10((uint64_t)x)) {
		return false;
	}
	return true;
}

RANDOM_TEST(ilog10_rand_64, 1u << 30)
{
	uint64_t x = random_seed;
	unsigned int lg10 = ilog10_reference(x);
	if (ilog10((int64_t)x) != lg10) {
		return false;
	}
	return true;
}

static unsigned int hackers_delight_clz32(uint32_t x)
{
	static _Alignas(64) const char table[64] = {
#define u 0
		32, 20, 19, u, u, 18, u, 7, 10, 17, u, u, 14, u, 6, u,
		u, 9, u, 16, u, u, 1, 26, u, 13, u, u, 24, 5, u, u,
		u, 21, u, 8, 11, u, 15, u, u, u, u, 2, 27, 0, 25, u,
		22, u, 12, u, u, 3, 28, u, 23, u, 4, 29, u, u, 30, 31
#undef u
	};
	x = x | (x >> 1);
	x = x | (x >> 2);
	x = x | (x >> 4);
	x = x | (x >> 8);
	x = x & ~(x >> 16);
	x = x * 0xFD7049FF;
	return table[x >> 26];
}

static unsigned int hackers_delight_clz64(uint64_t x)
{
	unsigned int n = hackers_delight_clz32(x >> 32);
	if (n == 32) {
		n += hackers_delight_clz32(x);
	}
	return n;
}

RANGE_TEST(clz32, 0, UINT32_MAX)
{
	uint64_t x = value;
	unsigned int reference = hackers_delight_clz32(x);
	if (clz((uint32_t)x) != reference ||
	    clz((int32_t)x) != reference) {
		return false;
	}
	return true;
}

RANGE_TEST(clz64, 0, UINT32_MAX)
{
	uint64_t x = value;
	unsigned int reference = hackers_delight_clz64(x);
	if (clz((uint64_t)x) != reference ||
	    clz((int64_t)x) != reference) {
		return false;
	}
	return true;
}

RANDOM_TEST(clz64_rand, 1u << 30)
{
	uint64_t x = random_seed;
	unsigned int reference = hackers_delight_clz64(x);
	if (clz((uint64_t)x) != reference ||
	    clz((int64_t)x) != reference) {
		return false;

	}
	return true;
}

static unsigned int hackers_delight_ctz32(uint32_t x)
{
	static _Alignas(64) const char table[64] = {
#define u 0
		32, 0, 1, 12, 2, 6, u, 13, 3, u, 7, u, u, u, u, 14,
		10, 4, u, u, 8, u, u, 25, u, u, u, u, u, 21, 27, 15,
		31, 11, 5, u, u, u, u, u, 9, u, u, 24, u, u, 20, 26,
		30, u, u, u, u, 23, u, 19, 29, u, 22, 18, 28, 17, 16, u
#undef u
	};
	x = (x & -x) * 0x0450FBAF;
	return table[x >> 26];
}

static unsigned int hackers_delight_ctz64(uint64_t x)
{
	unsigned int n = hackers_delight_ctz32(x);
	if (n == 32) {
		n += hackers_delight_ctz32(x >> 32);
	}
	return n;
}

RANGE_TEST(ctz32, 0, UINT32_MAX)
{
	uint64_t x = value;
	unsigned int reference = hackers_delight_ctz32(x);
	if (ctz((uint32_t)x) != reference ||
	    ctz((int32_t)x) != reference) {
		return false;
	}
	return true;
}

RANGE_TEST(ctz64, 0, UINT32_MAX)
{
	uint64_t x = value;
	unsigned int reference = hackers_delight_ctz64(x);
	if (ctz((uint64_t)x) != reference ||
	    ctz((int64_t)x) != reference) {
		return false;
	}

	return true;
}

RANDOM_TEST(ctz64_rand, 1u << 30)
{
	uint64_t x = random_seed;
	unsigned int reference = hackers_delight_ctz64(x);
	if (ctz((uint64_t)x) != reference ||
	    ctz((int64_t)x) != reference) {
		return false;
	}
	return true;
}

static unsigned int reference_ffs32(uint32_t x)
{
	unsigned int ctz = ctz(x);
	return ctz == 32 ? 0 : ctz + 1;
}

static unsigned int reference_ffs64(uint64_t x)
{
	unsigned int ctz = ctz(x);
	return ctz == 64 ? 0 : ctz + 1;
}

RANGE_TEST(ffs32, 0, UINT32_MAX)
{
	uint64_t x = value;
	unsigned int reference = reference_ffs32(x);
	if (ffs((uint32_t)x) != reference ||
	    ffs((int32_t)x) != reference) {
		return false;
	}

	return true;
}

RANGE_TEST(ffs64, 0, UINT32_MAX)
{
	uint64_t x = value;
	unsigned int reference = reference_ffs64(x);
	if (ffs((uint64_t)x) != reference ||
	    ffs((int64_t)x) != reference) {
		return false;
	}
	return true;
}

RANDOM_TEST(ffs64_rand, 1u << 30)
{
	uint64_t x = random_seed;
	unsigned int reference = reference_ffs64(x);
	if (ffs((uint64_t)x) != reference ||
	    ffs((int64_t)x) != reference) {
		return false;
	}
	return true;
}

static unsigned int hackers_delight_popcount32(uint32_t x)
{
	x = x - ((x >> 1) & 0x55555555);
	x = (x & 0x33333333) + ((x >> 2) & 0x33333333);
	x = (x + (x >> 4)) & 0x0F0F0F0F;
	x = x + (x >> 8);
	x = x + (x >> 16);
	return x & 0x0000003F;
}

static unsigned int hackers_delight_popcount64(uint64_t x)
{
	return hackers_delight_popcount32(x) + hackers_delight_popcount32(x >> 32);
}

RANGE_TEST(popcount32, 0, UINT32_MAX)
{
	uint64_t x = value;
	unsigned int reference = hackers_delight_popcount32(x);
	if (popcount((uint32_t)x) != reference ||
	    popcount((int32_t)x) != reference) {
		return false;
	}
	return true;
}

RANGE_TEST(popcount64, 0, UINT32_MAX)
{
	uint64_t x = value;
	unsigned int reference = hackers_delight_popcount64(x);
	if (popcount((uint64_t)x) != reference ||
	    popcount((int64_t)x) != reference) {
		return false;
	}
	return true;
}

RANDOM_TEST(popcount64_rand, 1u << 30)
{
	uint64_t x = random_seed;
	unsigned int reference = hackers_delight_popcount64(x);
	if (popcount((uint64_t)x) != reference ||
	    popcount((int64_t)x) != reference) {
		return false;
	}
	return true;
}

SIMPLE_TEST(minmax)
{
#define MINMAX_TEST(type)						\
	do {								\
		_Static_assert(types_are_compatible(min((type)-1, (type)0), type), "");	\
		_Static_assert(types_are_compatible(max((type)-1, (type)0), type), "");	\
		_Static_assert(types_are_compatible(min_t(type, (type)-1, (type)0), type), ""); \
		_Static_assert(types_are_compatible(max_t(type, (type)-1, (type)0), type), ""); \
		for (int i = CHAR_MIN; i <= CHAR_MAX; i++) {		\
			for (int j = CHAR_MIN; j <= CHAR_MAX; j++) {	\
				type a = (type)i;			\
				type b = (type)j;			\
				type min_val = min_t(type, a, b);	\
				type max_val = max_t(type, a, b);	\
				CHECK(min(a, b) == min_val);		\
				CHECK(max(a, b) == max_val);		\
				CHECK(min_val == (a < b ? a : b));	\
				CHECK(max_val == (a > b ? a : b));	\
			}						\
		}							\
	} while (0)

	MINMAX_TEST(char);
	MINMAX_TEST(unsigned char);
	MINMAX_TEST(unsigned short);
	MINMAX_TEST(unsigned int);
	MINMAX_TEST(unsigned long);
	MINMAX_TEST(unsigned long long);
	MINMAX_TEST(signed char);
	MINMAX_TEST(signed short);
	MINMAX_TEST(signed int);
	MINMAX_TEST(signed long);
	MINMAX_TEST(signed long long);

	return true;
}

RANDOM_TEST(bswap, 1u << 16)
{
	uint64_t random = random_seed;

#define CHECK_BSWAP_SIGNEDNESS(type)					\
	CHECK(bswap((unsigned type)random) == (unsigned type)bswap((signed type)random));

	CHECK_BSWAP_SIGNEDNESS(short);
	CHECK_BSWAP_SIGNEDNESS(int);
	CHECK_BSWAP_SIGNEDNESS(long);
	CHECK_BSWAP_SIGNEDNESS(long long);

#define CHECK_BSWAP_TYPE(type) CHECK(_Generic(bswap((type)0), type: 1))
	CHECK_BSWAP_TYPE(unsigned short);
	CHECK_BSWAP_TYPE(unsigned int);
	CHECK_BSWAP_TYPE(unsigned long);
	CHECK_BSWAP_TYPE(unsigned long long);
	CHECK_BSWAP_TYPE(signed short);
	CHECK_BSWAP_TYPE(signed int);
	CHECK_BSWAP_TYPE(signed long);
	CHECK_BSWAP_TYPE(signed long long);

#define CHECK_BSWAP(type)						\
	do {								\
		union {							\
			type val;					\
			char bytes[sizeof(type)];			\
		} normal = {.val = (type)random};			\
		union {							\
			type val;					\
			char bytes[sizeof(type)];			\
		} reversed = {.val = bswap((type)random)};		\
		for (unsigned int i = 0; i < sizeof(type); i++) {	\
			CHECK(normal.bytes[i] == reversed.bytes[sizeof(type) - i - 1]);	\
		}							\
	} while (0)

	CHECK_BSWAP(unsigned short);
	CHECK_BSWAP(unsigned int);
	CHECK_BSWAP(unsigned long);
	CHECK_BSWAP(unsigned long long);

	return true;
}

RANDOM_TEST(endianness, 1u << 16)
{
	uint64_t random = random_seed;

#define CHECK_CPU_TO_LE_TYPE(bits) CHECK(_Generic(cpu_to_le((uint##bits##_t)0), le##bits##_t: 1))
	CHECK_CPU_TO_LE_TYPE(16);
	CHECK_CPU_TO_LE_TYPE(32);
	CHECK_CPU_TO_LE_TYPE(64);
#define CHECK_LE_TO_CPU_TYPE(bits) CHECK(_Generic(le_to_cpu((le##bits##_t){0}), uint##bits##_t: 1))
	CHECK_LE_TO_CPU_TYPE(16);
	CHECK_LE_TO_CPU_TYPE(32);
	CHECK_LE_TO_CPU_TYPE(64);
#define CHECK_CPU_TO_BE_TYPE(bits) CHECK(_Generic(cpu_to_be((uint##bits##_t)0), be##bits##_t: 1))
	CHECK_CPU_TO_BE_TYPE(16);
	CHECK_CPU_TO_BE_TYPE(32);
	CHECK_CPU_TO_BE_TYPE(64);
#define CHECK_BE_TO_CPU_TYPE(bits) CHECK(_Generic(be_to_cpu((be##bits##_t){0}), uint##bits##_t: 1))
	CHECK_BE_TO_CPU_TYPE(16);
	CHECK_BE_TO_CPU_TYPE(32);
	CHECK_BE_TO_CPU_TYPE(64);

#define CHECK_BE_ROUNDTRIP(bits) CHECK(be##bits##_to_cpu(cpu_to_be##bits(random)) == (uint##bits##_t)random)
	CHECK_BE_ROUNDTRIP(16);
	CHECK_BE_ROUNDTRIP(32);
	CHECK_BE_ROUNDTRIP(64);
#define CHECK_LE_ROUNDTRIP(bits) CHECK(le##bits##_to_cpu(cpu_to_le##bits(random)) == (uint##bits##_t)random)
	CHECK_LE_ROUNDTRIP(16);
	CHECK_LE_ROUNDTRIP(32);
	CHECK_LE_ROUNDTRIP(64);

	const bool is_little_endian = (const union {uint16_t i; uint8_t c;}){.i = 1}.c == 1;
	if (is_little_endian) {
#define CHECK_LE_IDENTITY(bits) CHECK(cpu_to_le##bits(random).val == (uint##bits##_t)random)
		CHECK_LE_IDENTITY(16);
		CHECK_LE_IDENTITY(32);
		CHECK_LE_IDENTITY(64);
#define CHECK_BE_BSWAP(bits) CHECK(cpu_to_be##bits(random).val == bswap((uint##bits##_t)random))
		CHECK_BE_BSWAP(16);
		CHECK_BE_BSWAP(32);
		CHECK_BE_BSWAP(64);
	} else {
#define CHECK_BE_IDENTITY(bits) CHECK(cpu_to_be##bits(random).val == (uint##bits##_t)random)
		CHECK_BE_IDENTITY(16);
		CHECK_BE_IDENTITY(32);
		CHECK_BE_IDENTITY(64);
#define CHECK_LE_BSWAP(bits) CHECK(cpu_to_le##bits(random).val == bswap((uint##bits##_t)random))
		CHECK_LE_BSWAP(16);
		CHECK_LE_BSWAP(32);
		CHECK_LE_BSWAP(64);
	}

	return true;
}

SIMPLE_TEST(min_max_value)
{
#define CHECK_MIN_MAX_SIGNED(type)					\
	do {								\
		type min = (type)(1llu << (sizeof(type) * 8 - 1));	\
		type max = ~min;					\
		CHECK(min_value(type) == min);				\
		CHECK(min_value(min) == min);				\
		CHECK(max_value(type) == max);				\
		CHECK(max_value(max) == max);				\
		CHECK(_Generic(min_value((type)0), type : 1));		\
		CHECK(_Generic(max_value((type)0), type : 1));		\
	} while (0)

#define CHECK_MIN_MAX_UNSIGNED(type)				\
	do {							\
		type min = 0;					\
		type max = ~min;				\
		CHECK(min_value(type) == min);			\
		CHECK(min_value(min) == min);			\
		CHECK(max_value(type) == max);			\
		CHECK(max_value(max) == max);			\
		CHECK(_Generic(min_value((type)0), type : 1));	\
		CHECK(_Generic(max_value((type)0), type : 1));	\
	} while (0)

#if CHAR_MIN < 0
	CHECK_MIN_MAX_SIGNED(char);
#endif
	CHECK_MIN_MAX_SIGNED(signed char);
	CHECK_MIN_MAX_SIGNED(signed short);
	CHECK_MIN_MAX_SIGNED(signed int);
	CHECK_MIN_MAX_SIGNED(signed long);
	CHECK_MIN_MAX_SIGNED(signed long long);

#if CHAR_MIN == 0
	CHECK_MIN_MAX_UNSIGNED(char);
#endif
	CHECK_MIN_MAX_UNSIGNED(unsigned char);
	CHECK_MIN_MAX_UNSIGNED(unsigned short);
	CHECK_MIN_MAX_UNSIGNED(unsigned int);
	CHECK_MIN_MAX_UNSIGNED(unsigned long);
	CHECK_MIN_MAX_UNSIGNED(unsigned long long);

	return true;
}

SIMPLE_TEST(to_unsigned)
{
#define CHECK_TO_UNSIGNED_SIGNED(type, unsigned_type)			\
	do {								\
		type val = -1;						\
		unsigned_type uval = val;				\
		CHECK(to_unsigned(val) == uval);			\
		CHECK((unsigned long long)to_unsigned(val) == uval);	\
		CHECK(_Generic(to_unsigned((type)0), unsigned_type : 1)); \
		CHECK(_Generic((to_unsigned_type(type))0, unsigned_type : 1)); \
	} while (0)

#define CHECK_TO_UNSIGNED_UNSIGNED(type)				\
	do {								\
		type val = -1;						\
		CHECK(to_unsigned(val) == val);				\
		CHECK(_Generic(to_unsigned((type)0), type : 1));	\
		CHECK(_Generic((to_unsigned_type(type))0, type : 1));	\
	} while (0)

#if CHAR_MIN < 0
	CHECK_TO_UNSIGNED_SIGNED(char, unsigned char);
#endif
	CHECK_TO_UNSIGNED_SIGNED(signed char, unsigned char);
	CHECK_TO_UNSIGNED_SIGNED(signed short, unsigned short);
	CHECK_TO_UNSIGNED_SIGNED(signed int, unsigned int);
	CHECK_TO_UNSIGNED_SIGNED(signed long, unsigned long);
	CHECK_TO_UNSIGNED_SIGNED(signed long long, unsigned long long);

#if CHAR_MIN == 0
	CHECK_TO_UNSIGNED_UNSIGNED(char);
#endif
	CHECK_TO_UNSIGNED_UNSIGNED(unsigned char);
	CHECK_TO_UNSIGNED_UNSIGNED(unsigned short);
	CHECK_TO_UNSIGNED_UNSIGNED(unsigned int);
	CHECK_TO_UNSIGNED_UNSIGNED(unsigned long);
	CHECK_TO_UNSIGNED_UNSIGNED(unsigned long long);

	return true;
}

SIMPLE_TEST(to_signed)
{
#define CHECK_TO_SIGNED_UNSIGNED(type, signed_type)			\
	do {								\
		type val = -1;						\
		signed_type sval = val;					\
		CHECK(to_signed(val) == sval);				\
		CHECK((long long)to_signed(val) == sval);		\
		CHECK(_Generic(to_signed((type)0), signed_type : 1));	\
		CHECK(_Generic((to_signed_type(type))0, signed_type : 1)); \
	} while (0)

#define CHECK_TO_SIGNED_SIGNED(type)					\
	do {								\
		type val = -1;						\
		CHECK(to_signed(val) == val);				\
		CHECK(_Generic(to_signed((type)0), type : 1));		\
		CHECK(_Generic((to_signed_type(type))0, type : 1));	\
	} while (0)

#if CHAR_MIN == 0
	CHECK_TO_SIGNED_UNSIGNED(char, signed char);
#endif
	CHECK_TO_SIGNED_UNSIGNED(unsigned char, signed char);
	CHECK_TO_SIGNED_UNSIGNED(unsigned short, signed short);
	CHECK_TO_SIGNED_UNSIGNED(unsigned int, signed int);
	CHECK_TO_SIGNED_UNSIGNED(unsigned long, signed long);
	CHECK_TO_SIGNED_UNSIGNED(unsigned long long, signed long long);

#if CHAR_MIN < 0
	CHECK_TO_SIGNED_SIGNED(char);
#endif
	CHECK_TO_SIGNED_SIGNED(signed char);
	CHECK_TO_SIGNED_SIGNED(signed short);
	CHECK_TO_SIGNED_SIGNED(signed int);
	CHECK_TO_SIGNED_SIGNED(signed long);
	CHECK_TO_SIGNED_SIGNED(signed long long);

	return true;
}

SIMPLE_TEST(overflow8)
{
	for (uint64_t a = 0; a <= UINT8_MAX; a++) {
		for (uint64_t b = 0; b <= UINT8_MAX; b++) {
			{
				uint8_t x = a, y = b, r;
				uint16_t sum = (uint16_t)x + (uint16_t)y;
				bool sum_overflow = sum > UINT8_MAX;
				CHECK(add_overflow(x, y, &r) == sum_overflow);
				CHECK(r == (uint8_t)sum);
				uint8_t difference = x - y;
				bool difference_overflow = y > x;
				CHECK(sub_overflow(x, y, &r) == difference_overflow);
				CHECK(r == difference);
				uint16_t product = (uint16_t)x * (uint16_t)y;
				bool product_overflow = product > UINT8_MAX;
				CHECK(mul_overflow(x, y, &r) == product_overflow);
				CHECK(r == (uint8_t)product);
			}
			{
				int8_t x = a, y = b, r;
				int8_t sum8 = (uint8_t)x + (uint8_t)y;
				int16_t sum16 = (int16_t)x + (int16_t)y;
				bool sum_overflow = sum8 != sum16;
				CHECK(add_overflow(x, y, &r) == sum_overflow);
				CHECK(r == sum8);
				int8_t difference8 = (uint8_t)x - (uint8_t)y;
				int16_t difference16 = (int16_t)x - (int16_t)y;
				bool difference_overflow = difference8 != difference16;
				CHECK(sub_overflow(x, y, &r) == difference_overflow);
				CHECK(r == difference8);
				int16_t product = (int16_t)x * (int16_t)y;
				bool product_overflow = product < INT8_MIN || product > INT8_MAX;
				CHECK(mul_overflow(x, y, &r) == product_overflow);
				CHECK(r == (int8_t)product);
			}
		}
	}
	return true;
}

RANGE_TEST(overflow16, 0, UINT16_MAX)
{
	uint64_t a = value;
	for (uint64_t b = 0; b <= UINT16_MAX; b++) {
		{
			uint16_t x = a, y = b, r;
			uint32_t sum = (uint32_t)x + (uint32_t)y;
			bool sum_overflow = sum > UINT16_MAX;
			CHECK(add_overflow(x, y, &r) == sum_overflow);
			CHECK(r == (uint16_t)sum);
			uint16_t difference = x - y;
			bool difference_overflow = y > x;
			CHECK(sub_overflow(x, y, &r) == difference_overflow);
			CHECK(r == difference);
			uint32_t product = (uint32_t)x * (uint32_t)y;
			bool product_overflow = product > UINT16_MAX;
			CHECK(mul_overflow(x, y, &r) == product_overflow);
			CHECK(r == (uint16_t)product);
		}
		{
			int16_t x = a, y = b, r;
			int16_t sum16 = (uint16_t)x + (uint16_t)y;
			int32_t sum32 = (int32_t)x + (int32_t)y;
			bool sum_overflow = sum16 != sum32;
			CHECK(add_overflow(x, y, &r) == sum_overflow);
			CHECK(r == sum16);
			int16_t difference16 = (uint16_t)x - (uint16_t)y;
			int32_t difference32 = (int32_t)x - (int32_t)y;
			bool difference_overflow = difference16 != difference32;
			CHECK(sub_overflow(x, y, &r) == difference_overflow);
			CHECK(r == difference16);
			int32_t product = (int32_t)x * (int32_t)y;
			bool product_overflow = product < INT16_MIN || product > INT16_MAX;
			CHECK(mul_overflow(x, y, &r) == product_overflow);
			CHECK(r == (int16_t)product);
		}
	}
	return true;
}

static const uint64_t test_cases[] = {
	0,
	1,
	2,
	3,
	4,
	5,
	6,
	7,
	8,
	9,

	UINT8_MAX,
	INT8_MIN,
	INT8_MAX,
	UINT16_MAX,
	INT16_MIN,
	INT16_MAX,
	UINT32_MAX,
	INT32_MIN,
	INT32_MAX,
	UINT64_MAX,
	INT64_MIN,
	INT64_MAX,

	UINT8_MAX + 1u,
	UINT8_MAX - 1u,
	INT8_MIN + 1u,
	INT8_MIN - 1u,
	INT8_MAX + 1u,
	INT8_MAX - 1u,
	UINT16_MAX + 1u,
	UINT16_MAX - 1u,
	INT16_MIN + 1u,
	INT16_MIN - 1u,
	INT16_MAX + 1u,
	INT16_MAX - 1u,
	UINT32_MAX + 1u,
	UINT32_MAX - 1u,
	INT32_MIN + 1u,
	INT32_MIN - 1u,
	INT32_MAX + 1u,
	INT32_MAX - 1u,
	UINT64_MAX - 1llu,
	INT64_MIN + 1llu,
	INT64_MIN - 1llu,
	INT64_MAX + 1llu,
	INT64_MAX - 1llu,

	0xac9a4d148fd0fd85,
	0x39d3550adece9015,
	0xe774d919a479b1f6,
	0xb8e33802737ed779,
	0x2f9e5d2dd152c5cb,
	0x96f0721828bc3dda,
	0x4f2a9110583cd0c9,
	0x61b4899942731ca4,
	0xb913372e3acc65a7,
	0x509bc3c54d8f3695,
	0x19c144f2457c63f8,
	0x847f1ffb1905a03a,
	0xeb9694914a084bb3,
	0xd70fd341b29d550f,
	0x5e313924c736d5df,
	0xfb680d189491c013,
	0x1137876f7cd1b918,
	0xcaa92603367e9b60,
	0xddc0530b5b217f5e,
	0x69c2e990674e91bd,
	0x849faf1e7ccb91ef,
	0x8c5fd7a04913d587,
	0x9ea96ca15cf2c82,
	0x7be2ad9cc887378c,
	0x6d9a03124c77dd85,
	0x77ed0d00ebfa2455,
	0xb830961914b25f0,
	0x1cfa8a0db88dcb02,
	0x3cdfc888cb56f1e5,
	0x4b1b5f12ede25e01,
	0x6c660a802ce374c9,
	0xb9d91395190ac988,
	0xb24e64ff213e157c,
	0x719fcd9e0004df74,
	0xdbddc68d42972e26,
	0xcc36c57cd86fe4a0,
	0x546643c39862065d,
	0xcab1a90ac08ad1a6,
	0x5882b983bdd07edc,
	0x94677f3994a40013,
	0xa8e33d01d8213f7,
	0xfa4936f0064ec6a1,
	0xd100905ebbd838a6,
	0x702881c931049f8b,
	0x491c1472e5257a11,
	0xd1399493da70de5c,
	0x5162ef5453677d69,
	0xdbbebb28930e6ecb,
	0x6371a8e14f99db2c,
	0xa22f3b2ee309860c,
	0xaae0cdc7d9bc2100,
	0x8c48ba721ce41585,
	0x8aad1497fba1ab00,
	0x202bc75655e420d6,
	0x5a486bb76ec11963,
	0xe3df6b6eb78ecd88,
	0x6b5909f52d7bebeb,
	0xc2e395ea45da3ac9,
	0xd14d75df42182a89,
	0x21ef9b8db4d29989,
	0x41b915984f1a84d2,
	0x513a235006501b68,
	0x1a0bc9adf7362c33,
	0x25059b79e112f7f1,
	0xa28dc33b8e6400f9,
	0xc948812cd8a12893,
	0x1e3150ad34826c19,
	0x50844988fd48ad17,
	0x5a92bc16c7ad94d8,
	0xf3ab22a3a815a674,
	0xf8a467c3d56f3843,
	0x4b713b0fe368890c,
	0xaeb223ef2429d1da,
	0x5e770645901d2885,
	0x44e471137d373aa2,
	0x45d7619f016a482f,
	0x72c724e5ed557881,
	0x6895e6aeea0b37fd,
	0x281d951b40292fae,
	0x89ed5ec8383b40df,
	0xcef377d1f08946e0,
	0x2f870afdb379ebda,
	0xb7c562260a025c54,
	0x19561aa219f02013,
	0x29314599653aef03,
	0x53e782930bb56a18,
	0x9288b2991b2541c0,
	0xd7e23f7991fdf3e0,
	0x863f2f5eaf63f25b,
	0xc0a126fdaea421f7,
	0x95cda3f4ee34a0a0,
	0xd21b4fe3d9d93ae9,
	0xa45c2cd2fa40a960,
	0x5f26d31c39dd3b9d,
	0xe90eff6f8405a9e5,
	0x652b20664b02bdb9,
	0x18333a54ef2ba896,
	0x854218e7dcfd4e1,
	0x14330715aa21117b,
	0x179f918d4a085387,
	0x6145053d4f6c72ce,
	0x1df26fe5caafc8aa,
	0x51ec0cee88b71c51,
	0xfec632338c887602,
	0xc94b59b2f4009b52,
	0xfba02431a1fe5508,
	0xbc9b93ded959879e,
	0xa3cb3fce69e629cc,
	0x838c1a59fa14b97e,
	0x331edc6dfb886ac,
	0x549b503c933daf90,
	0x7c997d3a7cdad62c,
	0x6421c1271c99bbb1,
	0xaf940f66f7907b29,
	0xf74235badd585f36,
	0x602879084bf2f23a,
	0xc0e176b191859c9c,
	0xccfa06fb89947861,
	0x7afbec73b06950f3,
	0x674b7725b20b60b9,
	0xe847a7206ab34645,
	0xed9ecdc66f097041,
	0xfc8ccb289dae60bf,
	0xf4f22407e95340ec,
	0x58b55b324564846a,
	0x17267214dc84963b,
	0xdef8d6d8536e6b22,
	0x108f15b366a4bc80,
	0x50a417ae4309895a,
	0x5926f475467062d9,
	0xb0e5968189454ae1,
	0x706926e75545947c,
	0x84f0b3df56792c58,
	0xa561d8d9c1970718,
	0x656e96754d2eac85,
	0xd9a92a68fae3619e,
	0x6ae05d7b4c7d38cd,
	0xa4e4404582fc0b5e,
	0x7d098c37fac0a209,
	0xc1424a0876e26bdb,
	0xc0a96a48f900a7b7,
	0x1d9072eeb3bfaf4,
	0x1b5f366ab4bfdc99,
	0xf1dca35ad2b3e5ef,
	0x5b279bc41f460c54,
	0x634d9d4307d36303,
	0x6d364d1e0fdfb19d,
	0x8e3ca4eb7bdc4dd3,
	0x3da0ee02e51fee46,
	0xa79869d06546f88a,
	0x793c670913db03b2,
	0xa675d3470f5d55ab,
	0xc898d8cb368aeadc,
	0x36639d780ef31861,
	0xcf58a0c350e2911c,
	0xbb4f68511fd7db90,
	0x916c66bc54f9d81e,
	0xa6c67983d480a7a9,
	0xdf96aa45e3ca7fdd,
	0x7d3dedf1565fc15b,
	0x2a266258d49addf5,
	0x513885a3684ed6ea,
	0x28c7e808178e832b,
	0x90aea9a1ff8fd9d9,
	0xb54fe0aaf5c36988,
	0xc9f87a052032439a,
	0xaebaf5f8e89e51b2,
	0xe5ba05f08837ed36,
	0x41e86f58379a3afc,
	0xfda70133767265af,
	0xa63761ce44e58dbc,
	0x4535dc43a12bfdfe,
	0xba29f586ef9af81d,
	0xe825297387270f1c,
	0xcedaca1fe1cd7625,
	0xc475f1b9f2e2f081,
	0x21e23340de9ef2c5,
	0x80c39f492d607e8f,
	0xbec023c4fca04595,
	0x9690ae05d3a27d86,
	0xb487caefbb7d401a,
	0x49c2e199670c4b42,
	0xfcaffeb442126596,
	0x70c5f30a1d3e03b1,
	0x890075f187a7fd95,
	0x27599cceb4d0b413,
	0xc541ee66b2f9f48f,
	0x52cf4f21bb4cdaa7,
	0x33269d4189370ed0,
	0x964b16c1c3fc9a34,
	0x4450fd42401aa13c,
	0x350bea9c54996805,
	0x117224e959b7daba,
	0x7058a560b4c4ff62,
	0x72ce4f24a6d682e9,
	0x94369b65cd2cfbab,
	0xcfc4b9a98056d100,
	0x426a77953c77a13f,
	0x8c566e87270316fb,
	0xbf4dc37768ecd844,
	0xef7e62a809c64cff,
	0x6d7a7c3f7896aa67,
	0x1def8a23b302d29e,
	0xbd0dc1a3245d7c29,
	0xfba16b2dd3cde6ad,
	0x30f79d5c95085ae8,
	0x225323d909f340ca,
	0xd679d97623b47e7f,
	0x4ac701619c6122b7,
	0x591f094603e8c578,
	0xe3e6c0dc322d503d,
	0xbd2dab2c09d32f4,
	0xb4ca37945abbd562,
	0x3092d325e739cbfe,
	0xf7d9d4eec65edc,
	0x5e10893238360e1c,
	0x635d8bb265bb8305,
	0x36e9f53febf0cdd3,
	0xc5d4e3a75ddd4254,
	0xe557c7cf67846e00,
	0x25543d7b285702f7,
	0xe622308d44af470e,
	0x850a3f5245e33e1e,
	0x919a43852f4fc5a3,
	0x9d58c8048d710cad,
	0x881795a539297b1f,
	0xf9be76f49a83bdc3,
	0xdaa78f2184ca1d26,
	0x9bfa9aae18dbfa44,
	0xf31997701ac593e4,
	0x40df98d2cdf54425,
	0x96b3e641fd18e1c4,
	0x43671b13736c788e,
	0x34dc8d2ce6b5f002,
	0x9f3ffc7d5f266251,
	0xaed9fc96f28de1a1,
	0x34ae3c9840b42d6f,
	0x50e7264dac22b932,
	0xc0396d2448bac9a4,
	0xc1dafd16bb598d17,
	0x21f907e708b5ac1b,
	0xa1c055be71c05e37,
	0x8b4c216115bad1a6,
	0xb9e70a38d7087848,
	0x5d9f82c604553a04,
	0xcb152a9a8f479421,
	0x2f78bbd08bddbbc7,
	0x3a1372600c9554ed,
	0xac0cd2b754b34e87,
	0x5a40a9e1cda75d93,
	0x98a00e145866bee,
	0xc466965f6589876d,
	0x686591c27f61b773,
	0x7b32324f6fdeedb3,
	0x6bae06f6e7684166,
	0xa738a7dde3c92947,
	0x5b306ca83e87ccfb,
	0x76c5776d0a055721,
	0x8ba53e474c73bc1a,
	0xa41fe1f0e22464ff,
	0x9e6227772712e44b,
	0x3993907a5c559135,
	0xff7bb63d82fd50ae,
	0xac03621972614e5b,
	0xd291d20701d5e8a6,
	0x7c9dcd43589b28c8,
	0x992d1cd624aaca13,
	0xadd02cf2bbf7e031,
	0x8ebc114e2db4e35f,
	0x8b25e0453183d54d,
	0xac94fb84385268b9,
	0x2e9802febfadcf4b,
	0xcbe8e8ee955d61be,
	0x7770a4b20e4f5ec3,
	0x2f79a65a5986cb1,
	0xddfae9c06fa649bf,
	0xd2d6ce6860a35c43,
	0x7ea773384042c25c,
	0xfb52b7aa7eed5504,
	0x2a5b4bbe3027d28a,
	0x7360a5df59cd298b,
	0xfdde78e2321121f9,
	0xd94316738130fc96,
	0xcce7fe4762ef2361,
	0x9f59b6dfbce069dc,
	0x515d8932dbb110e7,
	0xf5ad1a892579ce01,
	0x381cef5d01401636,
	0x83c9cda51d5170a5,
	0xedc98f863bb360fa,
	0x1beaba146ef58c27,
	0xa6af2fe4965263de,
	0xd6f5756dfecad5b8,
	0xc4a03586c6d6e0c8,
	0x61738c5ca0ec162f,
	0xcf39070be413eab4,
	0xf85e610802687d9f,
	0xca2f80c7462bbc1,
	0x2dc6291af5b9f1b6,
	0xf76950a5eb3b1c1e,
	0x9f57569857aa9f8,
	0xd9687f3f9e5706ca,
	0xd9f9c3dc8f660336,
	0x257274d8644150ac,
	0x5ca724cab5794672,
	0x15c4cc398e0cce2f,
	0xac660deab460f785,
	0xde9ae822a94c6e7,
	0xa95460dd18242527,
	0x23ec7bc264bfa563,
	0x25b48c946672b9ad,
	0x915abd5c19ffc8c3,
	0x2e5af3bac4c7433a,
	0xef77546ec97b2ec5,
	0x4f757e85c9c52a64,
	0x5ce10120d85803a3,
	0x412623cd785f5de3,
	0x4e0d4677065cada5,
	0x24bffc993f7ffabe,
	0x3c8a3d1e41af61b9,
	0xc194fa62c8f2112,
	0x9f31c7b994ec7ae2,
	0x7581e25656478780,
	0xe62725e7230c64f9,
	0x72f1cabb281d6d51,
	0xa2dc9df21102da8e,
	0x6b83a2c281e0858a,
	0x31106676ed5e8d37,
	0xd553b6b3ab847a19,
	0x274c42f3686c06b9,
	0x41bcbaf78cf9bea6,
	0xef37bcc0f680c4d5,
	0x2bddce37802d1985,
	0xc016f05fc032ec44,
	0x964db820613d4a39,
	0xcac10bd391edf885,
	0xd858fff300d7da56,
	0x6124dfe9989f0030,
	0x26d0337817acfa70,
	0x20036bb24b035a70,
	0x93bf5607ef930829,
	0x7d1c008dd2855ec3,
	0x7c829d10b3fa096b,
	0xbdb43a6c489f7fa0,
	0x8735edb5d06381cb,
	0x365a230b46e066e9,
	0xa4c9d04914623f26,
	0x38e5419fff42dd7,
	0x2de193ee9e0fe27,
	0x6b7c6749bada5dc5,
	0xe34cfaad9b2cd87,
	0xd296e0a4da4ea1f7,
	0xa51570d45e35fb97,
	0xf5c2592721bc59be,
	0xe98d940842144893,
	0x5d3c362bde018126,
	0xd5ade9c85248ec33,
	0xabd71c75047ee070,
	0x6bc55d4c1b9dafc0,
	0x66535441229ed292,
	0xad4a927d45f5542d,
	0xeca3c7bd9ca59b5f,
	0x1e84ade1819f1cd,
	0xbbd133c6674eb212,
	0x9d853f3084d9b5ab,
	0x7cf2f777d253f435,
	0xde665c6110d50d75,
	0x15d7559a86fe35f9,
	0xabc3635aac5ad65a,
	0x53d8eb6b8e3afefd,
	0x657ecea881b79bcf,
	0xd53a2d84be7cc5e,
	0x55a4ac282f933d78,
	0x5ceb5a83274f9f2b,
	0x33d47506e784b759,
	0x8858cefe2d35504f,
	0x36c009a7033b18e1,
	0x5c8776678755b9ba,
	0xfe0e790f1316f4ff,
	0x929f8c8e0f7a6267,
	0x63cb2c46edb27828,
	0x328d261d589faec5,
	0x626aecdc9b844075,
	0x6548a79709fb00db,
	0x65d15f15f617d9c5,
	0x5d2f98956917eff1,
	0xfbe2af9b1955319d,
	0x28179b38c17309c7,
	0x707cb536ab1b3934,
	0x7f8cad5ad875c36f,
	0xab7e9ef21b68a3c0,
	0x47c3d6aee62fe783,
	0x3135a505d7a227e0,
	0x516331ca9b15a263,
	0x69b01de6d2f57f91,
	0x989b65d33f925bca,
	0xd99571cc587b11de,
	0x3bed5e92836beb58,
	0x5b854bac29f629e0,
	0x411a7b183f0697a4,
	0xe7caea72d08355a3,
	0xb7051fb04617e68,
	0x1330e95574f9e7a3,
	0x39c9ae1037477dd7,
	0x6cc4bf0edc720d19,
	0x62db0fce86a150cd,
	0x43e1ea7912cb6ff4,
	0x7cdcbd8d22210cf9,
	0xd4a5b299d373ffb9,
	0x5e817ed51be0b279,
	0xddf0052e8f440ccb,
	0xbf0ab6d0898d0bf2,
	0xd2cecf23646fecc6,
	0xbc31f0dd632e45b8,
	0x2a0b494de72c47d5,
	0x370a6a84fc66c574,
	0x6aeec535f269d181,
	0xb8cdedfb7a1baf67,
	0x2fcd678c5674c402,
	0xb52ee8eb8f33f953,
	0xe128fc2934d61a1b,
	0x7cc58c708a9cec13,
	0xfe2ca3cab6c58134,
	0x89ee8c8884bde618,
	0xab06d65bc3c770df,
	0xef7165e79ff8ac75,
	0x3c4b526a0f31e755,
	0xe89f3645a13337fe,
	0xf41ef1f944aa0a3f,
	0xf259d95620aa80d9,
	0xcd97158a53551a3e,
	0x7c0b891e6a57e5da,
	0x23c6596b9cb5a50a,
	0x7162b5ca41d55159,
	0xc07c2bd314a0c604,
	0xc78985aaa9017f21,
	0x232a70fd6e9427dc,
	0xb01c28ed89015080,
	0xf2ff0decc36b1aa7,
	0x527219a2a7b8a52a,
	0xec6fc599f2b0c48d,
	0x9971f8c013e1663,
	0xf4ff884b3ca8a8d8,
	0x7c21baba9b57edcb,
	0xce41f952b1130224,
	0x536903337d00f584,
	0x9c981d7dac89e1f5,
	0x9de2412e9317bf57,
	0x70b18d6261d223af,
	0x3fb997917dac4fb6,
	0x27262c2fb076844b,
	0x6ef42054c11bdeca,
	0x6d62bf75e74163e4,
	0x229e7b782374698a,
	0x7ba339a2ac2a19b5,
	0x1cfa6629c85ede42,
	0x65140d3030a229de,
	0x7ff3bed3a90e998a,
	0xe288fa2852f51265,
	0xe9da5ec1dc650336,
	0xc514d20bcd61e216,
	0x241bcab2021f79e6,
	0x8ff633f93484acc2,
	0xb50af896539775d7,
	0x719853ac10f47165,
	0x3e723d5d8537053c,
	0x9c6374f2b08c5978,
	0xcd5ac5d03f8de875,
	0xe741c672c6d57224,
	0x4264a26fb71cfd73,
	0x37d037fd0ef4f9ce,
	0x4e990f61e39b0ff0,
	0x492fb24286f79361,
	0x2e3de8f2b5c67112,
	0x3d9e4c8c35856912,
	0x7d5c7ee0932efe1b,
	0x50b8ffa464d9e92e,
	0x66be28f12bf4da7b,
	0xa3ae63176fb8ac5d,
	0xa4dc7f7ad4d0e6b0,
	0x4896c3f315c01999,
	0x88231046d45d3b1c,
	0xc4bdbecfe412e44f,
	0xdda6b1f6bea6a27a,
	0xe44cb0d7f6ce938b,
	0x172281ff530d6072,
	0x640f10ee085b8fc3,
	0x3133b8c5add8af9d,
	0xb167cd3999a30e07,
	0x49cbef0a00109a59,
	0xeec00ba1f0af2306,
	0xd89b603e1e5abe09,
	0xf9c476c8ba133f26,
	0x34673054f979628a,
	0x326a5ece79418feb,
	0x3d7f4fbc5d1e73ed,
	0x1c7ff5a080deb3e9,
	0xd4dac467fb57dbd5,
	0x4f1b5401240eed79,
	0x78f66fb2b839bdd0,
	0x67cf65cd6a7e7671,
	0x22f20f5315e6792d,
	0x4d73307f112bcec8,
	0x47fb92063a53ef0b,
	0xb121afa955f56903,
	0x336e1e7dce88f5c7,
	0xf589e5b441380044,
	0xbb73783aa44d9efc,
	0x3f43846f957473fb,
	0xe924ded4d267252b,
	0x3978992313cfd45a,
	0x4261e08b06d75bc7,
};

static bool check_overflow32(uint64_t a, uint64_t b)
{
	{
		uint32_t x = a, y = b, r;
		uint64_t sum = (uint64_t)x + (uint64_t)y;
		bool sum_overflow = sum > UINT32_MAX;
		CHECK(add_overflow(x, y, &r) == sum_overflow);
		CHECK(r == (uint32_t)sum);
		uint32_t difference = x - y;
		bool difference_overflow = y > x;
		CHECK(sub_overflow(x, y, &r) == difference_overflow);
		CHECK(r == difference);
		uint64_t product = (uint64_t)x * (uint64_t)y;
		bool product_overflow = product > UINT32_MAX;
		CHECK(mul_overflow(x, y, &r) == product_overflow);
		CHECK(r == (uint32_t)product);
	}
	{
		int32_t x = a, y = b, r;
		int32_t sum32 = (uint32_t)x + (uint32_t)y;
		int64_t sum64 = (int64_t)x + (int64_t)y;
		bool sum_overflow = sum32 != sum64;
		CHECK(add_overflow(x, y, &r) == sum_overflow);
		CHECK(r == sum32);
		int32_t difference32 = (uint32_t)x - (uint32_t)y;
		int64_t difference64 = (int64_t)x - (int64_t)y;
		bool difference_overflow = difference32 != difference64;
		CHECK(sub_overflow(x, y, &r) == difference_overflow);
		CHECK(r == difference32);
		int64_t product = (int64_t)x * (int64_t)y;
		bool product_overflow = product < INT32_MIN || product > INT32_MAX;
		CHECK(mul_overflow(x, y, &r) == product_overflow);
		CHECK(r == (int32_t)product);
	}
	return true;
}

SIMPLE_TEST(overflow32)
{
	for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++) {
		for (size_t j = 0; j < sizeof(test_cases) / sizeof(test_cases[0]); j++) {
			uint64_t a = test_cases[i];
			uint64_t b = test_cases[j];
			if (!check_overflow32(a, b)) {
				return false;
			}
		}
	}
	return true;
}

RANDOM_TEST(overflow32_random, 1 << 18)
{
	for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++) {
		if (!check_overflow32(test_cases[i], random_seed)) {
			return false;
		}
		if (!check_overflow32(random_seed, test_cases[i])) {
			return false;
		}
	}
	return true;
}

#ifdef __SIZEOF_INT128__
static bool check_overflow64(uint64_t a, uint64_t b)
{
	__extension__ typedef __int128 int128_t;
	__extension__ typedef unsigned __int128 uint128_t;
	{
		uint64_t x = a, y = b, r;
		uint128_t sum = (uint128_t)x + (uint128_t)y;
		bool sum_overflow = sum > UINT64_MAX;
		CHECK(add_overflow(x, y, &r) == sum_overflow);
		CHECK(r == (uint64_t)sum);
		uint64_t difference = x - y;
		bool difference_overflow = y > x;
		CHECK(sub_overflow(x, y, &r) == difference_overflow);
		CHECK(r == difference);
		uint128_t product = (uint128_t)x * (uint128_t)y;
		bool product_overflow = product > UINT64_MAX;
		CHECK(mul_overflow(x, y, &r) == product_overflow);
		CHECK(r == (uint64_t)product);
	}
	{
		int64_t x = a, y = b, r;
		int64_t sum64 = (uint64_t)x + (uint64_t)y;
		int128_t sum128 = (int128_t)x + (int128_t)y;
		bool sum_overflow = sum64 != sum128;
		CHECK(add_overflow(x, y, &r) == sum_overflow);
		CHECK(r == sum64);
		int64_t difference64 = (uint64_t)x - (uint64_t)y;
		int128_t difference128 = (int128_t)x - (int128_t)y;
		bool difference_overflow = difference64 != difference128;
		CHECK(sub_overflow(x, y, &r) == difference_overflow);
		CHECK(r == difference64);
		int128_t product = (int128_t)x * (int128_t)y;
		bool product_overflow = product < INT64_MIN || product > INT64_MAX;
		CHECK(mul_overflow(x, y, &r) == product_overflow);
		CHECK(r == (int64_t)product);
	}
	return true;
}

SIMPLE_TEST(overflow64)
{
	for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++) {
		for (size_t j = 0; j < sizeof(test_cases) / sizeof(test_cases[0]); j++) {
			uint64_t a = test_cases[i];
			uint64_t b = test_cases[j];
			if (!check_overflow64(a, b)) {
				return false;
			}
		}
	}
	return true;
}

RANDOM_TEST(overflow64_random, 1 << 18)
{
	for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++) {
		if (!check_overflow64(test_cases[i], random_seed)) {
			return false;
		}
		if (!check_overflow64(random_seed, test_cases[i])) {
			return false;
		}
	}
	return true;
}
#endif
