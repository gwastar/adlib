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

#define _GNU_SOURCE

#include <assert.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "compiler.h"
#include "config.h"
#include "dstring.h"
#include "fortify.h"
#include "stringview.h"

struct dstr_small {
	uint8_t capacity;
	uint8_t length;
	char    characters[];
};
_Static_assert(sizeof(struct dstr_small) == offsetof(struct dstr_small, characters), "");

struct dstr_medium {
	uint16_t capacity;
	uint16_t length;
	uint8_t  is_big;
	uint8_t  _small_length;
	char     characters[];
};
_Static_assert(sizeof(struct dstr_medium) == offsetof(struct dstr_medium, characters), "");
_Static_assert(offsetof(struct dstr_medium, characters) - offsetof(struct dstr_medium, _small_length) ==
	       offsetof(struct dstr_small, characters) - offsetof(struct dstr_small, length), "");

// if _attr_packed is not available then sizeof(struct dstr_big) != offsetof(struct dstr_big, characters)
// which is why the code for dstr_big is different than the code for dstr_small and dstr_medium below
struct dstr_big {
	size_t   capacity;
	size_t   length;
	uint8_t  is_big;
	uint8_t  _small_length;
	char     characters[];
#ifdef HAVE_ATTR_PACKED
} _attr_packed;
#else
};
#endif
_Static_assert(offsetof(struct dstr_big, characters) - offsetof(struct dstr_big, _small_length) ==
	       offsetof(struct dstr_small, characters) - offsetof(struct dstr_small, length), "");

struct dstr_common {
	union {
		uint8_t  is_big;
		uint8_t  small_capacity;
	};
	uint8_t  small_length;
	char     characters[];
};
_Static_assert(sizeof(struct dstr_common) == sizeof(struct dstr_small), "");
_Static_assert(sizeof(struct dstr_common) == offsetof(struct dstr_common, characters), "");
_Static_assert(offsetof(struct dstr_common, characters) - offsetof(struct dstr_common, small_length) ==
	       offsetof(struct dstr_small, characters) - offsetof(struct dstr_small, length), "");
_Static_assert(offsetof(struct dstr_common, characters) - offsetof(struct dstr_common, is_big) ==
	       offsetof(struct dstr_medium, characters) - offsetof(struct dstr_medium, is_big), "");
_Static_assert(offsetof(struct dstr_common, characters) - offsetof(struct dstr_common, is_big) ==
	       offsetof(struct dstr_big, characters) - offsetof(struct dstr_big, is_big), "");

enum dstr_type {
	DSTR_SMALL,
	DSTR_MEDIUM,
	DSTR_BIG
};

static const uint8_t _dstr_empty_dstr_bytes[3] = { 0 /* length */, 0 /* capacity */, 0 /* null terminator */};
static const dstr_t dstr_empty_dstr = ((struct dstr_small *)_dstr_empty_dstr_bytes)->characters;

static inline enum dstr_type dstr_get_type(const dstr_t dstr)
{
	const struct dstr_common *header = (const struct dstr_common *)dstr - 1;
	if (likely(header->small_length != UINT8_MAX)) {
		return DSTR_SMALL;
	}
	return unlikely(header->is_big) ? DSTR_BIG : DSTR_MEDIUM;
}

size_t dstr_length(const dstr_t dstr)
{
	switch (dstr_get_type(dstr)) {
	case DSTR_SMALL: {
		const struct dstr_small *header = (const struct dstr_small *)dstr - 1;
		return header->length;
	}
	case DSTR_MEDIUM: {
		const struct dstr_medium *header = (const struct dstr_medium *)dstr - 1;
		return header->length;
	}
	case DSTR_BIG: {
		const struct dstr_big *header = (const struct dstr_big *)(dstr - offsetof(struct dstr_big,
											    characters));
		return header->length;
	}
	}
	unreachable();
	return 0;
}

bool dstr_is_empty(const dstr_t dstr)
{
	return dstr_length(dstr) == 0;
}

