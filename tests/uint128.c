#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "random.h"
#include "testing.h"
#include "uint128.h"

static uint128_t edge_cases[] = {
#define UINT128(h, l) {.high = h, .low = l}
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
#undef UINT128
};

static void print_hexadecimal(void *p, size_t size)
{
	unsigned char *c = p;
	for (size_t i = 0; i < size; i++) {
		test_log("%02x", c[i]);
	}
}

#define check_function(name, ...)					\
	static bool check_##name(uint128_t a, uint128_t b)		\
	{								\
		typedef typeof(uint128_##name(__VA_ARGS__)) result_t;	\
		result_t r1 = _##uint128_##name##_generic(__VA_ARGS__);	\
		result_t r2 = _##uint128_##name##_u128(__VA_ARGS__);	\
		if (memcmp(&r1, &r2, sizeof(r1)) != 0) {		\
			test_log("%s: a=%016" PRIx64 "%016" PRIx64 ", b=%016" PRIx64 "%016" PRIx64 "\n", \
				 #name, a.high, a.low, b.high, b.low);	\
			test_log("got:      ");				\
			print_hexadecimal(&r1, sizeof(r1));		\
			test_log("\nexpected: ");			\
			print_hexadecimal(&r2, sizeof(r2));		\
			test_log("\n");					\
			return false;					\
		}							\
		result_t r3 = uint128_##name(__VA_ARGS__);		\
		return memcmp(&r3, &r1, sizeof(r1)) == 0;		\
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
	f(invert, a)				\
	f(from_high_low_bits, a.low, b.low)	\
	f(from_low_bits, a.low)			\
	f(get_low_bits, a)			\
	f(get_high_bits, a)

foreach_function(check_function)

#define call_check_function(name, ...) CHECK(check_##name(a, b));

SIMPLE_TEST(uint128_edge_cases)
{
	size_t num_edge_cases = sizeof(edge_cases) / sizeof(edge_cases[0]);
	for (size_t i = 0; i < num_edge_cases; i++) {
		for (size_t j = 0; j < num_edge_cases; j++) {
			uint128_t a = edge_cases[i];
			uint128_t b = edge_cases[j];
			foreach_function(call_check_function);
		}
	}
	CHECK(uint128_eq(UINT128_MAX, uint128_max_value()));
	CHECK(uint128_get_low_bits(uint128_zero()) == 0 && uint128_get_high_bits(uint128_zero()) == 0);
	CHECK(uint128_get_low_bits(uint128_one()) == 1 && uint128_get_high_bits(uint128_one()) == 0);
	CHECK(uint128_get_low_bits(uint128_max_value()) == UINT64_MAX &&
	      uint128_get_high_bits(uint128_max_value()) == UINT64_MAX);
	return true;
}

RANDOM_TEST(uint128_random, 2)
{
	struct random_state rng;
	random_state_init(&rng, random_seed);
	static const size_t N = 1 << 26;
	for (size_t i = 0; i < N; i++) {
		uint128_t a = uint128_from_high_low_bits(random_next_u64(&rng), random_next_u64(&rng));
		uint128_t b = uint128_from_high_low_bits(random_next_u64(&rng), random_next_u64(&rng));
		foreach_function(call_check_function);
	}
	return true;
}
