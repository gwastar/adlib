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

#include "utils.h"
#include <limits.h>
#include <stdint.h>

#ifndef HAVE_BUILTIN_CLZ
unsigned int _clz(unsigned int x)
{
	unsigned int n;
	for (n = 0; x; n++, x >>= 1);
	return 8 * sizeof(x) - n;
}

unsigned int _clzl(unsigned long x)
{
	unsigned int n;
	for (n = 0; x; n++, x >>= 1);
	return 8 * sizeof(x) - n;
}

unsigned int _clzll(unsigned long long x)
{
	unsigned int n;
	for (n = 0; x; n++, x >>= 1);
	return 8 * sizeof(x) - n;
}
#endif

#ifndef HAVE_BUILTIN_CTZ
unsigned int _ctz(unsigned int x)
{
	x = ~x & (x - 1);
	unsigned int n;
	for (n = 0; x; n++, x >>= 1);
	return n;
}

unsigned int _ctzl(unsigned long x)
{
	x = ~x & (x - 1);
	unsigned int n;
	for (n = 0; x; n++, x >>= 1);
	return n;
}

unsigned int _ctzll(unsigned long long x)
{
	x = ~x & (x - 1);
	unsigned int n;
	for (n = 0; x; n++, x >>= 1);
	return n;
}
#endif

#ifndef HAVE_BUILTIN_FFS
unsigned int _ffs(unsigned int x)
{
	if (x == 0) {
		return 0;
	}
	x = ~x & (x - 1);
	unsigned int n;
	for (n = 1; x; n++, x >>= 1);
	return n;
}

unsigned int _ffsl(unsigned long x)
{
	if (x == 0) {
		return 0;
	}
	x = ~x & (x - 1);
	unsigned int n;
	for (n = 1; x; n++, x >>= 1);
	return n;
}

unsigned int _ffsll(unsigned long long x)
{
	if (x == 0) {
		return 0;
	}
	x = ~x & (x - 1);
	unsigned int n;
	for (n = 1; x; n++, x >>= 1);
	return n;
}
#endif

#ifndef HAVE_BUILTIN_POPCOUNT
unsigned int _popcount(unsigned int x)
{
	unsigned int n;
	for (n = 0; x; n++, x = x & (x - 1));
	return n;
}

unsigned int _popcountl(unsigned long x)
{
	unsigned int n;
	for (n = 0; x; n++, x = x & (x - 1));
	return n;
}

unsigned int _popcountll(unsigned long long x)
{
	unsigned int n;
	for (n = 0; x; n++, x = x & (x - 1));
	return n;
}
#endif

static unsigned int _ilog10_32(uint32_t x)
{
	// https://lemire.me/blog/2021/06/03/computing-the-number-of-digits-of-an-integer-even-faster/
	// the constants are different because the original algorithm returns ceil(log10(x)) but we want
	// floor(log10(x)) so that ilog10 matches ilog2
	// the constants are therefore the original constants minus (1 << 32)
	static const uint64_t table[] = {
		UINT64_C(0),
		UINT64_C(4294967286),
		UINT64_C(4294967286),
		UINT64_C(4294967286),
		UINT64_C(8589934492),
		UINT64_C(8589934492),
		UINT64_C(8589934492),
		UINT64_C(12884900888),
		UINT64_C(12884900888),
		UINT64_C(12884900888),
		UINT64_C(17179859184),
		UINT64_C(17179859184),
		UINT64_C(17179859184),
		UINT64_C(17179859184),
		UINT64_C(21474736480),
		UINT64_C(21474736480),
		UINT64_C(21474736480),
		UINT64_C(25768803776),
		UINT64_C(25768803776),
		UINT64_C(25768803776),
		UINT64_C(30054771072),
		UINT64_C(30054771072),
		UINT64_C(30054771072),
		UINT64_C(30054771072),
		UINT64_C(34259738368),
		UINT64_C(34259738368),
		UINT64_C(34259738368),
		UINT64_C(37654705664),
		UINT64_C(37654705664),
		UINT64_C(37654705664),
		UINT64_C(38654705664),
		UINT64_C(38654705664),
	};
	return (x + table[ilog2(x)]) >> 32;
}

static unsigned int _ilog10_64(uint64_t x)
{
	// Hacker's Delight
	static const uint64_t table[] = {
		UINT64_C(9),
		UINT64_C(99),
		UINT64_C(999),
		UINT64_C(9999),
		UINT64_C(99999),
		UINT64_C(999999),
		UINT64_C(9999999),
		UINT64_C(99999999),
		UINT64_C(999999999),
		UINT64_C(9999999999),
		UINT64_C(99999999999),
		UINT64_C(999999999999),
		UINT64_C(9999999999999),
		UINT64_C(99999999999999),
		UINT64_C(999999999999999),
		UINT64_C(9999999999999999),
		UINT64_C(99999999999999999),
		UINT64_C(999999999999999999),
		UINT64_C(9999999999999999999),
	};
	unsigned int y = (19 * ilog2(x)) >> 6;
	y += x > table[y];
	return y;
}

unsigned int _ilog10(unsigned int x)
{
#if UINT_MAX <= UINT32_MAX
	return _ilog10_32(x);
#else
	return _ilog10_64(x);
#endif
}

unsigned int _ilog10l(unsigned long x)
{
#if ULONG_MAX <= UINT32_MAX
	return _ilog10_32(x);
#else
	return _ilog10_64(x);
#endif
}

unsigned int _ilog10ll(unsigned long long x)
{
#if ULLONG_MAX <= UINT32_MAX
	return _ilog10_32(x);
#else
	return _ilog10_64(x);
#endif
}