size_t dstr_capacity(const dstr_t dstr)
{
	switch (dstr_get_type(dstr)) {
	case DSTR_SMALL: {
		const struct dstr_small *header = (const struct dstr_small *)dstr - 1;
		return header->capacity;
	}
	case DSTR_MEDIUM: {
		const struct dstr_medium *header = (const struct dstr_medium *)dstr - 1;
		return header->capacity;
	}
	case DSTR_BIG: {
		const struct dstr_big *header = (const struct dstr_big *)(dstr - offsetof(struct dstr_big,
											  characters));
		return header->capacity;
	}
	}
	unreachable();
	return 0;
}

static void dstr_set_length(dstr_t dstr, size_t length)
{
	assert(length <= dstr_capacity(dstr));
	if (unlikely(dstr == dstr_empty_dstr)) {
		return;
	}
	dstr[length] = '\0';
	switch (dstr_get_type(dstr)) {
	case DSTR_SMALL: {
		struct dstr_small *header = (struct dstr_small *)dstr - 1;
		header->length = length;
		break;
	}
	case DSTR_MEDIUM: {
		struct dstr_medium *header = (struct dstr_medium *)dstr - 1;
		header->length = length;
		break;
	}
	case DSTR_BIG: {
		struct dstr_big *header = (struct dstr_big *)(dstr - offsetof(struct dstr_big,
										characters));
		header->length = length;
		break;
	}
	}
}

static _attr_pure size_t dstr_header_size(enum dstr_type type)
{
	switch (type) {
	case DSTR_SMALL:  return sizeof(struct dstr_small);
	case DSTR_MEDIUM: return sizeof(struct dstr_medium);
	case DSTR_BIG:    return offsetof(struct dstr_big, characters);
	}
	assert(false);
	unreachable();
}

void dstr_resize(dstr_t *dstrp, size_t new_capacity)
{
	if (unlikely(new_capacity == 0)) {
		if (likely(*dstrp != dstr_empty_dstr)) {
			size_t header_size = dstr_header_size(dstr_get_type(*dstrp));
			free((*dstrp) - header_size);
			*dstrp = dstr_empty_dstr;
		}
		return;
	}

	if (unlikely(new_capacity == dstr_capacity(*dstrp))) {
		return;
	}

	size_t old_length = dstr_length(*dstrp);
	size_t new_length = old_length;
	if (unlikely(old_length > new_capacity)) {
		// this currently only happens when a user calls this function directly to truncate the string
		new_length = new_capacity;
		(*dstrp)[new_length] = '\0';
	}

	enum dstr_type new_type = DSTR_SMALL;
	if (unlikely(new_length >= UINT8_MAX || new_capacity > UINT8_MAX)) {
		new_type = DSTR_MEDIUM;
		if (unlikely(new_length > UINT16_MAX || new_capacity > UINT16_MAX)) {
			new_type = DSTR_BIG;
		}
	}

	size_t new_header_size = dstr_header_size(new_type);
	size_t new_alloc_size = new_header_size + new_capacity + 1; // +1 for the null byte
	uint8_t *h;
	if (*dstrp == dstr_empty_dstr) {
		h = malloc(new_alloc_size);
		if (unlikely(!h)) {
			abort();
		}
		h[new_header_size] = '\0';
	} else {
		size_t old_header_size = dstr_header_size(dstr_get_type(*dstrp));
		h = (uint8_t *)(*dstrp) - old_header_size;
		if (unlikely(old_header_size > new_header_size)) {
			uint8_t *src = h + old_header_size;
			uint8_t *dst = h + new_header_size;
			memmove(dst, src, new_length + 1); // +1 for the null byte
		}
		h = realloc(h, new_alloc_size);
		if (unlikely(!h)) {
			abort();
		}
		if (unlikely(old_header_size < new_header_size)) {
			uint8_t *src = h + old_header_size;
			uint8_t *dst = h + new_header_size;
			memmove(dst, src, new_length + 1); // +1 for the null byte
		}
	}

	switch (new_type) {
	case DSTR_SMALL: {
		struct dstr_small *header = (struct dstr_small *)h;
		header->length = new_length;
		header->capacity = new_capacity;
		*dstrp = header->characters;
		break;
	}
	case DSTR_MEDIUM: {
		struct dstr_medium *header = (struct dstr_medium *)h;
		header->length = new_length;
		header->capacity = new_capacity;
		header->is_big = false;
		header->_small_length = UINT8_MAX;
		*dstrp = header->characters;
		break;
	}
	case DSTR_BIG: {
		struct dstr_big *header = (struct dstr_big *)h;
		header->length = new_length;
		header->capacity = new_capacity;
		header->is_big = true;
		header->_small_length = UINT8_MAX;
		*dstrp = header->characters;
		break;
	}
	}
}

