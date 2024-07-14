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
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "charconv.h"
#include "compiler.h"
#include "utils.h"

#define CACHELINE_SIZE 64 // TODO

static _Alignas(CACHELINE_SIZE) const char __to_chars_lut_base2[64] = {
	'0', '0', '0', '0',
	'0', '0', '0', '1',
	'0', '0', '1', '0',
	'0', '0', '1', '1',
	'0', '1', '0', '0',
	'0', '1', '0', '1',
	'0', '1', '1', '0',
	'0', '1', '1', '1',
	'1', '0', '0', '0',
	'1', '0', '0', '1',
	'1', '0', '1', '0',
	'1', '0', '1', '1',
	'1', '1', '0', '0',
	'1', '1', '0', '1',
	'1', '1', '1', '0',
	'1', '1', '1', '1',
};

static _Alignas(CACHELINE_SIZE) const char __to_chars_lut_base8[128] = {
	'0', '0', '0', '1', '0', '2', '0', '3', '0', '4', '0', '5', '0', '6', '0', '7',
	'1', '0', '1', '1', '1', '2', '1', '3', '1', '4', '1', '5', '1', '6', '1', '7',
	'2', '0', '2', '1', '2', '2', '2', '3', '2', '4', '2', '5', '2', '6', '2', '7',
	'3', '0', '3', '1', '3', '2', '3', '3', '3', '4', '3', '5', '3', '6', '3', '7',
	'4', '0', '4', '1', '4', '2', '4', '3', '4', '4', '4', '5', '4', '6', '4', '7',
	'5', '0', '5', '1', '5', '2', '5', '3', '5', '4', '5', '5', '5', '6', '5', '7',
	'6', '0', '6', '1', '6', '2', '6', '3', '6', '4', '6', '5', '6', '6', '6', '7',
	'7', '0', '7', '1', '7', '2', '7', '3', '7', '4', '7', '5', '7', '6', '7', '7',
};

static _Alignas(CACHELINE_SIZE) const char __to_chars_lut_base10[200] = {
	'0', '0', '0', '1', '0', '2', '0', '3', '0', '4', '0', '5', '0', '6', '0', '7', '0', '8', '0', '9',
	'1', '0', '1', '1', '1', '2', '1', '3', '1', '4', '1', '5', '1', '6', '1', '7', '1', '8', '1', '9',
	'2', '0', '2', '1', '2', '2', '2', '3', '2', '4', '2', '5', '2', '6', '2', '7', '2', '8', '2', '9',
	'3', '0', '3', '1', '3', '2', '3', '3', '3', '4', '3', '5', '3', '6', '3', '7', '3', '8', '3', '9',
	'4', '0', '4', '1', '4', '2', '4', '3', '4', '4', '4', '5', '4', '6', '4', '7', '4', '8', '4', '9',
	'5', '0', '5', '1', '5', '2', '5', '3', '5', '4', '5', '5', '5', '6', '5', '7', '5', '8', '5', '9',
	'6', '0', '6', '1', '6', '2', '6', '3', '6', '4', '6', '5', '6', '6', '6', '7', '6', '8', '6', '9',
	'7', '0', '7', '1', '7', '2', '7', '3', '7', '4', '7', '5', '7', '6', '7', '7', '7', '8', '7', '9',
	'8', '0', '8', '1', '8', '2', '8', '3', '8', '4', '8', '5', '8', '6', '8', '7', '8', '8', '8', '9',
	'9', '0', '9', '1', '9', '2', '9', '3', '9', '4', '9', '5', '9', '6', '9', '7', '9', '8', '9', '9',
};

