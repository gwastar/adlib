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
#include <stdint.h>
#include <string.h>
#include "hash.h"
#include "macros.h"

// TODO add FNV-1 and FNV-1a

/*
  SipHash reference C implementation
  Copyright (c) 2012-2021 Jean-Philippe Aumasson
  <jeanphilippe.aumasson@gmail.com>
  Copyright (c) 2012-2014 Daniel J. Bernstein <djb@cr.yp.to>
  To the extent possible under law, the author(s) have dedicated all copyright
  and related and neighboring rights to this software to the public domain
  worldwide. This software is distributed without any warranty.
  You should have received a copy of the CC0 Public Domain Dedication along
  with
  this software. If not, see
  <http://creativecommons.org/publicdomain/zero/1.0/>.
*/

#define __HASH_ROTL32(x, b) (uint32_t)(((x) << (b)) | ((x) >> (32 - (b))))
#define __HASH_ROTL64(x, b) (uint64_t)(((x) << (b)) | ((x) >> (64 - (b))))

#define __HASH_U32TO8_LE(p, v)			\
	(p)[0] = (uint8_t)((v));		\
	(p)[1] = (uint8_t)((v) >> 8);		\
	(p)[2] = (uint8_t)((v) >> 16);		\
	(p)[3] = (uint8_t)((v) >> 24);

#define __HASH_U64TO8_LE(p, v)					\
	__HASH_U32TO8_LE((p), (uint32_t)((v)));			\
	__HASH_U32TO8_LE((p) + 4, (uint32_t)((v) >> 32));

#define __HASH_U8TO32_LE(p)						\
	(((uint32_t)((p)[0])) | ((uint32_t)((p)[1]) << 8) |		\
	 ((uint32_t)((p)[2]) << 16) | ((uint32_t)((p)[3]) << 24))

#define __HASH_U8TO64_LE(p)						\
	(((uint64_t)((p)[0])) | ((uint64_t)((p)[1]) << 8) |		\
	 ((uint64_t)((p)[2]) << 16) | ((uint64_t)((p)[3]) << 24) |	\
	 ((uint64_t)((p)[4]) << 32) | ((uint64_t)((p)[5]) << 40) |	\
	 ((uint64_t)((p)[6]) << 48) | ((uint64_t)((p)[7]) << 56))

#define __SIPROUND				\
	do {					\
		v0 += v1;			\
		v1 = __HASH_ROTL64(v1, 13);	\
		v1 ^= v0;			\
		v0 = __HASH_ROTL64(v0, 32);	\
		v2 += v3;			\
		v3 = __HASH_ROTL64(v3, 16);	\
		v3 ^= v2;			\
		v0 += v3;			\
		v3 = __HASH_ROTL64(v3, 21);	\
		v3 ^= v0;			\
		v2 += v1;			\
		v1 = __HASH_ROTL64(v1, 17);	\
		v1 ^= v2;			\
		v2 = __HASH_ROTL64(v2, 32);	\
	} while (0)