void dstr_free(dstr_t *dstrp)
{
	dstr_resize(dstrp, 0);
	*dstrp = NULL;
}

static void dstr_grow(dstr_t *dstrp, size_t n)
{
	if (unlikely(n == 0)) {
		return;
	}
	size_t capacity = dstr_capacity(*dstrp);
	_fortify_check(SIZE_MAX - n >= capacity);
	const size_t numerator = DSTRING_GROWTH_FACTOR_NUMERATOR;
	const size_t denominator = DSTRING_GROWTH_FACTOR_DENOMINATOR;
	size_t new_capacity = (capacity + denominator - 1) / denominator * numerator;
	if (unlikely(new_capacity < capacity + n)) {
		new_capacity = capacity + n;
	}
	if (unlikely(new_capacity < DSTRING_INITIAL_SIZE)) {
		new_capacity = DSTRING_INITIAL_SIZE;
	}
	dstr_resize(dstrp, new_capacity);
}

void dstr_reserve(dstr_t *dstrp, size_t n)
{
	size_t rem = dstr_capacity(*dstrp) - dstr_length(*dstrp);
	if (n > rem) {
		dstr_grow(dstrp, n - rem);
	}
}

void dstr_clear(dstr_t *dstrp)
{
	dstr_set_length(*dstrp, 0);
}

void dstr_shrink_to_fit(dstr_t *dstrp)
{
	dstr_resize(dstrp, dstr_length(*dstrp));
}

dstr_t dstr_new(void)
{
	return dstr_empty_dstr;
}

dstr_t dstr_with_capacity(size_t capacity)
{
	dstr_t dstr = dstr_new();
	dstr_reserve(&dstr, capacity);
	return dstr;
}

static _attr_always_inline char *dstr_replace_internal(dstr_t *dstrp, size_t pos, size_t len, size_t n)
{
	size_t length = dstr_length(*dstrp);
	_fortify_check(pos <= length);
	if (len == DSTR_NPOS) {
		len = length - pos;
	}
	_fortify_check(pos + len <= length);
	if (n == len) {
		return &(*dstrp)[pos];
	}
	if (n > len) {
		dstr_reserve(dstrp, n - len);
	}
	char *p = &(*dstrp)[pos];
	memmove(p + n, p + len, length - (pos + len));
	length += n - len;
	dstr_set_length(*dstrp, length);
	return p;
}

void dstr_append_char(dstr_t *dstrp, char c)
{
	dstr_reserve(dstrp, 1);
	size_t length = dstr_length(*dstrp);
	(*dstrp)[length] = c;
	dstr_set_length(*dstrp, length + 1);
}

static _attr_always_inline void dstr_append_impl(dstr_t *dstrp, const char *chars, size_t n)
{
	char *p = dstr_replace_internal(dstrp, dstr_length(*dstrp), 0, n);
	memcpy(p, chars, n);
}

void (dstr_append_chars)(dstr_t *dstrp, const char *chars, size_t n)
{
	dstr_append_impl(dstrp, chars, n);
}

void dstr_append_dstr(dstr_t *dstrp, const dstr_t dstr)
{
	_fortify_check(dstr == dstr_empty_dstr || *dstrp != dstr); // TODO? *dstrp must be != dstr currently
	dstr_append_impl(dstrp, dstr, dstr_length(dstr));
}

void dstr_append_cstr(dstr_t *dstrp, const char *cstr)
{
	dstr_append_impl(dstrp, cstr, strlen(cstr));
}