static _Alignas(CACHELINE_SIZE) const char __to_chars_lut_base16[512] = {
	'0', '0', '0', '1', '0', '2', '0', '3', '0', '4', '0', '5', '0', '6', '0', '7',
	'0', '8', '0', '9', '0', 'a', '0', 'b', '0', 'c', '0', 'd', '0', 'e', '0', 'f',
	'1', '0', '1', '1', '1', '2', '1', '3', '1', '4', '1', '5', '1', '6', '1', '7',
	'1', '8', '1', '9', '1', 'a', '1', 'b', '1', 'c', '1', 'd', '1', 'e', '1', 'f',
	'2', '0', '2', '1', '2', '2', '2', '3', '2', '4', '2', '5', '2', '6', '2', '7',
	'2', '8', '2', '9', '2', 'a', '2', 'b', '2', 'c', '2', 'd', '2', 'e', '2', 'f',
	'3', '0', '3', '1', '3', '2', '3', '3', '3', '4', '3', '5', '3', '6', '3', '7',
	'3', '8', '3', '9', '3', 'a', '3', 'b', '3', 'c', '3', 'd', '3', 'e', '3', 'f',
	'4', '0', '4', '1', '4', '2', '4', '3', '4', '4', '4', '5', '4', '6', '4', '7',
	'4', '8', '4', '9', '4', 'a', '4', 'b', '4', 'c', '4', 'd', '4', 'e', '4', 'f',
	'5', '0', '5', '1', '5', '2', '5', '3', '5', '4', '5', '5', '5', '6', '5', '7',
	'5', '8', '5', '9', '5', 'a', '5', 'b', '5', 'c', '5', 'd', '5', 'e', '5', 'f',
	'6', '0', '6', '1', '6', '2', '6', '3', '6', '4', '6', '5', '6', '6', '6', '7',
	'6', '8', '6', '9', '6', 'a', '6', 'b', '6', 'c', '6', 'd', '6', 'e', '6', 'f',
	'7', '0', '7', '1', '7', '2', '7', '3', '7', '4', '7', '5', '7', '6', '7', '7',
	'7', '8', '7', '9', '7', 'a', '7', 'b', '7', 'c', '7', 'd', '7', 'e', '7', 'f',
	'8', '0', '8', '1', '8', '2', '8', '3', '8', '4', '8', '5', '8', '6', '8', '7',
	'8', '8', '8', '9', '8', 'a', '8', 'b', '8', 'c', '8', 'd', '8', 'e', '8', 'f',
	'9', '0', '9', '1', '9', '2', '9', '3', '9', '4', '9', '5', '9', '6', '9', '7',
	'9', '8', '9', '9', '9', 'a', '9', 'b', '9', 'c', '9', 'd', '9', 'e', '9', 'f',
	'a', '0', 'a', '1', 'a', '2', 'a', '3', 'a', '4', 'a', '5', 'a', '6', 'a', '7',
	'a', '8', 'a', '9', 'a', 'a', 'a', 'b', 'a', 'c', 'a', 'd', 'a', 'e', 'a', 'f',
	'b', '0', 'b', '1', 'b', '2', 'b', '3', 'b', '4', 'b', '5', 'b', '6', 'b', '7',
	'b', '8', 'b', '9', 'b', 'a', 'b', 'b', 'b', 'c', 'b', 'd', 'b', 'e', 'b', 'f',
	'c', '0', 'c', '1', 'c', '2', 'c', '3', 'c', '4', 'c', '5', 'c', '6', 'c', '7',
	'c', '8', 'c', '9', 'c', 'a', 'c', 'b', 'c', 'c', 'c', 'd', 'c', 'e', 'c', 'f',
	'd', '0', 'd', '1', 'd', '2', 'd', '3', 'd', '4', 'd', '5', 'd', '6', 'd', '7',
	'd', '8', 'd', '9', 'd', 'a', 'd', 'b', 'd', 'c', 'd', 'd', 'd', 'e', 'd', 'f',
	'e', '0', 'e', '1', 'e', '2', 'e', '3', 'e', '4', 'e', '5', 'e', '6', 'e', '7',
	'e', '8', 'e', '9', 'e', 'a', 'e', 'b', 'e', 'c', 'e', 'd', 'e', 'e', 'e', 'f',
	'f', '0', 'f', '1', 'f', '2', 'f', '3', 'f', '4', 'f', '5', 'f', '6', 'f', '7',
	'f', '8', 'f', '9', 'f', 'a', 'f', 'b', 'f', 'c', 'f', 'd', 'f', 'e', 'f', 'f',
};