static _attr_always_inline void _siphash(const void *in, const size_t inlen, const void *k,
					 uint8_t *out, const size_t outlen,
					 const int cROUNDS, const int dROUNDS)
{
	const unsigned char *ni = (const unsigned char *)in;
	const unsigned char *kk = (const unsigned char *)k;

	assert((outlen == 8) || (outlen == 16));
	uint64_t v0 = UINT64_C(0x736f6d6570736575);
	uint64_t v1 = UINT64_C(0x646f72616e646f6d);
	uint64_t v2 = UINT64_C(0x6c7967656e657261);
	uint64_t v3 = UINT64_C(0x7465646279746573);
	uint64_t k0 = __HASH_U8TO64_LE(kk);
	uint64_t k1 = __HASH_U8TO64_LE(kk + 8);
	uint64_t m;
	int i;
	const unsigned char *end = ni + inlen - (inlen % sizeof(uint64_t));
	const int left = inlen & 7;
	uint64_t b = ((uint64_t)inlen) << 56;
	v3 ^= k1;
	v2 ^= k0;
	v1 ^= k1;
	v0 ^= k0;

	if (outlen == 16) {
		v1 ^= 0xee;
	}

	for (; ni != end; ni += 8) {
		m = __HASH_U8TO64_LE(ni);
		v3 ^= m;

		for (i = 0; i < cROUNDS; ++i) {
			__SIPROUND;
		}

		v0 ^= m;
	}

	switch (left) {
	case 7:
		b |= ((uint64_t)ni[6]) << 48; _attr_fallthrough;
	case 6:
		b |= ((uint64_t)ni[5]) << 40; _attr_fallthrough;
	case 5:
		b |= ((uint64_t)ni[4]) << 32; _attr_fallthrough;
	case 4:
		b |= ((uint64_t)ni[3]) << 24; _attr_fallthrough;
	case 3:
		b |= ((uint64_t)ni[2]) << 16; _attr_fallthrough;
	case 2:
		b |= ((uint64_t)ni[1]) << 8; _attr_fallthrough;
	case 1:
		b |= ((uint64_t)ni[0]); _attr_fallthrough;
	case 0:
		break;
	}

	v3 ^= b;

	for (i = 0; i < cROUNDS; ++i) {
		__SIPROUND;
	}

	v0 ^= b;

	if (outlen == 16) {
		v2 ^= 0xee;
	} else {
		v2 ^= 0xff;
	}

	for (i = 0; i < dROUNDS; ++i) {
		__SIPROUND;
	}

	b = v0 ^ v1 ^ v2 ^ v3;
	__HASH_U64TO8_LE(out, b);

	if (outlen == 8) {
		return;
	}

	v1 ^= 0xdd;

	for (i = 0; i < dROUNDS; ++i) {
		__SIPROUND;
	}

	b = v0 ^ v1 ^ v2 ^ v3;
	__HASH_U64TO8_LE(out + 8, b);
}

hash64_t siphash24_64(const void *in, size_t inlen, const void *key)
{
	hash64_t out;
	_siphash(in, inlen, key, out.bytes, sizeof(out), 2, 4);
	return out;
}

hash128_t siphash24_128(const void *in, size_t inlen, const void *key)
{
	hash128_t out;
	_siphash(in, inlen, key, out.bytes, sizeof(out), 2, 4);
	return out;
}

hash64_t siphash13_64(const void *in, size_t inlen, const void *key)
{
	hash64_t out;
	_siphash(in, inlen, key, out.bytes, sizeof(out), 1, 3);
	return out;
}

hash128_t siphash13_128(const void *in, size_t inlen, const void *key)
{
	hash128_t out;
	_siphash(in, inlen, key, out.bytes, sizeof(out), 1, 3);
	return out;
}

#define __HSIPROUND				\
	do {					\
		v0 += v1;			\
		v1 = __HASH_ROTL32(v1, 5);	\
		v1 ^= v0;			\
		v0 = __HASH_ROTL32(v0, 16);	\
		v2 += v3;			\
		v3 = __HASH_ROTL32(v3, 8);	\
		v3 ^= v2;			\
		v0 += v3;			\
		v3 = __HASH_ROTL32(v3, 7);	\
		v3 ^= v0;			\
		v2 += v1;			\
		v1 = __HASH_ROTL32(v1, 13);	\
		v1 ^= v2;			\
		v2 = __HASH_ROTL32(v2, 16);	\
	} while (0)