void dstr_append_view(dstr_t *dstrp, struct strview view)
{
	dstr_append_impl(dstrp, view.characters, view.length);
}

size_t dstr_append_fmt(dstr_t *dstrp, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	size_t n = dstr_append_fmtv(dstrp, fmt, args);
	va_end(args);
	return n;
}

size_t dstr_append_fmtv(dstr_t *dstrp, const char *fmt, va_list args)
{
	return dstr_replace_fmtv(dstrp, dstr_length(*dstrp), 0, fmt, args);
}

char *dstr_append_uninitialized(dstr_t *dstrp, size_t uninit_len)
{
	return dstr_replace_internal(dstrp, dstr_length(*dstrp), 0, uninit_len);
}

static _attr_always_inline void dstr_insert_impl(dstr_t *dstrp, size_t pos, const char *chars, size_t n)
{
	char *p = dstr_replace_internal(dstrp, pos, 0, n);
	memcpy(p, chars, n);
}

void (dstr_insert_chars)(dstr_t *dstrp, size_t pos, const char *chars, size_t n)
{
	dstr_insert_impl(dstrp, pos, chars, n);
}

void dstr_insert_char(dstr_t *dstrp, size_t pos, char c)
{
	dstr_insert_impl(dstrp, pos, &c, 1);
}

void dstr_insert_dstr(dstr_t *dstrp, size_t pos, const dstr_t dstr)
{
	_fortify_check(dstr == dstr_empty_dstr || *dstrp != dstr); // TODO? *dstrp must be != dstr currently
	dstr_insert_impl(dstrp, pos, dstr, dstr_length(dstr));
}

void dstr_insert_cstr(dstr_t *dstrp, size_t pos, const char *cstr)
{
	dstr_insert_impl(dstrp, pos, cstr, strlen(cstr));
}

void dstr_insert_view(dstr_t *dstrp, size_t pos, struct strview view)
{
	dstr_insert_impl(dstrp, pos, view.characters, view.length);
}

size_t dstr_insert_fmt(dstr_t *dstrp, size_t pos, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	size_t n = dstr_insert_fmtv(dstrp, pos, fmt, args);
	va_end(args);
	return n;
}

size_t dstr_insert_fmtv(dstr_t *dstrp, size_t pos, const char *fmt, va_list args)
{
	return dstr_replace_fmtv(dstrp, pos, 0, fmt, args);
}

char *dstr_insert_uninitialized(dstr_t *dstrp, size_t pos, size_t uninit_len)
{
	return dstr_replace_internal(dstrp, pos, 0, uninit_len);
}

static _attr_always_inline void dstr_replace_impl(dstr_t *dstrp, size_t pos, size_t len,
						  const char *chars, size_t n)
{
	char *p = dstr_replace_internal(dstrp, pos, len, n);
	memcpy(p, chars, n);
}

void (dstr_replace_chars)(dstr_t *dstrp, size_t pos, size_t len, const char *chars, size_t n)
{
	dstr_replace_impl(dstrp, pos, len, chars, n);
}

void dstr_replace_dstr(dstr_t *dstrp, size_t pos, size_t len, const dstr_t dstr)
{
	_fortify_check(dstr == dstr_empty_dstr || *dstrp != dstr); // TODO? *dstrp must be != dstr currently
	dstr_replace_impl(dstrp, pos, len, dstr, dstr_length(dstr));
}

void dstr_replace_cstr(dstr_t *dstrp, size_t pos, size_t len, const char *cstr)
{
	dstr_replace_impl(dstrp, pos, len, cstr, strlen(cstr));
}

void dstr_replace_view(dstr_t *dstrp, size_t pos, size_t len, struct strview view)
{
	dstr_replace_impl(dstrp, pos, len, view.characters, view.length);
}

size_t dstr_replace_fmt(dstr_t *dstrp, size_t pos, size_t len, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	size_t n = dstr_replace_fmtv(dstrp, pos, len, fmt, args);
	va_end(args);
	return n;
}

