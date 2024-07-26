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

// TODO documentation
// TODO fortify more functions

#include <stdbool.h> // bool
#include <stddef.h> // size_t
#include "compiler.h"
#include "fortify.h"

#define STRVIEW_NPOS ((size_t)-1)

// TODO a strview is theoretically allowed to contain null bytes and the strview functions were implemented
// accordingly, but this functionality is currently untested
struct strview {
	const char *characters;
	size_t length;
};

struct strview_list {
	struct strview *strings;
	size_t count;
};

struct strview strview_from_chars(const char *chars, size_t n) _attr_const;
struct strview strview_from_cstr(const char *cstr) _attr_pure;
char *strview_to_cstr(struct strview view) _attr_nodiscard;
struct strview strview_substring(struct strview view, size_t start, size_t length) _attr_pure;
struct strview strview_narrow(struct strview view, size_t left, size_t right) _attr_const;
int strview_compare(struct strview view1, struct strview view2) _attr_pure;
int strview_compare_cstr(struct strview view, const char *cstr) _attr_pure;
bool strview_equal(struct strview view1, struct strview view2) _attr_pure;
bool strview_equal_cstr(struct strview view, const char *cstr) _attr_pure;
size_t strview_find(struct strview haystack, struct strview needle, size_t pos) _attr_pure;
size_t strview_find_cstr(struct strview haystack, const char *needle, size_t pos) _attr_pure;
size_t strview_rfind(struct strview haystack, struct strview needle, size_t pos) _attr_pure;
size_t strview_rfind_cstr(struct strview haystack, const char *needle, size_t pos) _attr_pure;
size_t strview_find_first_of(struct strview view, const char *accept, size_t pos);
size_t strview_find_last_of(struct strview view, const char *accept, size_t pos);
size_t strview_find_first_not_of(struct strview view, const char *reject, size_t pos);
size_t strview_find_last_not_of(struct strview view, const char *reject, size_t pos);
struct strview strview_strip(struct strview view, const char *strip);
struct strview strview_lstrip(struct strview view, const char *strip);
struct strview strview_rstrip(struct strview view, const char *strip);
bool strview_startswith(struct strview view, struct strview prefix) _attr_pure;
bool strview_startswith_cstr(struct strview view, const char *prefix) _attr_pure;
bool strview_endswith(struct strview view, struct strview suffix) _attr_pure;
bool strview_endswith_cstr(struct strview view, const char *suffix) _attr_pure;
struct strview_list strview_split(struct strview view, char c, size_t max) _attr_nodiscard;
struct strview_list strview_rsplit(struct strview view, char c, size_t max) _attr_nodiscard;
void strview_list_free(struct strview_list *list);

#ifdef __FORTIFY_ENABLED

static _attr_always_inline struct strview _strview_from_chars_fortified(const char *chars, size_t n)
{
	_fortify_check(_fortify_bos(chars) >= n);
	return strview_from_chars(chars, n);
}
#define strview_from_chars(chars, n) _strview_from_chars_fortified(chars, n)

#endif