static _attr_always_inline void _halfsiphash(const void *in, const size_t inlen, const void *k,
					     uint8_t *out, const size_t outlen,
					     const int cROUNDS, const int dROUNDS)
{
	const unsigned char *ni = (const unsigned char *)in;
	const unsigned char *kk = (const unsigned char *)k;

	assert((outlen == 4) || (outlen == 8));
	uint32_t v0 = 0;
	uint32_t v1 = 0;
	uint32_t v2 = UINT32_C(0x6c796765);
	uint32_t v3 = UINT32_C(0x74656462);
	uint32_t k0 = __HASH_U8TO32_LE(kk);
	uint32_t k1 = __HASH_U8TO32_LE(kk + 4);
	uint32_t m;
	int i;
	const unsigned char *end = ni + inlen - (inlen % sizeof(uint32_t));
	const int left = inlen & 3;
	uint32_t b = ((uint32_t)inlen) << 24;
	v3 ^= k1;
	v2 ^= k0;
	v1 ^= k1;
	v0 ^= k0;

	if (outlen == 8) {
		v1 ^= 0xee;
	}

	for (; ni != end; ni += 4) {
		m = __HASH_U8TO32_LE(ni);
		v3 ^= m;

		for (i = 0; i < cROUNDS; ++i) {
			__HSIPROUND;
		}

		v0 ^= m;
	}

	switch (left) {
	case 3:
		b |= ((uint32_t)ni[2]) << 16; _attr_fallthrough;
	case 2:
		b |= ((uint32_t)ni[1]) << 8; _attr_fallthrough;
	case 1:
		b |= ((uint32_t)ni[0]); _attr_fallthrough;
	case 0:
		break;
	}

	v3 ^= b;

	for (i = 0; i < cROUNDS; ++i) {
		__HSIPROUND;
	}

	v0 ^= b;

	if (outlen == 8) {
		v2 ^= 0xee;
	} else {
		v2 ^= 0xff;
	}

	for (i = 0; i < dROUNDS; ++i) {
		__HSIPROUND;
	}

	b = v1 ^ v3;
	__HASH_U32TO8_LE(out, b);

	if (outlen == 4) {
		return;
	}

	v1 ^= 0xdd;

	for (i = 0; i < dROUNDS; ++i) {
		__HSIPROUND;
	}

	b = v1 ^ v3;
	__HASH_U32TO8_LE(out + 4, b);
}

hash32_t halfsiphash24_32(const void *in, size_t inlen, const void *key)
{
	hash32_t out;
	_halfsiphash(in, inlen, key, out.bytes, sizeof(out), 2, 4);
	return out;
}

hash64_t halfsiphash24_64(const void *in, size_t inlen, const void *key)
{
	hash64_t out;
	_halfsiphash(in, inlen, key, out.bytes, sizeof(out), 2, 4);
	return out;
}

hash32_t halfsiphash13_32(const void *in, size_t inlen, const void *key)
{
	hash32_t out;
	_halfsiphash(in, inlen, key, out.bytes, sizeof(out), 1, 3);
	return out;
}

hash64_t halfsiphash13_64(const void *in, size_t inlen, const void *key)
{
	hash64_t out;
	_halfsiphash(in, inlen, key, out.bytes, sizeof(out), 1, 3);
	return out;
}

// MurmurHash3 was written by Austin Appleby, and is placed in the public
// domain. The author hereby disclaims copyright to this source code.

static _attr_always_inline uint32_t _murmur_getblock32(const uint32_t *p, size_t i)
{
	const uint8_t *q = (const uint8_t *)(p + i);
	return __HASH_U8TO32_LE(q);
}

static _attr_always_inline uint64_t _murmur_getblock64(const uint64_t *p, size_t i)
{
	const uint8_t *q = (const uint8_t *)(p + i);
	return __HASH_U8TO64_LE(q);
}

static _attr_always_inline uint32_t _murmur_fmix32(uint32_t h)
{
	h ^= h >> 16;
	h *= UINT32_C(0x85ebca6b);
	h ^= h >> 13;
	h *= UINT32_C(0xc2b2ae35);
	h ^= h >> 16;
	return h;
}

static _attr_always_inline uint64_t _murmur_fmix64(uint64_t k)
{
	k ^= k >> 33;
	k *= UINT64_C(0xff51afd7ed558ccd);
	k ^= k >> 33;
	k *= UINT64_C(0xc4ceb9fe1a85ec53);
	k ^= k >> 33;
	return k;
}