size_t dstr_replace_fmtv(dstr_t *dstrp, size_t pos, size_t len, const char *fmt, va_list args)
{
	char buf[128];

	va_list args_copy;
	va_copy(args_copy, args);
	size_t n = vsnprintf(buf, sizeof(buf), fmt, args_copy);
	va_end(args_copy);

	char *p = dstr_replace_internal(dstrp, pos, len, n);
	if (n < sizeof(buf)) {
		memcpy(p, buf, n);
		return n;
	}

	// vsnprintf will overwrite p[n] with a null byte.
	// If pos == length, than p[n] is already zero,
	// but otherwise we need to save and restore p[n]
	char saved_char = p[n];
	vsnprintf(p, n + 1, fmt, args);
	p[n] = saved_char;
	return n;
}

char *dstr_replace_uninitialized(dstr_t *dstrp, size_t pos, size_t len, size_t uninit_len)
{
	return dstr_replace_internal(dstrp, pos, len, uninit_len);
}

void dstr_erase(dstr_t *dstrp, size_t pos, size_t len)
{
	dstr_replace_internal(dstrp, pos, len, 0);
}

static void dstr_strip_impl(dstr_t dstr, const char *strip, bool left, bool right)
{
	if (right) {
		size_t pos = dstr_find_last_not_of(dstr, strip, DSTR_NPOS);
		if (pos == DSTR_NPOS) {
			dstr_set_length(dstr, 0);
			return;
		}
		dstr_set_length(dstr, pos + 1);
	}
	if (left) {
		size_t pos = dstr_find_first_not_of(dstr, strip, 0);
		if (pos == DSTR_NPOS) {
			dstr_set_length(dstr, 0);
			return;
		}
		size_t len = dstr_length(dstr) - pos;
		memmove(dstr, dstr + pos, len);
		dstr_set_length(dstr, len);
	}
}

void dstr_strip(dstr_t *dstrp, const char *strip)
{
	dstr_strip_impl(*dstrp, strip, true, true);
}

void dstr_lstrip(dstr_t *dstrp, const char *strip)
{
	dstr_strip_impl(*dstrp, strip, true, false);
}

void dstr_rstrip(dstr_t *dstrp, const char *strip)
{
	dstr_strip_impl(*dstrp, strip, false, true);
}

static _attr_always_inline dstr_t dstr_from_chars_impl(const char *chars, size_t n)
{
	dstr_t dstr = dstr_new();
	dstr_append_impl(&dstr, chars, n);
	return dstr;
}

dstr_t (dstr_from_chars)(const char *chars, size_t n)
{
	return dstr_from_chars_impl(chars, n);
}

dstr_t dstr_from_cstr(const char *cstr)
{
	return dstr_from_chars_impl(cstr, strlen(cstr));
}

dstr_t dstr_from_view(struct strview view)
{
	return dstr_from_chars_impl(view.characters, view.length);
}

dstr_t dstr_from_fmt(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	dstr_t dstr = dstr_from_fmtv(fmt, args);
	va_end(args);
	return dstr;
}

dstr_t dstr_from_fmtv(const char *fmt, va_list args)
{
	dstr_t dstr = dstr_new();
	dstr_append_fmtv(&dstr, fmt, args);
	return dstr;
}

// dstr_t _dstr_join(const char *separator, ...)
// {
// 	va_list args;
// 	va_start(args, separator);
// 	struct strview separator_view = strview_from_cstr(separator);
// 	dstr_t dstr = dstr_new_empty();
// 	for (bool first = true;; first = false) {
// 		const dstr_t s = va_arg(args, const dstr_t);
// 		if (!s) {
// 			break;
// 		}
// 		if (!first) {
// 			dstr_append_view(&dstr, separator_view);
// 		}
// 		dstr_append(&dstr, s);
// 	}
// 	va_end(args);
// 	return dstr;
// }

