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
#include <stdlib.h>
#include <string.h>
#include "stringview.h"

static _attr_always_inline struct strview _strview_from_chars(const char *chars, size_t n)
{
	return (struct strview){
		.characters = chars,
		.length = n,
	};
}

struct strview (strview_from_chars)(const char *chars, size_t n)
{
	return _strview_from_chars(chars, n);
}

struct strview strview_from_cstr(const char *cstr)
{
	return _strview_from_chars(cstr, strlen(cstr));
}

char *strview_to_cstr(struct strview view)
{
	char *p = malloc(view.length + 1);
	if (unlikely(!p)) {
		abort();
	}
	memcpy(p, view.characters, view.length);
	p[view.length] = '\0';
	return p;
}

struct strview strview_substring(struct strview view, size_t start, size_t length)
{
	assert(start <= view.length);
	size_t rem = view.length - start;
	if (length > rem) {
		length = rem;
	}
	return (struct strview){
		.characters = view.characters + start,
		.length = length,
	};
}

struct strview strview_narrow(struct strview view, size_t left, size_t right)
{
	if (left > view.length) {
		left = view.length;
	}
	view.characters += left;
	view.length -= left;
	if (right > view.length) {
		right = view.length;
	}
	view.length -= right;
	return view;
}

int strview_compare(struct strview view1, struct strview view2)
{
	size_t n = view1.length < view2.length ? view1.length : view2.length;
	int result = strncmp(view1.characters, view2.characters, n);
	if (result == 0) {
		if (view1.length < view2.length) {
			result = -(int)(unsigned char)view2.characters[n];
		} else if (view1.length > view2.length) {
			result = (int)(unsigned char)view1.characters[n];
		}
	}
	return result;
}

int strview_compare_cstr(struct strview view, const char *cstr)
{
	size_t n = view.length;
	int result = strncmp(view.characters, cstr, n);
	if (result == 0) {
		if (cstr[n] != '\0') {
			result = -(int)(unsigned char)cstr[n];
		}
	}
	return result;
}

bool strview_equal(struct strview view1, struct strview view2)
{
	if (view1.length != view2.length) {
		return false;
	}
	return memcmp(view1.characters, view2.characters, view1.length) == 0;
}

bool strview_equal_cstr(struct strview view, const char *cstr)
{
	return strncmp(view.characters, cstr, view.length) == 0 && cstr[view.length] == '\0';
}

size_t strview_find(struct strview haystack, struct strview needle, size_t pos)
{
	haystack = strview_narrow(haystack, pos, 0);
#if defined(HAVE_MEMMEM)
	const char *found = memmem(haystack.characters, haystack.length, needle.characters, needle.length);
#else
	if (unlikely(needle.length == 0)) {
		return 0;
	}
	if (unlikely(needle.length > haystack.length)) {
		return STRVIEW_NPOS;
	}
	const char *found = NULL;
	const char *p = haystack.characters;
	const char *end = haystack.characters + haystack.length - needle.length;
	for (;;) {
		p = memchr(p, needle.characters[0], end - p + 1);
		if (!p) {
			break;
		}
		if (memcmp(p + 1, needle.characters + 1, needle.length - 1) == 0) {
			found = p;
			break;
		}
		p++;
	}
#endif
	return found ? (size_t)(found - haystack.characters) + pos : STRVIEW_NPOS;
}

size_t strview_find_cstr(struct strview haystack, const char *needle, size_t pos)
{
	return strview_find(haystack, strview_from_cstr(needle), pos);
}

static _attr_always_inline void *_strview_memrchr(const void *s, unsigned char c, size_t n)
{
#if defined(HAVE_MEMRCHR)
	return memrchr(s, c, n);
#else
	if (unlikely(n == 0)) {
		return NULL;
	}
	const unsigned char *p = (const unsigned char *)s + n - 1;
	while (*p != c) {
		if (p == s) {
			return NULL;
		}
		p--;
	}
	return (void *)p;
#endif
}