static _Alignas(CACHELINE_SIZE) const char __to_chars_lut_base16_upper[512] = {
	'0', '0', '0', '1', '0', '2', '0', '3', '0', '4', '0', '5', '0', '6', '0', '7',
	'0', '8', '0', '9', '0', 'A', '0', 'B', '0', 'C', '0', 'D', '0', 'E', '0', 'F',
	'1', '0', '1', '1', '1', '2', '1', '3', '1', '4', '1', '5', '1', '6', '1', '7',
	'1', '8', '1', '9', '1', 'A', '1', 'B', '1', 'C', '1', 'D', '1', 'E', '1', 'F',
	'2', '0', '2', '1', '2', '2', '2', '3', '2', '4', '2', '5', '2', '6', '2', '7',
	'2', '8', '2', '9', '2', 'A', '2', 'B', '2', 'C', '2', 'D', '2', 'E', '2', 'F',
	'3', '0', '3', '1', '3', '2', '3', '3', '3', '4', '3', '5', '3', '6', '3', '7',
	'3', '8', '3', '9', '3', 'A', '3', 'B', '3', 'C', '3', 'D', '3', 'E', '3', 'F',
	'4', '0', '4', '1', '4', '2', '4', '3', '4', '4', '4', '5', '4', '6', '4', '7',
	'4', '8', '4', '9', '4', 'A', '4', 'B', '4', 'C', '4', 'D', '4', 'E', '4', 'F',
	'5', '0', '5', '1', '5', '2', '5', '3', '5', '4', '5', '5', '5', '6', '5', '7',
	'5', '8', '5', '9', '5', 'A', '5', 'B', '5', 'C', '5', 'D', '5', 'E', '5', 'F',
	'6', '0', '6', '1', '6', '2', '6', '3', '6', '4', '6', '5', '6', '6', '6', '7',
	'6', '8', '6', '9', '6', 'A', '6', 'B', '6', 'C', '6', 'D', '6', 'E', '6', 'F',
	'7', '0', '7', '1', '7', '2', '7', '3', '7', '4', '7', '5', '7', '6', '7', '7',
	'7', '8', '7', '9', '7', 'A', '7', 'B', '7', 'C', '7', 'D', '7', 'E', '7', 'F',
	'8', '0', '8', '1', '8', '2', '8', '3', '8', '4', '8', '5', '8', '6', '8', '7',
	'8', '8', '8', '9', '8', 'A', '8', 'B', '8', 'C', '8', 'D', '8', 'E', '8', 'F',
	'9', '0', '9', '1', '9', '2', '9', '3', '9', '4', '9', '5', '9', '6', '9', '7',
	'9', '8', '9', '9', '9', 'A', '9', 'B', '9', 'C', '9', 'D', '9', 'E', '9', 'F',
	'A', '0', 'A', '1', 'A', '2', 'A', '3', 'A', '4', 'A', '5', 'A', '6', 'A', '7',
	'A', '8', 'A', '9', 'A', 'A', 'A', 'B', 'A', 'C', 'A', 'D', 'A', 'E', 'A', 'F',
	'B', '0', 'B', '1', 'B', '2', 'B', '3', 'B', '4', 'B', '5', 'B', '6', 'B', '7',
	'B', '8', 'B', '9', 'B', 'A', 'B', 'B', 'B', 'C', 'B', 'D', 'B', 'E', 'B', 'F',
	'C', '0', 'C', '1', 'C', '2', 'C', '3', 'C', '4', 'C', '5', 'C', '6', 'C', '7',
	'C', '8', 'C', '9', 'C', 'A', 'C', 'B', 'C', 'C', 'C', 'D', 'C', 'E', 'C', 'F',
	'D', '0', 'D', '1', 'D', '2', 'D', '3', 'D', '4', 'D', '5', 'D', '6', 'D', '7',
	'D', '8', 'D', '9', 'D', 'A', 'D', 'B', 'D', 'C', 'D', 'D', 'D', 'E', 'D', 'F',
	'E', '0', 'E', '1', 'E', '2', 'E', '3', 'E', '4', 'E', '5', 'E', '6', 'E', '7',
	'E', '8', 'E', '9', 'E', 'A', 'E', 'B', 'E', 'C', 'E', 'D', 'E', 'E', 'E', 'F',
	'F', '0', 'F', '1', 'F', '2', 'F', '3', 'F', '4', 'F', '5', 'F', '6', 'F', '7',
	'F', '8', 'F', '9', 'F', 'A', 'F', 'B', 'F', 'C', 'F', 'D', 'F', 'E', 'F', 'F',
};

// TODO make separate functions for base 2, 8, 10, 16?