// dstr_t _dstr_join_views(const char *separator, ...)
// {
// 	va_list args;
// 	va_start(args, separator);
// 	struct strview separator_view = strview_from_cstr(separator);
// 	dstr_t dstr = dstr_new_empty();
// 	for (bool first = true;; first = false) {
// 		struct strview s = va_arg(args, struct strview);
// 		if (!s.characters) {
// 			break;
// 		}
// 		if (!first) {
// 			dstr_append_view(&dstr, separator_view);
// 		}
// 		dstr_append_view(&dstr, s);
// 	}
// 	va_end(args);
// 	return dstr;
// }

// dstr_t _dstr_join_cstrs(const char *separator, ...)
// {

// 	va_list args;
// 	va_start(args, separator);
// 	struct strview separator_view = strview_from_cstr(separator);
// 	dstr_t dstr = dstr_new_empty();
// 	for (bool first = true;; first = false) {
// 		const char *s = va_arg(args, const char *);
// 		if (!s) {
// 			break;
// 		}
// 		if (!first) {
// 			dstr_append_view(&dstr, separator_view);
// 		}
// 		dstr_append_cstr(&dstr, s);
// 	}
// 	va_end(args);
// 	return dstr;
// }

dstr_t dstr_copy(const dstr_t dstr)
{
	return dstr_from_chars_impl(dstr, dstr_length(dstr));
}

char *dstr_to_cstr(dstr_t *dstrp)
{
	size_t length = dstr_length(*dstrp);
	size_t header_size = dstr_header_size(dstr_get_type(*dstrp));
	char *p = *dstrp - header_size;
	*dstrp = NULL;
	memmove(p, p + header_size, length + 1);
	char *p2 = realloc(p, length + 1);
	if (p2) {
		p = p2;
	}
	return p;
}

char *dstr_to_cstr_copy(const dstr_t dstr)
{
	return strview_to_cstr(dstr_view(dstr));
}

struct strview dstr_view(const dstr_t dstr)
{
	return (struct strview){
		.characters = dstr,
		.length = dstr_length(dstr),
	};
}

struct strview dstr_substring_view(const dstr_t dstr, size_t start, size_t length)
{
	return strview_substring(dstr_view(dstr), start, length);
}

void dstr_substring(dstr_t *dstrp, size_t start, size_t length)
{
	struct strview view = dstr_substring_view(*dstrp, start, length);
	memmove(*dstrp, view.characters, view.length);
	dstr_set_length(*dstrp, view.length);
}

dstr_t dstr_substring_copy(const dstr_t dstr, size_t start, size_t length)
{
	struct strview view = dstr_substring_view(dstr, start, length);
	return dstr_from_chars_impl(view.characters, view.length);
}

int dstr_compare_dstr(const dstr_t dstr1, const dstr_t dstr2)
{
	return strcmp(dstr1, dstr2);
}

int dstr_compare_view(const dstr_t dstr, struct strview view)
{
	return strview_compare(dstr_view(dstr), view);
}

int dstr_compare_cstr(const dstr_t dstr, const char *cstr)
{
	return strcmp(dstr, cstr);
}

bool dstr_equals_dstr(const dstr_t dstr1, const dstr_t dstr2)
{
	return strview_equal(dstr_view(dstr1), dstr_view(dstr2));
}

bool dstr_equals_view(const dstr_t dstr, struct strview view)
{
	return strview_equal(dstr_view(dstr), view);
}

bool dstr_equals_cstr(const dstr_t dstr, const char *cstr)
{
	return strcmp(dstr, cstr) == 0;
}

size_t dstr_find_dstr(const dstr_t haystack, const dstr_t needle, size_t pos)
{
	// TODO test that both versions behave the same for pos > length(haystack) and length(needle) ==/!= 0
#if defined(HAVE_MEMMEM) && defined(_GNU_SOURCE)
	// if memmem is available, this might be a little faster since we already know the lengths
	return strview_find(dstr_view(haystack), dstr_view(needle), pos);
#else
	// if memmem is not available, this is probably faster than the naive implementation in strview_find
	return dstr_find_cstr(haystack, needle, pos);
#endif
}

size_t dstr_find_view(const dstr_t haystack, struct strview needle_view, size_t pos)
{
	return strview_find(dstr_view(haystack), needle_view, pos);
}

