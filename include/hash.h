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

#include <stddef.h>
#include <stdint.h>
#include "config.h"
#include "compiler.h"

typedef union {
	uint8_t bytes[4];
	uint32_t u32;
} hash32_t;

typedef union {
	uint8_t bytes[8];
	uint64_t u64;
} hash64_t;

typedef union {
	_Alignas(uint64_t) uint8_t bytes[16];
} hash128_t;

// SipHash-2-4
// (the key k must have 16 bytes)
// (see also https://github.com/veorq/SipHash)
hash64_t siphash24_64(const void *in, size_t inlen, const void *key) _attr_pure;
hash128_t siphash24_128(const void *in, size_t inlen, const void *key) _attr_pure;

// SipHash-1-3
// (the key k must have 16 bytes)
// (see also https://github.com/veorq/SipHash)
hash64_t siphash13_64(const void *in, size_t inlen, const void *key) _attr_pure;
hash128_t siphash13_128(const void *in, size_t inlen, const void *key) _attr_pure;

// HalfSipHash-2-4
// (the key must have 8 bytes)
// (see also https://github.com/veorq/SipHash)
hash32_t halfsiphash24_32(const void *in, size_t inlen, const void *key) _attr_pure;
hash64_t halfsiphash24_64(const void *in, size_t inlen, const void *key) _attr_pure;

// HalfSipHash-1-3
// (the key must have 8 bytes)
// (see also https://github.com/veorq/SipHash)
hash32_t halfsiphash13_32(const void *in, size_t inlen, const void *key) _attr_pure;
hash64_t halfsiphash13_64(const void *in, size_t inlen, const void *key) _attr_pure;


// MurmurHash3_x86_32
// (use this on 32-bit architectures for short inputs)
// (see also https://github.com/PeterScott/murmur3)
hash32_t murmurhash3_x86_32(const void *in, size_t inlen, uint32_t seed) _attr_pure;

// MurmurHash3_x86_64/MurmurHash3_x86_128
// (use this on 32-bit architectures for long inputs or when you need more than 32 bits of output)
// (see also https://github.com/PeterScott/murmur3)
hash64_t murmurhash3_x86_64(const void *in, size_t inlen, uint32_t seed) _attr_pure;
hash128_t murmurhash3_x86_128(const void *in, size_t inlen, uint32_t seed) _attr_pure;

// MurmurHash3_x64_64/MurmurHash3_x64_128
// (use this on 64-bit architectures)
// (see also https://github.com/PeterScott/murmur3)
hash64_t murmurhash3_x64_64(const void *in, size_t inlen, uint32_t seed) _attr_pure;
hash128_t murmurhash3_x64_128(const void *in, size_t inlen, uint32_t seed) _attr_pure;

hash32_t hash_int32(uint32_t val) _attr_const;
hash64_t hash_int64(uint64_t val) _attr_const;

hash32_t fibonacci_hash32(uint32_t val, unsigned int bits) _attr_const;
hash64_t fibonacci_hash64(uint64_t val, unsigned int bits) _attr_const;

hash32_t hash_combine_int32(uint32_t seed, uint32_t val) _attr_const;
hash64_t hash_combine_int64(uint64_t seed, uint64_t val) _attr_const;