static _attr_always_inline void _murmurhash3_x86_32(const void *in, size_t len, uint32_t seed, void *out)
{
	const uint8_t *data = (const uint8_t *)in;
	const size_t nblocks = len / 4;

	uint32_t h1 = seed;

	const uint32_t c1 = UINT32_C(0xcc9e2d51);
	const uint32_t c2 = UINT32_C(0x1b873593);

	const uint32_t *blocks = (const uint32_t *)data;

	for (size_t i = 0; i < nblocks; i++) {
		uint32_t k1 = _murmur_getblock32(blocks, i);

		k1 *= c1;
		k1 = __HASH_ROTL32(k1, 15);
		k1 *= c2;

		h1 ^= k1;
		h1 = __HASH_ROTL32(h1, 13);
		h1 = h1 * 5 + UINT32_C(0xe6546b64);
	}

	const uint8_t *tail = (const uint8_t *)(data + nblocks * 4);

	uint32_t k1 = 0;

	switch (len & 3)
	{
	case 3: k1 ^= tail[2] << 16; _attr_fallthrough;
	case 2: k1 ^= tail[1] << 8; _attr_fallthrough;
	case 1: k1 ^= tail[0];
		k1 *= c1; k1 = __HASH_ROTL32(k1, 15); k1 *= c2; h1 ^= k1;
	};

	h1 ^= len;

	h1 = _murmur_fmix32(h1);

	uint8_t *o = out;
	__HASH_U32TO8_LE(o, h1);
}

hash32_t murmurhash3_x86_32(const void *in, size_t inlen, uint32_t seed)
{
	hash32_t out;
	_murmurhash3_x86_32(in, inlen, seed, out.bytes);
	return out;
}

static _attr_always_inline void _murmurhash3_x86_128(const void *in, size_t len, uint32_t seed, void *out)
{
	const uint8_t *data = (const uint8_t *)in;
	const size_t nblocks = len / 16;

	uint32_t h1 = seed;
	uint32_t h2 = seed;
	uint32_t h3 = seed;
	uint32_t h4 = seed;

	const uint32_t c1 = UINT32_C(0x239b961b);
	const uint32_t c2 = UINT32_C(0xab0e9789);
	const uint32_t c3 = UINT32_C(0x38b34ae5);
	const uint32_t c4 = UINT32_C(0xa1e38b93);

	const uint32_t *blocks = (const uint32_t *)data;

	for (size_t i = 0; i < nblocks; i++) {
		uint32_t k1 = _murmur_getblock32(blocks, i * 4 + 0);
		uint32_t k2 = _murmur_getblock32(blocks, i * 4 + 1);
		uint32_t k3 = _murmur_getblock32(blocks, i * 4 + 2);
		uint32_t k4 = _murmur_getblock32(blocks, i * 4 + 3);

		k1 *= c1; k1 = __HASH_ROTL32(k1, 15); k1 *= c2; h1 ^= k1;

		h1 = __HASH_ROTL32(h1, 19); h1 += h2; h1 = h1 * 5 + UINT32_C(0x561ccd1b);

		k2 *= c2; k2 = __HASH_ROTL32(k2, 16); k2 *= c3; h2 ^= k2;

		h2 = __HASH_ROTL32(h2, 17); h2 += h3; h2 = h2 * 5 + UINT32_C(0x0bcaa747);

		k3 *= c3; k3 = __HASH_ROTL32(k3, 17); k3 *= c4; h3 ^= k3;

		h3 = __HASH_ROTL32(h3, 15); h3 += h4; h3 = h3 * 5 + UINT32_C(0x96cd1c35);

		k4 *= c4; k4 = __HASH_ROTL32(k4, 18); k4 *= c1; h4 ^= k4;

		h4 = __HASH_ROTL32(h4, 13); h4 += h1; h4 = h4 * 5 + UINT32_C(0x32ac3b17);
	}

	const uint8_t *tail = (const uint8_t *)(data + nblocks * 16);

	uint32_t k1 = 0;
	uint32_t k2 = 0;
	uint32_t k3 = 0;
	uint32_t k4 = 0;

	switch (len & 15)
	{
	case 15: k4 ^= tail[14] << 16; _attr_fallthrough;
	case 14: k4 ^= tail[13] << 8; _attr_fallthrough;
	case 13: k4 ^= tail[12] << 0;
		k4 *= c4; k4 = __HASH_ROTL32(k4, 18); k4 *= c1; h4 ^= k4;
		_attr_fallthrough;
	case 12: k3 ^= (uint32_t)tail[11] << 24; _attr_fallthrough;
	case 11: k3 ^= tail[10] << 16; _attr_fallthrough;
	case 10: k3 ^= tail[ 9] << 8; _attr_fallthrough;
	case  9: k3 ^= tail[ 8] << 0;
		k3 *= c3; k3 = __HASH_ROTL32(k3, 17); k3 *= c4; h3 ^= k3;
		_attr_fallthrough;
	case  8: k2 ^= (uint32_t)tail[ 7] << 24; _attr_fallthrough;
	case  7: k2 ^= tail[ 6] << 16; _attr_fallthrough;
	case  6: k2 ^= tail[ 5] << 8; _attr_fallthrough;
	case  5: k2 ^= tail[ 4] << 0;
		k2 *= c2; k2 = __HASH_ROTL32(k2, 16); k2 *= c3; h2 ^= k2;
		_attr_fallthrough;
	case  4: k1 ^= (uint32_t)tail[ 3] << 24; _attr_fallthrough;
	case  3: k1 ^= tail[ 2] << 16; _attr_fallthrough;
	case  2: k1 ^= tail[ 1] << 8; _attr_fallthrough;
	case  1: k1 ^= tail[ 0] << 0;
		k1 *= c1; k1 = __HASH_ROTL32(k1, 15); k1 *= c2; h1 ^= k1;
	};

	h1 ^= len; h2 ^= len; h3 ^= len; h4 ^= len;

	h1 += h2; h1 += h3; h1 += h4;
	h2 += h1; h3 += h1; h4 += h1;

	h1 = _murmur_fmix32(h1);
	h2 = _murmur_fmix32(h2);
	h3 = _murmur_fmix32(h3);
	h4 = _murmur_fmix32(h4);

	h1 += h2; h1 += h3; h1 += h4;
	h2 += h1; h3 += h1; h4 += h1;

	uint8_t *o = out;
	__HASH_U32TO8_LE(o, h1);
	o += 4;
	__HASH_U32TO8_LE(o, h2);
	o += 4;
	__HASH_U32TO8_LE(o, h3);
	o += 4;
	__HASH_U32TO8_LE(o, h4);
}