size_t strview_rfind(struct strview haystack, struct strview needle, size_t pos)
{
	if (unlikely(needle.length == 0)) {
		return haystack.length;
	}
	if (unlikely(needle.length > haystack.length)) {
		return STRVIEW_NPOS;
	}
	if (pos < haystack.length - needle.length) {
		haystack = strview_substring(haystack, 0, pos + needle.length);
	}
	const char *found = NULL;
	const char *start = haystack.characters;
	const char *p = start + haystack.length - needle.length;
	for (;;) {
		p = _strview_memrchr(start, needle.characters[0], p - start + 1);
		if (!p) {
			break;
		}
		if (memcmp(p + 1, needle.characters + 1, needle.length - 1) == 0) {
			found = p;
			break;
		}
		p--;
	}
	return found ? (size_t)(found - haystack.characters) : STRVIEW_NPOS;
}

size_t strview_rfind_cstr(struct strview haystack, const char *needle, size_t pos)
{
	return strview_rfind(haystack, strview_from_cstr(needle), pos);
}

static _attr_always_inline
size_t _strview_find_characters(struct strview view, const unsigned char *chars,
				bool reject, bool reverse, size_t pos)
{
	const char *start = view.characters;
	if (reverse && pos < view.length) {
		view = strview_substring(view, 0, pos + 1);
	} else if (!reverse) {
		view = strview_narrow(view, pos, 0);
	}
	unsigned char accept_table[UCHAR_MAX];
	memset(accept_table, reject, sizeof(accept_table));
	for (const unsigned char *c = chars; *c != '\0'; c++) {
		accept_table[*c] = !reject;
	}
	const char *found = NULL;
	for (size_t i = reverse ? view.length : 0;
	     reverse ? i-- > 0 : i < view.length;
	     reverse ? 0 : i++) {
		if (accept_table[(unsigned char)view.characters[i]]) {
			found = &view.characters[i];
			break;
		}
	}
	return found ? (size_t)(found - start) : STRVIEW_NPOS;
}

size_t strview_find_first_of(struct strview view, const char *accept, size_t pos)
{
	if (unlikely(accept[0] == '\0')) {
		return STRVIEW_NPOS;
	}
	if (accept[1] == '\0') {
		view = strview_narrow(view, pos, 0);
		const char *found = memchr(view.characters, accept[0], view.length);
		return found ? (size_t)(found - view.characters) + pos : STRVIEW_NPOS;
	}
	return _strview_find_characters(view, (const unsigned char *)accept, false, false, pos);
}

size_t strview_find_last_of(struct strview view, const char *accept, size_t pos)
{
	if (unlikely(accept[0] == '\0')) {
		return STRVIEW_NPOS;
	}
	if (accept[1] == '\0') {
		if (pos < view.length) {
			view = strview_substring(view, 0, pos + 1);
		}
		const char *found = _strview_memrchr(view.characters, accept[0], view.length);
		return found ? (size_t)(found - view.characters) : STRVIEW_NPOS;
	}
	return _strview_find_characters(view, (const unsigned char *)accept, false, true, pos);
}

size_t strview_find_first_not_of(struct strview view, const char *reject, size_t pos)
{
	// TODO optimized version for single character reject?
	return _strview_find_characters(view, (const unsigned char *)reject, true, false, pos);
}

size_t strview_find_last_not_of(struct strview view, const char *reject, size_t pos)
{
	// TODO optimized version for single character reject?
	return _strview_find_characters(view, (const unsigned char *)reject, true, true, pos);
}

bool strview_startswith(struct strview view, struct strview prefix)
{
	if (prefix.length > view.length) {
		return false;
	}
	return memcmp(view.characters, prefix.characters, prefix.length) == 0;
}