#define _to_chars_impl(suffix, uint_t, int_t)				\
	static _attr_always_inline					\
	size_t _to_chars_helper##suffix(char *buf, size_t bufsize, uint_t uval, size_t bits, \
					bool is_signed, unsigned int base, bool leading_zeros, \
					bool sign_always, bool uppercase) \
	{								\
		assert(2 <= base && base <= 36);			\
		char sign_char = 0;					\
		if (is_signed) {					\
			int_t sval = uval;				\
			if (sval < 0) {					\
				uval = -(uint_t)sval;			\
				sign_char = '-';			\
			} else if (sign_always) {			\
				sign_char = '+';			\
			}						\
		}							\
									\
		uint_t mask = ((uint_t)-1) >> (8 * sizeof(uint_t) - bits); \
		uval &= mask;						\
									\
		const char *alphabet = "0123456789abcdefghijklmnopqrstuvwxyz"; \
		uint_t tmp = uval;					\
		if (leading_zeros) {					\
			/* determine number of characters based on max value */	\
			tmp = mask;					\
			if (is_signed) {				\
				tmp = tmp / 2 + 1;			\
			}						\
		}							\
		const char *lut = NULL;					\
		uint_t divisor = 0;					\
		uint_t nchars = 0;					\
		size_t n;						\
		switch (base) {						\
		case 2:							\
			n = ilog2(tmp) + 1;				\
			lut = __to_chars_lut_base2;			\
			divisor = 16;					\
			nchars = 4;					\
			break;						\
		case 8:							\
			n = ilog2(tmp) / 3 + 1;				\
			lut = __to_chars_lut_base8;			\
			divisor = 64;					\
			nchars = 2;					\
			break;						\
		case 10:						\
			n = ilog10(tmp) + 1;				\
			lut = __to_chars_lut_base10;			\
			divisor = 100;					\
			nchars = 2;					\
			break;						\
		case 16:						\
			n = ilog2(tmp) / 4 + 1;				\
			lut = __to_chars_lut_base16;			\
			if (uppercase) {				\
				alphabet = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"; \
				lut = __to_chars_lut_base16_upper;	\
			}						\
			divisor = 256;					\
			nchars = 2;					\
			break;						\
		default: {						\
			unsigned int base_2 = base * base;		\
			unsigned int base_4 = base_2 * base_2;		\
			n = 0;						\
			while (tmp >= base_4) {				\
				n += 4;					\
				tmp /= base_4;				\
			}						\
			while (tmp >= base_2) {				\
				n += 2;					\
				tmp /= base_2;				\
			}						\
			do {						\
				n++;					\
				tmp /= base;				\
			} while (tmp != 0);				\
			if (uppercase) {				\
				alphabet = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"; \
			}						\
		}							\
		}							\
		size_t total_length = n + (sign_char ? 1 : 0);		\
		if (!buf || bufsize < total_length) {			\
			return total_length;				\
		}							\
		char *p = buf;						\
		if (sign_char) {					\
			*p++ = sign_char;				\
		}							\
		if (lut) {						\
			while (uval >= divisor) {			\
				uint_t rem = uval % divisor;		\
				uval /= divisor;			\
				n -= nchars;				\
				memcpy(&p[n], &lut[nchars * rem], nchars); \
			}						\
		}							\
		if (n != 0) {						\
			do {						\
				p[--n] = alphabet[uval % base];		\
				uval /= base;				\
			} while (uval != 0);				\
		}							\
		if (leading_zeros) {					\
			while (n-- > 0) {				\
				p[n] = '0';				\
			}						\
		}							\
		return total_length;					\
	}								\
									\
	static size_t _to_chars##suffix(char *buf, size_t bufsize, uint_t uval, size_t bits, \
					unsigned int flags, bool is_signed) \
	{								\
		unsigned int base = flags & __TO_CHARS_BASE_MASK;	\
		if (base == 0) {					\
			base = 10;					\
		}							\
		bool leading_zeros = unlikely(flags & TO_CHARS_LEADING_ZEROS); \
		bool sign_always = unlikely(flags & TO_CHARS_PLUS_SIGN); \
		bool uppercase = unlikely(flags & TO_CHARS_UPPERCASE);	\
		if (likely(base == 10)) {				\
			return _to_chars_helper##suffix(buf, bufsize, uval, bits, is_signed, 10, \
							leading_zeros, sign_always, uppercase);	\
		}							\
		if (likely(base == 16)) {				\
			return _to_chars_helper##suffix(buf, bufsize, uval, bits, is_signed, 16, \
							leading_zeros, sign_always, uppercase);	\
		}							\
		if (likely(base == 2)) {				\
			return _to_chars_helper##suffix(buf, bufsize, uval, bits, is_signed, 2,	\
							leading_zeros, sign_always, uppercase); \
		}							\
		if (likely(base == 8)) {				\
			return _to_chars_helper##suffix(buf, bufsize, uval, bits, is_signed, 8,	\
							leading_zeros, sign_always, uppercase); \
		}							\
		_fortify_check(2 <= base && base <= 36);		\
		return _to_chars_helper##suffix(buf, bufsize, uval, bits, is_signed, base, \
						leading_zeros, sign_always, uppercase); \
	}

