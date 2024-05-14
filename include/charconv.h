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

#include "compiler.h"

enum to_chars_flags {
	TO_CHARS_DEFAULT = 0, // equivalent to TO_CHARS_DECIMAL, allows passing literal 0 for convenience

	TO_CHARS_BINARY = 2, // %b
	TO_CHARS_OCTAL = 8, // %o
	TO_CHARS_DECIMAL = 10, // %d, %u
	TO_CHARS_HEXADECIMAL = 16, // %x

	__TO_CHARS_BASE_MASK = 63, // base must be between 2 and 36 inclusive

	TO_CHARS_LEADING_ZEROS = 128, // %0n with n=maxlen for the specified conversion
	TO_CHARS_PLUS_SIGN = 256, // %+
	TO_CHARS_UPPERCASE = 512, // %x -> %X
};

// TODO fortify these functions?
// pass NULL for 'buf' to get the required size
size_t to_chars_char(char *buf, char val, unsigned int flags);
size_t to_chars_schar(char *buf, signed char val, unsigned int flags);
size_t to_chars_uchar(char *buf, unsigned char val, unsigned int flags);
size_t to_chars_short(char *buf, short val, unsigned int flags);
size_t to_chars_ushort(char *buf, unsigned short val, unsigned int flags);
size_t to_chars_int(char *buf, int val, unsigned int flags);
size_t to_chars_uint(char *buf, unsigned int val, unsigned int flags);
size_t to_chars_long(char *buf, long val, unsigned int flags);
size_t to_chars_ulong(char *buf, unsigned long val, unsigned int flags);
size_t to_chars_llong(char *buf, long long val, unsigned int flags);
size_t to_chars_ullong(char *buf, unsigned long long val, unsigned int flags);
#define to_chars(buf, val, flags) _Generic(val,				\
					   char : to_chars_char(buf, (char)val, flags), \
					   unsigned char : to_chars_uchar(buf, (unsigned char)val, flags), \
					   unsigned short : to_chars_ushort(buf, (unsigned short)val, flags), \
					   unsigned int : to_chars_uint(buf, (unsigned int)val, flags), \
					   unsigned long : to_chars_ulong(buf, (unsigned long)val, flags), \
					   unsigned long long : to_chars_ullong(buf, (unsigned long long)val, flags), \
					   signed char : to_chars_schar(buf, (signed char)val, flags), \
					   signed short : to_chars_short(buf, (signed short)val, flags), \
					   signed int : to_chars_int(buf, (signed int)val, flags), \
					   signed long : to_chars_long(buf, (signed long)val, flags), \
					   signed long long : to_chars_llong(buf, (signed long long)val, flags))

#if 0 // TODO implement
enum from_chars_flags {
	FROM_CHARS_AUTODETECT_BASE = 0, // autodetect base according to C syntax

	FROM_CHARS_BINARY = 2, // %b
	FROM_CHARS_OCTAL = 8, // %o
	FROM_CHARS_DECIMAL = 10, // %d, %u
	FROM_CHARS_HEXADECIMAL = 16, // %x

	__FROM_CHARS_BASE_MASK = 63, // base must be between 2 and 36 inclusive

	// TODO FROM_CHARS_SKIP_LEADING_WHITESPACE
};

size_t from_chars_char(char *chars, size_t nchars, char *retval, unsigned int flags);
size_t from_chars_schar(char *chars, size_t nchars, signed char *retval, unsigned int flags);
size_t from_chars_uchar(char *chars, size_t nchars, unsigned char *retval, unsigned int flags);
size_t from_chars_short(char *chars, size_t nchars, short *retval, unsigned int flags);
size_t from_chars_ushort(char *chars, size_t nchars, unsigned short *retval, unsigned int flags);
size_t from_chars_int(char *chars, size_t nchars, int *retval, unsigned int flags);
size_t from_chars_uint(char *chars, size_t nchars, unsigned int *retval, unsigned int flags);
size_t from_chars_long(char *chars, size_t nchars, long *retval, unsigned int flags);
size_t from_chars_ulong(char *chars, size_t nchars, unsigned long *retval, unsigned int flags);
size_t from_chars_llong(char *chars, size_t nchars, long long *retval, unsigned int flags);
size_t from_chars_ullong(char *chars, size_t nchars, unsigned long long *retval, unsigned int flags);
#define from_chars(chars, nchars, retval, flags) _Generic(retval,	\
							  char * : from_chars_char(chars, nchars, (char *)retval, flags), \
							  unsigned char * : from_chars_char(chars, nchars, (unsigned char *)retval, flags), \
							  unsigned short * : from_chars_char(chars, nchars, (unsigned short *)retval, flags), \
							  unsigned int * : from_chars_char(chars, nchars, (unsigned int *)retval, flags), \
							  unsigned long * : from_chars_char(chars, nchars, (unsigned long *)retval, flags), \
							  unsigned long long * : from_chars_char(chars, nchars, (unsigned long long *)retval, flags), \
							  signed char * : from_chars_char(chars, nchars, (signed char *)retval, flags), \
							  signed short * : from_chars_char(chars, nchars, (signed short *)retval, flags), \
							  signed int * : from_chars_char(chars, nchars, (signed int *)retval, flags), \
							  signed long * : from_chars_char(chars, nchars, (signed long *)retval, flags), \
							  signed long long * : from_chars_char(chars, nchars, (signed long long *)retval, flags))
#endif