bool strview_startswith_cstr(struct strview view, const char *prefix)
{
	// TODO which implementation is faster?
#ifdef HAVE_STRNLEN
	// if the prefix is longer than the view we always return false, so use strnlen to limit the number
	// of bytes we touch in the prefix
	// (if the prefix is *much* longer than the view than using strlen would really hurt performance)
	struct strview prefix_view = _strview_from_chars(prefix, strnlen(prefix, view.length + 1));
	return strview_startswith(view, prefix_view);
#else
	// theoretically this should be faster if the compiler can vectorize this loop
	size_t i;
	for (i = 0; i < view.length; i++) {
		if (prefix[i] == '\0') {
			return true;
		}
		if (prefix[i] != view.characters[i]) {
			break;
		}
	}
	return prefix[i] == '\0';
#endif
}

bool strview_endswith(struct strview view, struct strview suffix)
{
	if (suffix.length > view.length) {
		return false;
	}
	return memcmp(view.characters + (view.length - suffix.length), suffix.characters, suffix.length) == 0;
}

bool strview_endswith_cstr(struct strview view, const char *suffix)
{
#ifdef HAVE_STRNLEN
	// see comment in strview_startswith_cstr
	struct strview suffix_view = _strview_from_chars(suffix, strnlen(suffix, view.length + 1));
#else
	struct strview suffix_view = strview_from_cstr(suffix);
#endif
	return strview_endswith(view, suffix_view);
}

static struct strview _strview_strip(struct strview view, const char *strip, bool left, bool right)
{
	if (left) {
		size_t pos = strview_find_first_not_of(view, strip, 0);
		if (pos == STRVIEW_NPOS) {
			return strview_from_cstr("");
		}
		view.characters += pos;
		view.length -= pos;
	}
	if (right) {
		size_t pos = strview_find_last_not_of(view, strip, STRVIEW_NPOS);
		if (pos == STRVIEW_NPOS) {
			return strview_from_cstr("");
		}
		view.length = pos + 1;
	}
	return view;
}

struct strview strview_strip(struct strview view, const char *strip)
{
	return _strview_strip(view, strip, true, true);
}

struct strview strview_lstrip(struct strview view, const char *strip)
{
	return _strview_strip(view, strip, true, false);
}

struct strview strview_rstrip(struct strview view, const char *strip)
{
	return _strview_strip(view, strip, false, true);
}

struct strview_list strview_split(struct strview view, char c, size_t max)
{
	struct strview *list = NULL;
	size_t allocated = 0;
	size_t start = 0;
	size_t count = 0;
	while (count < max) {
		if (count == allocated) {
			allocated = allocated == 0 ? 2 : 2 * allocated;
			list = realloc(list, allocated * sizeof(list[0]));
			if (unlikely(!list)) {
				abort();
			}
		}
		const char *p = memchr(&view.characters[start], (unsigned char)c, view.length - start);
		size_t pos = p ? (size_t)(p - view.characters) : view.length;
		list[count++] = strview_substring(view, start, pos - start);
		if (!p) {
			break;
		}
		start = pos + 1;
	}
	if (count != allocated) {
		struct strview *list2 = realloc(list, count * sizeof(list[0]));
		if (list2) {
			list = list2;
		}
	}
	return (struct strview_list){ .strings = list, .count = count };
}

struct strview_list strview_rsplit(struct strview view, char c, size_t max)
{
	struct strview *list = NULL;
	size_t allocated = 0;
	size_t end = view.length;
	size_t count = 0;
	while (count < max) {
		if (count == allocated) {
			allocated = allocated == 0 ? 2 : 2 * allocated;
			list = realloc(list, allocated * sizeof(list[0]));
			if (unlikely(!list)) {
				abort();
			}
		}
		const char *p = _strview_memrchr(view.characters, c, end);
		size_t pos = p ? (size_t)(p - view.characters) + 1 : 0;
		list[count++] = strview_substring(view, pos, end - pos);
		if (pos == 0) {
			break;
		}
		end = pos - 1;
	}
	if (count != allocated) {
		struct strview *list2 = realloc(list, count * sizeof(list[0]));
		if (list2) {
			list = list2;
		}
	}
	return (struct strview_list){ .strings = list, .count = count };
}

void strview_list_free(struct strview_list *list)
{
	free(list->strings);
	list->strings = NULL;
	list->count = 0;
}