size_t dstr_find_cstr(const dstr_t haystack, const char *needle_cstr, size_t pos)
{
	size_t len = dstr_length(haystack);
	if (unlikely(pos > len)) {
		pos = len;
	}
	const char *found = strstr(haystack + pos, needle_cstr);
	return found ? (size_t)(found - haystack) : DSTR_NPOS;
}

size_t dstr_rfind_dstr(const dstr_t haystack, const dstr_t needle, size_t pos)
{
	return strview_rfind(dstr_view(haystack), dstr_view(needle), pos);
}

size_t dstr_rfind_view(const dstr_t haystack, struct strview needle, size_t pos)
{
	return strview_rfind(dstr_view(haystack), needle, pos);
}

size_t dstr_rfind_cstr(const dstr_t haystack, const char *needle_cstr, size_t pos)
{
	return strview_rfind(dstr_view(haystack), strview_from_cstr(needle_cstr), pos);
}


size_t dstr_find_replace_view(dstr_t *haystackp, struct strview needle_view, struct strview view, size_t max)
{
	if (max > dstr_length(*haystackp) + 1) {
		max = dstr_length(*haystackp) + 1;
	}
	size_t skip = needle_view.length == 0 ? 1 : 0;
	size_t start = 0;
	size_t replaced = 0;
	while (replaced < max) {
		struct strview haystack_view = dstr_substring_view(*haystackp, start, DSTR_NPOS);
		size_t pos = strview_find(haystack_view, needle_view, 0);
		if (pos == STRVIEW_NPOS) {
			break;
		}
		pos += start;
		dstr_replace_view(haystackp, pos, needle_view.length, view);
		start = pos + view.length + skip;
		replaced++;
	}
	return replaced;
}

size_t dstr_find_replace_dstr(dstr_t *haystackp, const dstr_t needle, const dstr_t dstr, size_t max)
{
	return dstr_find_replace_view(haystackp, dstr_view(needle), dstr_view(dstr), max);
}

size_t dstr_find_replace_cstr(dstr_t *haystackp, const char *needle_cstr, const char *cstr, size_t max)
{
	return dstr_find_replace_view(haystackp, strview_from_cstr(needle_cstr),
				      strview_from_cstr(cstr), max);
}

size_t dstr_rfind_replace_view(dstr_t *haystackp, struct strview needle_view, struct strview view, size_t max)
{
	if (max > dstr_length(*haystackp) + 1) {
		max = dstr_length(*haystackp) + 1;
	}
	size_t skip = needle_view.length == 0 ? 1 : 0;
	size_t end = dstr_length(*haystackp);
	size_t replaced = 0;
	while (replaced < max) {
		struct strview haystack_view = dstr_substring_view(*haystackp, 0, end);
		size_t pos = strview_rfind(haystack_view, needle_view, STRVIEW_NPOS);
		if (pos == STRVIEW_NPOS) {
			break;
		}
		dstr_replace_view(haystackp, pos, needle_view.length, view);
		end = pos - skip;
		replaced++;
	}
	return replaced;
}

size_t dstr_rfind_replace_dstr(dstr_t *haystackp, const dstr_t needle, const dstr_t dstr, size_t max)
{
	return dstr_rfind_replace_view(haystackp, dstr_view(needle), dstr_view(dstr), max);
}

size_t dstr_rfind_replace_cstr(dstr_t *haystackp, const char *needle_cstr, const char *cstr, size_t max)
{
	return dstr_rfind_replace_view(haystackp, strview_from_cstr(needle_cstr),
				       strview_from_cstr(cstr), max);
}

size_t dstr_find_first_of(const dstr_t dstr, const char *accept, size_t pos)
{
	return strview_find_first_of(dstr_view(dstr), accept, pos);
}

size_t dstr_find_last_of(const dstr_t dstr, const char *accept, size_t pos)
{
	return strview_find_last_of(dstr_view(dstr), accept, pos);
}

size_t dstr_find_first_not_of(const dstr_t dstr, const char *reject, size_t pos)
{
	return strview_find_first_not_of(dstr_view(dstr), reject, pos);
}

