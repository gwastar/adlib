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

#pragma once

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "config.h"
#include "compiler.h"
#include "macros.h"

struct random_state {
	uint64_t s[4];
};

void random_state_init(struct random_state *state, uint64_t seed);
uint64_t random_next_u64(struct random_state *state);
uint32_t random_next_u32(struct random_state *state);
double random_next_uniform_double(struct random_state *state);
float random_next_uniform_float(struct random_state *state);
bool random_next_bool(struct random_state *state);
uint32_t _random_next_u32_in_range(struct random_state *state, uint32_t limit);
uint64_t _random_next_u64_in_range(struct random_state *state, uint64_t limit);
// uint64_t _random_next_u64_in_range2(struct random_state *state, uint64_t limit);
float random_next_float_in_range(struct random_state *state, float min, float max);
double random_next_double_in_range(struct random_state *state, double min, double max);
void random_jump(struct random_state *state);
void random_long_jump(struct random_state *state);

#define __RANDOM_SPLITMIX64_1(z) (((z) ^ ((z) >> 30)) * 0xbf58476d1ce4e5b9)
#define __RANDOM_SPLITMIX64_2(z) (((z) ^ ((z) >> 27)) * 0x94d049bb133111eb)
#define __RANDOM_SPLITMIX64_3(z) ((z) ^ ((z) >> 31))
#define __RANDOM_SPLITMIX64(x, c)					\
	(__RANDOM_SPLITMIX64_3(__RANDOM_SPLITMIX64_2(__RANDOM_SPLITMIX64_1((x) + (c) * 0x9e3779b97f4a7c15))))

// this should only be used when a constant expression initializer is necessary (e.g. for static variables)
// since it expands to a lot of code
#define RANDOM_STATE_INITIALIZER(x) {					\
		.s[0] = __RANDOM_SPLITMIX64((uint64_t)(x), 1),		\
			.s[1] = __RANDOM_SPLITMIX64((uint64_t)(x), 2),	\
			.s[2] = __RANDOM_SPLITMIX64((uint64_t)(x), 3),	\
			.s[3] = __RANDOM_SPLITMIX64((uint64_t)(x), 4)}

static _attr_always_inline uint64_t random_next_u64_in_range(struct random_state *state, uint64_t min,
							     uint64_t max)
{
	assert(min <= max);
	uint64_t n = max - min + 1;
#if 0
	// TODO why does this not work properly
	// example: for min=9 max=9+1023 this produced a distribution with mean=718.502 stddev=270.958
	// when it should be mean=520.5 stddev=295.603
	if ((n & (n - 1)) == 0) {
		uint64_t r = random_next_u64(state);
		return min + (r & (n - 1));
	}
#else
	if (n == 0) {
		return random_next_u64(state);
	}
#endif
	return min + _random_next_u64_in_range(state, n);
}

#if 0
// for processors that don't have fast 64x64->128 multiplication

// TODO why does this not work properly
// example: for min=9 max=9+1023 this produced a distribution with mean=718.502 stddev=270.958
// when it should be mean=520.5 stddev=295.603
// (same problem as above?)
static _attr_always_inline uint64_t random_next_u64_in_range2(struct random_state *state, uint64_t min,
							      uint64_t max)
{
	assert(min <= max);
	uint64_t n = max - min + 1;
	if (n == 0) {
		return random_next_u64(state);
	}
	return min + _random_next_u64_in_range2(state, n);
}
#endif

static _attr_always_inline uint32_t random_next_u32_in_range(struct random_state *state, uint32_t min,
							     uint32_t max)
{
	assert(min <= max);
	uint32_t n = max - min + 1;
	if (n == 0) {
		return random_next_u32(state);
	}
	return min + _random_next_u32_in_range(state, n);
}