hash128_t murmurhash3_x86_128(const void *in, size_t inlen, uint32_t seed)
{
	hash128_t out;
	_murmurhash3_x86_128(in, inlen, seed, out.bytes);
	return out;
}

hash64_t murmurhash3_x86_64(const void *in, size_t inlen, uint32_t seed)
{
	hash128_t out128 = murmurhash3_x86_128(in, inlen, seed);
	hash64_t out64;
	memcpy(out64.bytes, out128.bytes, sizeof(out64));
	return out64;
}

static _attr_always_inline void _murmurhash3_x64_128(const void *in, size_t len, uint32_t seed, void *out)
{
	const uint8_t *data = (const uint8_t *)in;
	const size_t nblocks = len / 16;

	uint64_t h1 = seed;
	uint64_t h2 = seed;

	const uint64_t c1 = UINT64_C(0x87c37b91114253d5);
	const uint64_t c2 = UINT64_C(0x4cf5ad432745937f);

	const uint64_t *blocks = (const uint64_t *)data;

	for (size_t i = 0; i < nblocks; i++) {
		uint64_t k1 = _murmur_getblock64(blocks, i * 2 + 0);
		uint64_t k2 = _murmur_getblock64(blocks, i * 2 + 1);

		k1 *= c1; k1 = __HASH_ROTL64(k1, 31); k1 *= c2; h1 ^= k1;

		h1 = __HASH_ROTL64(h1, 27); h1 += h2; h1 = h1 * 5 + UINT32_C(0x52dce729);

		k2 *= c2; k2 = __HASH_ROTL64(k2, 33); k2 *= c1; h2 ^= k2;

		h2 = __HASH_ROTL64(h2, 31); h2 += h1; h2 = h2 * 5 + UINT32_C(0x38495ab5);
	}

	const uint8_t *tail = (const uint8_t *)(data + nblocks * 16);

	uint64_t k1 = 0;
	uint64_t k2 = 0;

	switch (len & 15)
	{
	case 15: k2 ^= ((uint64_t)tail[14]) << 48; _attr_fallthrough;
	case 14: k2 ^= ((uint64_t)tail[13]) << 40; _attr_fallthrough;
	case 13: k2 ^= ((uint64_t)tail[12]) << 32; _attr_fallthrough;
	case 12: k2 ^= ((uint64_t)tail[11]) << 24; _attr_fallthrough;
	case 11: k2 ^= ((uint64_t)tail[10]) << 16; _attr_fallthrough;
	case 10: k2 ^= ((uint64_t)tail[ 9]) << 8; _attr_fallthrough;
	case  9: k2 ^= ((uint64_t)tail[ 8]) << 0;
		k2 *= c2; k2 = __HASH_ROTL64(k2, 33); k2 *= c1; h2 ^= k2;
		_attr_fallthrough;
	case  8: k1 ^= ((uint64_t)tail[ 7]) << 56; _attr_fallthrough;
	case  7: k1 ^= ((uint64_t)tail[ 6]) << 48; _attr_fallthrough;
	case  6: k1 ^= ((uint64_t)tail[ 5]) << 40; _attr_fallthrough;
	case  5: k1 ^= ((uint64_t)tail[ 4]) << 32; _attr_fallthrough;
	case  4: k1 ^= ((uint64_t)tail[ 3]) << 24; _attr_fallthrough;
	case  3: k1 ^= ((uint64_t)tail[ 2]) << 16; _attr_fallthrough;
	case  2: k1 ^= ((uint64_t)tail[ 1]) << 8; _attr_fallthrough;
	case  1: k1 ^= ((uint64_t)tail[ 0]) << 0;
		k1 *= c1; k1 = __HASH_ROTL64(k1, 31); k1 *= c2; h1 ^= k1;
	};

	h1 ^= len; h2 ^= len;

	h1 += h2;
	h2 += h1;

	h1 = _murmur_fmix64(h1);
	h2 = _murmur_fmix64(h2);

	h1 += h2;
	h2 += h1;

	uint8_t *o = out;
	__HASH_U64TO8_LE(o, h1);
	o += 8;
	__HASH_U64TO8_LE(o, h2);
}