size_t dstr_find_last_not_of(const dstr_t dstr, const char *reject, size_t pos)
{
	return strview_find_last_not_of(dstr_view(dstr), reject, pos);
}

bool dstr_startswith_dstr(const dstr_t dstr, const dstr_t prefix)
{
	return strview_startswith(dstr_view(dstr), dstr_view(prefix));
}

bool dstr_startswith_view(const dstr_t dstr, struct strview prefix)
{
	return strview_startswith(dstr_view(dstr), prefix);
}

bool dstr_startswith_cstr(const dstr_t dstr, const char *prefix)
{
	return strview_startswith_cstr(dstr_view(dstr), prefix);
}

bool dstr_endswith_dstr(const dstr_t dstr, const dstr_t suffix)
{
	return strview_endswith(dstr_view(dstr), dstr_view(suffix));
}

bool dstr_endswith_view(const dstr_t dstr, struct strview suffix)
{
	return strview_endswith(dstr_view(dstr), suffix);
}

bool dstr_endswith_cstr(const dstr_t dstr, const char *suffix)
{
	return strview_endswith_cstr(dstr_view(dstr), suffix);
}

struct dstr_list dstr_split(const dstr_t dstr, char c, size_t max)
{
	dstr_t *list = NULL;
	size_t allocated = 0;
	size_t count = 0;
	struct strview view = dstr_view(dstr);
	const char accept[] = { c, '\0' };
	while (count < max) {
		if (count == allocated) {
			allocated = allocated == 0 ? 2 : 2 * allocated;
			list = realloc(list, allocated * sizeof(list[0]));
			if (unlikely(!list)) {
				abort();
			}
		}
		size_t pos = strview_find_first_of(view, accept, 0);
		list[count++] = dstr_from_view(strview_substring(view, 0, pos));
		if (pos == STRVIEW_NPOS) {
			break;
		}
		view = strview_substring(view, pos + 1, -1);
	}
	if (count != allocated) {
		dstr_t *list2 = realloc(list, count * sizeof(list[0]));
		if (list2) {
			list = list2;
		}
	}
	return (struct dstr_list){ .strings = list, .count = count };
}

struct dstr_list dstr_rsplit(const dstr_t dstr, char c, size_t max)
{
	dstr_t *list = NULL;
	size_t allocated = 0;
	size_t count = 0;
	struct strview view = dstr_view(dstr);
	const char accept[] = { c, '\0' };
	while (count < max) {
		if (count == allocated) {
			allocated = allocated == 0 ? 2 : 2 * allocated;
			list = realloc(list, allocated * sizeof(list[0]));
			if (unlikely(!list)) {
				abort();
			}
		}
		_Static_assert(STRVIEW_NPOS + 1 == 0, "pos should be 0 if not found");
		size_t pos = 1 + strview_find_last_of(view, accept, view.length - 1);
		list[count++] = dstr_substring_copy(dstr, pos, view.length - pos);
		if (pos == 0) {
			break;
		}
		view = strview_substring(view, 0, pos - 1);
	}
	if (count != allocated) {
		dstr_t *list2 = realloc(list, count * sizeof(list[0]));
		if (list2) {
			list = list2;
		}
	}
	return (struct dstr_list){ .strings = list, .count = count };
}

struct strview_list dstr_split_views(const dstr_t dstr, char c, size_t max)
{
	return strview_split(dstr_view(dstr), c, max);
}

struct strview_list dstr_rsplit_views(const dstr_t dstr, char c, size_t max)
{
	return strview_rsplit(dstr_view(dstr), c, max);
}

void dstr_list_free(struct dstr_list *list)
{
	for (size_t i = 0; i < list->count; i++) {
		if (list->strings[i]) {
			dstr_free(&list->strings[i]);
		}
	}
	free(list->strings);
	list->strings = NULL;
	list->count = 0;
}

void *_dstr_debug_get_head_ptr(const dstr_t dstr)
{
	if (dstr == dstr_empty_dstr) {
		return NULL;
	}
	return dstr - dstr_header_size(dstr_get_type(dstr));
}