_to_chars_impl(32, uint32_t, int32_t)
_to_chars_impl(64, uint64_t, int64_t)

#define __TO_CHARS_FUNC(name, type)					\
	size_t (to_chars_##name)(char *buf, size_t bufsize, type val, unsigned int flags) \
	{								\
		return sizeof(type) <= 4 ?				\
			_to_chars32(buf, bufsize, val, sizeof(type) * 8, flags, type_is_signed(type)) : \
			_to_chars64(buf, bufsize, val, sizeof(type) * 8, flags, type_is_signed(type)); \
	}
__CHARCONV_FOREACH_INTTYPE(__TO_CHARS_FUNC)
#undef __TO_CHARS_FUNC

static _Alignas(CACHELINE_SIZE) const unsigned char from_chars_lut[UCHAR_MAX] = {
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	00,  1,  2,  3,  4,  5,  6,  7,  8,  9, -1, -1, -1, -1, -1, -1,
	-1, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
	25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, -1, -1, -1, -1, -1,
	-1, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
	25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
};

// TODO add 32 bit version for performance?
// TODO try the strategy from the libcxx or stdlibc++ implementation
struct from_chars_result _from_chars(const char *chars, size_t maxlen, uint64_t *retval,
				     unsigned char base, unsigned long long cutoff, unsigned char cutlim)
{
	uint64_t res = 0;
	bool overflow = false;
	size_t i;
	for (i = 0; i < maxlen; i++) {
		unsigned char c = from_chars_lut[(unsigned char)chars[i]];
		if (c >= base) {
			break;
		}
		if (res > cutoff || (res == cutoff && c > cutlim)) {
			overflow = true;
			continue;
		}
		res = res * base + c;
	}
	*retval = res;
	bool ok = !overflow && (i == maxlen || chars[i] == '\0') && likely(i != 0);
	return (struct from_chars_result) {
		.ok = ok,
		.overflow = overflow,
		.nchars = i
	};
}

static size_t _from_chars_detect_base(const char *chars, size_t maxlen, unsigned int flags,
				       unsigned char *base)
{
	unsigned char b = flags & __FROM_CHARS_BASE_MASK;
	if (b != FROM_CHARS_AUTODETECT_BASE) {
		*base = b;
		return 0;
	}
	if (maxlen <= 1 || chars[0] != '0') {
		*base = 10;
		return 0;
	}
	if (chars[1] == 'x' || chars[1] == 'X') {
		*base = 16;
		return 2;
	}
	if (chars[1] == 'b' || chars[1] == 'B') {
		*base = 2;
		return 2;
	}
	if (chars[1] == 'o' || chars[1] == 'O') {
		*base = 8;
		return 2;
	}
	*base = 10;
	return 0;
}

#define __FROM_CHARS_FUNC(name, type)					\
	struct from_chars_result (from_chars_##name)(const char *chars, size_t maxlen, type *retval, \
						     unsigned int flags) \
	{								\
		typedef to_unsigned_type(type) unsigned_type;		\
		/* TODO prefetch lookup table? */			\
		bool negative = false;					\
		size_t i = 0;						\
		if (type_is_signed(type) && likely(maxlen > 0) && (chars[0] == '-' || chars[0] == '+')) { \
			negative = chars[0] == '-';			\
			i = 1;						\
		}							\
		unsigned char base;					\
		i += _from_chars_detect_base(chars + i, maxlen - i, flags, &base); \
		_fortify_check(2 <= base && base <= 36);		\
		unsigned long long cutoff = ((unsigned_type)max_value(type) + negative) / base; \
		unsigned char cutlim = ((unsigned_type)max_value(type) + negative) % base; \
		uint64_t v;						\
		struct from_chars_result res = _from_chars(chars + i, maxlen - i, &v, base, cutoff, cutlim); \
		res.nchars += i;					\
		if (res.ok) {						\
			*retval = negative ? -v : v;			\
		}							\
		return res;						\
	}
__CHARCONV_FOREACH_INTTYPE(__FROM_CHARS_FUNC)
#undef __FROM_CHARS_FUNC