hash128_t murmurhash3_x64_128(const void *in, size_t inlen, uint32_t seed)
{
	hash128_t out;
	_murmurhash3_x64_128(in, inlen, seed, out.bytes);
	return out;
}

hash64_t murmurhash3_x64_64(const void *in, size_t inlen, uint32_t seed)
{
	hash128_t out128 = murmurhash3_x64_128(in, inlen, seed);
	hash64_t out64;
	memcpy(out64.bytes, out128.bytes, sizeof(out64));
	return out64;
}

hash32_t hash_int32(uint32_t val)
{
	hash32_t out;
	val = _murmur_fmix32(val);
	__HASH_U32TO8_LE(out.bytes, val);
	return out;
}

hash64_t hash_int64(uint64_t val)
{
	hash64_t out;
	val = _murmur_fmix64(val);
	__HASH_U64TO8_LE(out.bytes, val);
	return out;
}

hash32_t fibonacci_hash32(uint32_t val, unsigned int bits)
{
	hash32_t out;
	// val *= UINT32_C(2654435769);
	val *= UINT32_C(1640531527);
	out.u32 = val >> (32 - bits);
	return out;
}

hash64_t fibonacci_hash64(uint64_t val, unsigned int bits)
{
	hash64_t out;
	// val *= UINT64_C(11400714819323198485);
	val *= UINT64_C(7046029254386353131);
	out.u64 = val >> (64 - bits);
	return out;
}

hash32_t hash_combine_int32(uint32_t seed, uint32_t val)
{
	return hash_int32(seed + UINT32_C(0xe6546b64) + UINT32_C(1640531527) * val);

	// uint32_t buffer[2] = {seed, val};
	// return murmurhash3_x86_32(&buffer, sizeof(buffer), 0);
}

hash64_t hash_combine_int64(uint64_t seed, uint64_t val)
{
	return hash_int64(seed + UINT32_C(0xe6546b64) + UINT64_C(7046029254386353131) * val);

	// uint64_t buffer[2] = {seed, val};
	// return murmurhash3_x64_64(&buffer, sizeof(buffer), 0);
}

#undef __HASH_ROTL32
#undef __HASH_ROTL64
#undef __HASH_U32TO8_LE
#undef __HASH_U64TO8_LE
#undef __HASH_U8TO32_LE
#undef __HASH_U8TO64_LE
#undef __SIPROUND
#undef __HSIPROUND
