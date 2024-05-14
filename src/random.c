/*
 * Copyright (C) 2024 Fabian Hügel
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <assert.h>
#include "random.h"
#include "uint128.h"

/* http://prng.di.unimi.it/splitmix64.c

   Written in 2015 by Sebastiano Vigna (vigna@acm.org)

   To the extent possible under law, the author has dedicated all copyright
   and related and neighboring rights to this software to the public domain
   worldwide. This software is distributed without any warranty.

   See <http://creativecommons.org/publicdomain/zero/1.0/>.

   This is a fixed-increment version of Java 8's SplittableRandom generator
   See http://dx.doi.org/10.1145/2714064.2660195 and
   http://docs.oracle.com/javase/8/docs/api/java/util/SplittableRandom.html

   It is a very fast generator passing BigCrush, and it can be useful if
   for some reason you absolutely want 64 bits of state.
*/

static uint64_t _random_splitmix64(uint64_t *x)
{
	uint64_t z = (*x += 0x9e3779b97f4a7c15);
	z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
	z = (z ^ (z >> 27)) * 0x94d049bb133111eb;
	return z ^ (z >> 31);
}

void random_state_init(struct random_state *state, uint64_t seed)
{
	uint64_t z, x = seed;
	z = _random_splitmix64(&x);
	state->s[0] = z;
	z = _random_splitmix64(&x);
	state->s[1] = z;
	z = _random_splitmix64(&x);
	state->s[2] = z;
	z = _random_splitmix64(&x);
	state->s[3] = z;
}

/* http://prng.di.unimi.it/xoshiro256starstar.c

   Written in 2018 by David Blackman and Sebastiano Vigna (vigna@acm.org)

   To the extent possible under law, the author has dedicated all copyright
   and related and neighboring rights to this software to the public domain
   worldwide. This software is distributed without any warranty.

   See <http://creativecommons.org/publicdomain/zero/1.0/>.

   This is xoshiro256** 1.0, one of our all-purpose, rock-solid
   generators. It has excellent (sub-ns) speed, a state (256 bits) that is
   large enough for any parallel application, and it passes all tests we
   are aware of.

   For generating just floating-point numbers, xoshiro256+ is even faster.

   The state must be seeded so that it is not everywhere zero. If you have
   a 64-bit seed, we suggest to seed a splitmix64 generator and use its
   output to fill s.
*/

uint64_t random_next_u64(struct random_state *state)
{
#define rotl(x, k) ((uint64_t)(x) << (k)) | ((uint64_t)(x) >> (64 - (k)))
	const uint64_t result = rotl(state->s[1] * 5, 7) * 9;

	const uint64_t t = state->s[1] << 17;

	state->s[2] ^= state->s[0];
	state->s[3] ^= state->s[1];
	state->s[1] ^= state->s[2];
	state->s[0] ^= state->s[3];

	state->s[2] ^= t;

	state->s[3] = rotl(state->s[3], 45);

	return result;
#undef rotl
}

uint32_t random_next_u32(struct random_state *state)
{
	return (uint32_t)random_next_u64(state);
}

double random_next_uniform_double(struct random_state *state)
{
	return (random_next_u64(state) >> 11) * 0x1.0p-53;
}

float random_next_uniform_float(struct random_state *state)
{
	return (random_next_u32(state) >> 8) * 0x1.0p-24f;
}

bool random_next_bool(struct random_state *state)
{
	return random_next_u32(state) & 1;
}

uint32_t _random_next_u32_in_range(struct random_state *state, uint32_t limit)
{
	// https://arxiv.org/pdf/1805.10941.pdf
	uint32_t s = limit;
	// if (unlikely(s == 0)) {
	// 	return x;
	// }
	uint32_t x = random_next_u32(state);
	uint64_t m = (uint64_t)x * s;
	uint32_t l = (uint32_t)m;
	if (l < s) {
		uint32_t t = (-s) % s;
		while (l < t) {
			x = random_next_u32(state);
			m = (uint64_t)x * s;
			l = (uint32_t)m;
		}
	}
	return m >> 32;
}

uint64_t _random_next_u64_in_range(struct random_state *state, uint64_t limit)
{
	// https://arxiv.org/pdf/1805.10941.pdf
	uint64_t s = limit;
	// if (unlikely(s == 0)) {
	// 	return x;
	// }
	uint64_t x = random_next_u64(state);
	uint128_t m = uint128_mul64(x, s);
	uint64_t l = uint128_get_low_bits(m);
	if (l < s) {
		uint64_t t = (-s) % s;
		while (l < t) {
			x = random_next_u64(state);
			m = uint128_mul64(x, s);
			l = uint128_get_low_bits(m);
		}
	}
	return uint128_get_high_bits(m);
}

#if 0
uint64_t _random_next_u64_in_range2(struct random_state *state, uint64_t limit)
{
	// if (unlikely(limit == 0)) {
	// 	return random_next_u64(state);
	// }
	uint64_t remainder = UINT64_MAX % limit;
	uint64_t x;
	do {
		x = random_next_u64(state);
	} while (x >= UINT64_MAX - remainder);
	return x % limit;
}
#endif

float random_next_float_in_range(struct random_state *state, float min, float max)
{
	assert(min <= max);
	return min + random_next_uniform_float(state) * (max - min);
}

double random_next_double_in_range(struct random_state *state, double min, double max)
{
	assert(min <= max);
	return min + random_next_uniform_double(state) * (max - min);
}


/* This is the jump function for the generator. It is equivalent
   to 2^128 calls to next(); it can be used to generate 2^128
   non-overlapping subsequences for parallel computations.
*/

void random_jump(struct random_state *state)
{
	static const uint64_t JUMP[] = { 0x180ec6d33cfd0aba, 0xd5a61266f0c9392c, 0xa9582618e03fc9aa, 0x39abdc4529b1661c };

	uint64_t s0 = 0;
	uint64_t s1 = 0;
	uint64_t s2 = 0;
	uint64_t s3 = 0;
	for (size_t i = 0; i < sizeof(JUMP) / sizeof(*JUMP); i++)
		for (size_t b = 0; b < 64; b++) {
			if (JUMP[i] & UINT64_C(1) << b) {
				s0 ^= state->s[0];
				s1 ^= state->s[1];
				s2 ^= state->s[2];
				s3 ^= state->s[3];
			}
			random_next_u64(state);
		}

	state->s[0] = s0;
	state->s[1] = s1;
	state->s[2] = s2;
	state->s[3] = s3;
}



/* This is the long-jump function for the generator. It is equivalent to
   2^192 calls to next(); it can be used to generate 2^64 starting points,
   from each of which jump() will generate 2^64 non-overlapping
   subsequences for parallel distributed computations.
*/

void random_long_jump(struct random_state *state)
{
	static const uint64_t LONG_JUMP[] = { 0x76e15d3efefdcbbf, 0xc5004e441c522fb3, 0x77710069854ee241, 0x39109bb02acbe635 };

	uint64_t s0 = 0;
	uint64_t s1 = 0;
	uint64_t s2 = 0;
	uint64_t s3 = 0;
	for (size_t i = 0; i < sizeof(LONG_JUMP) / sizeof(*LONG_JUMP); i++)
		for (size_t b = 0; b < 64; b++) {
			if (LONG_JUMP[i] & UINT64_C(1) << b) {
				s0 ^= state->s[0];
				s1 ^= state->s[1];
				s2 ^= state->s[2];
				s3 ^= state->s[3];
			}
			random_next_u64(state);
		}

	state->s[0] = s0;
	state->s[1] = s1;
	state->s[2] = s2;
	state->s[3] = s3;
}
