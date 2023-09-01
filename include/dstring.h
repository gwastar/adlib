/*
 * Copyright (C) 2020-2022 Fabian Hügel
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

#ifndef __DSTRING_INCLUDE__
#define __DSTRING_INCLUDE__

// TODO documentation
// TODO dstr_append/insert/replace_number..., join/concat/append with varargs, to_upper/lower?, casecmp?
// TODO add 'restrict' in the appropriate places?
//      (since currently many dstr functions implicitly disallow aliased arguments)
// TODO harden some functions with builtin_object_size?

#include <stdarg.h> // va_list
#include <stdbool.h> // bool
#include <stddef.h> // size_t
#include "compiler.h"
#include "fortify.h"

#define STRVIEW_NPOS ((size_t)-1)
#define DSTR_NPOS    STRVIEW_NPOS

// would like to make this an (un)signed char * so that _Generic can differentiate between char * and dstr_t
// but then gcc gives pointer signedness warnings (-Wpointer-sign) even though the signedness is the same...
typedef char * dstr_t;

// TODO a strview is theoretically allowed to contain null bytes and the strview functions were implemented
// accordingly, but this functionality is currently untested
struct strview {
	const char *characters;
	size_t length;
};

struct dstr_list {
	dstr_t *strings;
	size_t count;
};

struct strview_list {
	struct strview *strings;
	size_t count;
};

__AD_LINKAGE dstr_t dstr_new(void) _attr_unused _attr_nodiscard;
__AD_LINKAGE dstr_t dstr_with_capacity(size_t capacity) _attr_unused _attr_nodiscard;
__AD_LINKAGE dstr_t dstr_from_chars(const char *chars, size_t n) _attr_unused _attr_nodiscard;
__AD_LINKAGE dstr_t dstr_from_cstr(const char *cstr) _attr_unused _attr_nodiscard;
__AD_LINKAGE dstr_t dstr_from_view(struct strview view) _attr_unused _attr_nodiscard;
__AD_LINKAGE dstr_t dstr_from_fmt(const char *fmt, ...) _attr_unused _attr_nodiscard _attr_format_printf(1, 2);
__AD_LINKAGE dstr_t dstr_from_fmtv(const char *fmt, va_list args) _attr_unused _attr_nodiscard _attr_format_printf(1, 0);
__AD_LINKAGE size_t dstr_length(const dstr_t dstr) _attr_unused _attr_pure;
__AD_LINKAGE bool dstr_is_empty(const dstr_t dstr) _attr_unused _attr_pure;
__AD_LINKAGE size_t dstr_capacity(const dstr_t dstr) _attr_unused _attr_pure;
__AD_LINKAGE void dstr_resize(dstr_t *dstrp, size_t new_capacity) _attr_unused;
__AD_LINKAGE void dstr_free(dstr_t *dstrp) _attr_unused;
__AD_LINKAGE void dstr_reserve(dstr_t *dstrp, size_t n) _attr_unused;
__AD_LINKAGE void dstr_clear(dstr_t *dstrp) _attr_unused;
__AD_LINKAGE void dstr_shrink_to_fit(dstr_t *dstrp) _attr_unused;
__AD_LINKAGE dstr_t dstr_copy(const dstr_t dstr) _attr_unused _attr_nodiscard;
__AD_LINKAGE void dstr_append_char(dstr_t *dstrp, char c) _attr_unused;
__AD_LINKAGE void dstr_append_chars(dstr_t *dstrp, const char *chars, size_t n) _attr_unused;
__AD_LINKAGE void dstr_append_dstr(dstr_t *dstrp, const dstr_t dstr) _attr_unused;
__AD_LINKAGE void dstr_append_cstr(dstr_t *dstrp, const char *cstr) _attr_unused;
__AD_LINKAGE void dstr_append_view(dstr_t *dstrp, struct strview view) _attr_unused;
__AD_LINKAGE size_t dstr_append_fmt(dstr_t *dstrp, const char *fmt, ...) _attr_unused _attr_format_printf(2, 3);
__AD_LINKAGE size_t dstr_append_fmtv(dstr_t *dstrp, const char *fmt, va_list args) _attr_unused _attr_format_printf(2, 0);
__AD_LINKAGE char *dstr_append_uninitialized(dstr_t *dstrp, size_t uninit_len) _attr_unused;
__AD_LINKAGE void dstr_insert_char(dstr_t *dstrp, size_t pos, char c) _attr_unused;
__AD_LINKAGE void dstr_insert_chars(dstr_t *dstrp, size_t pos, const char *chars, size_t n) _attr_unused;
__AD_LINKAGE void dstr_insert_dstr(dstr_t *dstrp, size_t pos, const dstr_t dstr) _attr_unused;
__AD_LINKAGE void dstr_insert_cstr(dstr_t *dstrp, size_t pos, const char *cstr) _attr_unused;
__AD_LINKAGE void dstr_insert_view(dstr_t *dstrp, size_t pos, struct strview view) _attr_unused;
__AD_LINKAGE size_t dstr_insert_fmt(dstr_t *dstrp, size_t pos, const char *fmt, ...) _attr_unused _attr_format_printf(3, 4);
__AD_LINKAGE size_t dstr_insert_fmtv(dstr_t *dstrp, size_t pos, const char *fmt, va_list args) _attr_unused _attr_format_printf(3, 0);
__AD_LINKAGE char *dstr_insert_uninitialized(dstr_t *dstrp, size_t pos, size_t uninit_len) _attr_unused;
__AD_LINKAGE void dstr_replace_chars(dstr_t *dstrp, size_t pos, size_t len, const char *chars, size_t n) _attr_unused;
__AD_LINKAGE void dstr_replace_dstr(dstr_t *dstrp, size_t pos, size_t len, const dstr_t dstr) _attr_unused;
__AD_LINKAGE void dstr_replace_cstr(dstr_t *dstrp, size_t pos, size_t len, const char *cstr) _attr_unused;
__AD_LINKAGE void dstr_replace_view(dstr_t *dstrp, size_t pos, size_t len, struct strview view) _attr_unused;
__AD_LINKAGE size_t dstr_replace_fmt(dstr_t *dstrp, size_t pos, size_t len, const char *fmt, ...) _attr_unused _attr_format_printf(4, 5);
__AD_LINKAGE size_t dstr_replace_fmtv(dstr_t *dstrp, size_t pos, size_t len, const char *fmt, va_list args) _attr_unused _attr_format_printf(4, 0);
__AD_LINKAGE char *dstr_replace_uninitialized(dstr_t *dstrp, size_t pos, size_t len, size_t uninit_len) _attr_unused;
__AD_LINKAGE void dstr_erase(dstr_t *dstrp, size_t pos, size_t len) _attr_unused;
__AD_LINKAGE void dstr_strip(dstr_t *dstrp, const char *strip) _attr_unused;
__AD_LINKAGE void dstr_lstrip(dstr_t *dstrp, const char *strip) _attr_unused;
__AD_LINKAGE void dstr_rstrip(dstr_t *dstrp, const char *strip) _attr_unused;
__AD_LINKAGE char *dstr_to_cstr(dstr_t *dstrp) _attr_unused _attr_nodiscard;
__AD_LINKAGE char *dstr_to_cstr_copy(const dstr_t dstr) _attr_unused _attr_nodiscard;
__AD_LINKAGE struct strview dstr_view(const dstr_t dstr) _attr_unused _attr_pure;
__AD_LINKAGE struct strview dstr_substring_view(const dstr_t dstr, size_t start, size_t length) _attr_unused _attr_pure;
__AD_LINKAGE void dstr_substring(dstr_t *dstrp, size_t start, size_t length) _attr_unused;
__AD_LINKAGE dstr_t dstr_substring_copy(const dstr_t dstr, size_t start, size_t length) _attr_unused _attr_nodiscard;
__AD_LINKAGE int dstr_compare_dstr(const dstr_t dstr1, const dstr_t dstr2) _attr_unused _attr_pure;
__AD_LINKAGE int dstr_compare_view(const dstr_t dstr, struct strview view) _attr_unused _attr_pure;
__AD_LINKAGE int dstr_compare_cstr(const dstr_t dstr, const char *cstr) _attr_unused _attr_pure;
// TODO rename equals
__AD_LINKAGE bool dstr_equal_dstr(const dstr_t dstr1, const dstr_t dstr2) _attr_unused _attr_pure;
__AD_LINKAGE bool dstr_equal_view(const dstr_t dstr, struct strview view) _attr_unused _attr_pure;
__AD_LINKAGE bool dstr_equal_cstr(const dstr_t dstr, const char *cstr) _attr_unused _attr_pure;
__AD_LINKAGE size_t dstr_find_dstr(const dstr_t haystack, const dstr_t needle, size_t pos) _attr_unused _attr_pure;
__AD_LINKAGE size_t dstr_find_view(const dstr_t haystack, struct strview needle_view, size_t pos) _attr_unused _attr_pure;
__AD_LINKAGE size_t dstr_find_cstr(const dstr_t haystack, const char *needle_cstr, size_t pos) _attr_unused _attr_pure;
__AD_LINKAGE size_t dstr_rfind_dstr(const dstr_t haystack, const dstr_t needle, size_t pos) _attr_unused _attr_pure;
__AD_LINKAGE size_t dstr_rfind_view(const dstr_t haystack, struct strview needle_view, size_t pos) _attr_unused _attr_pure;
__AD_LINKAGE size_t dstr_rfind_cstr(const dstr_t haystack, const char *needle_cstr, size_t pos) _attr_unused _attr_pure;
// TODO these should have a pos argument
__AD_LINKAGE size_t dstr_find_replace_dstr(dstr_t *haystackp, const dstr_t needle, const dstr_t dstr, size_t max) _attr_unused;
__AD_LINKAGE size_t dstr_find_replace_view(dstr_t *haystackp, struct strview needle_view, struct strview view, size_t max) _attr_unused;
__AD_LINKAGE size_t dstr_find_replace_cstr(dstr_t *haystackp, const char *needle_cstr, const char *cstr, size_t max) _attr_unused;
__AD_LINKAGE size_t dstr_rfind_replace_dstr(dstr_t *haystackp, const dstr_t needle, const dstr_t dstr, size_t max) _attr_unused;
__AD_LINKAGE size_t dstr_rfind_replace_view(dstr_t *haystackp, struct strview needle_view, struct strview view, size_t max) _attr_unused;
__AD_LINKAGE size_t dstr_rfind_replace_cstr(dstr_t *haystackp, const char *needle_cstr, const char *cstr, size_t max) _attr_unused;
// TODO should the accept/reject characters be a strview?
__AD_LINKAGE size_t dstr_find_first_of(const dstr_t dstr, const char *accept, size_t pos) _attr_unused;
__AD_LINKAGE size_t dstr_find_last_of(const dstr_t dstr, const char *accept, size_t pos) _attr_unused;
__AD_LINKAGE size_t dstr_find_first_not_of(const dstr_t dstr, const char *reject, size_t pos) _attr_unused;
__AD_LINKAGE size_t dstr_find_last_not_of(const dstr_t dstr, const char *reject, size_t pos) _attr_unused;
__AD_LINKAGE bool dstr_startswith_dstr(const dstr_t dstr, const dstr_t prefix) _attr_unused _attr_pure;
__AD_LINKAGE bool dstr_startswith_view(const dstr_t dstr, struct strview prefix) _attr_unused _attr_pure;
__AD_LINKAGE bool dstr_startswith_cstr(const dstr_t dstr, const char *prefix) _attr_unused _attr_pure;
__AD_LINKAGE bool dstr_endswith_dstr(const dstr_t dstr, const dstr_t suffix) _attr_unused _attr_pure;
__AD_LINKAGE bool dstr_endswith_view(const dstr_t dstr, struct strview suffix) _attr_unused _attr_pure;
__AD_LINKAGE bool dstr_endswith_cstr(const dstr_t dstr, const char *suffix) _attr_unused _attr_pure;
// TODO should these take multiple delimiters?
__AD_LINKAGE struct dstr_list dstr_split(const dstr_t dstr, char c, size_t max) _attr_unused _attr_nodiscard;
__AD_LINKAGE struct dstr_list dstr_rsplit(const dstr_t dstr, char c, size_t max) _attr_unused _attr_nodiscard;
__AD_LINKAGE struct strview_list dstr_split_views(const dstr_t dstr, char c, size_t max) _attr_unused _attr_nodiscard;
__AD_LINKAGE struct strview_list dstr_rsplit_views(const dstr_t dstr, char c, size_t max) _attr_unused _attr_nodiscard;
__AD_LINKAGE void dstr_list_free(struct dstr_list *list) _attr_unused;


__AD_LINKAGE struct strview strview_from_chars(const char *chars, size_t n) _attr_unused _attr_const;
__AD_LINKAGE struct strview strview_from_cstr(const char *cstr) _attr_unused _attr_pure;
__AD_LINKAGE char *strview_to_cstr(struct strview view) _attr_unused _attr_nodiscard;
__AD_LINKAGE struct strview strview_substring(struct strview view, size_t start, size_t length) _attr_unused _attr_pure;
__AD_LINKAGE struct strview strview_narrow(struct strview view, size_t left, size_t right) _attr_unused _attr_const;
__AD_LINKAGE int strview_compare(struct strview view1, struct strview view2) _attr_unused _attr_pure;
__AD_LINKAGE int strview_compare_cstr(struct strview view, const char *cstr) _attr_unused _attr_pure;
__AD_LINKAGE bool strview_equal(struct strview view1, struct strview view2) _attr_unused _attr_pure;
__AD_LINKAGE bool strview_equal_cstr(struct strview view, const char *cstr) _attr_unused _attr_pure;
__AD_LINKAGE size_t strview_find(struct strview haystack, struct strview needle, size_t pos) _attr_unused _attr_pure;
__AD_LINKAGE size_t strview_find_cstr(struct strview haystack, const char *needle, size_t pos) _attr_unused _attr_pure;
__AD_LINKAGE size_t strview_rfind(struct strview haystack, struct strview needle, size_t pos) _attr_unused _attr_pure;
__AD_LINKAGE size_t strview_rfind_cstr(struct strview haystack, const char *needle, size_t pos) _attr_unused _attr_pure;
__AD_LINKAGE size_t strview_find_first_of(struct strview view, const char *accept, size_t pos) _attr_unused;
__AD_LINKAGE size_t strview_find_last_of(struct strview view, const char *accept, size_t pos) _attr_unused;
__AD_LINKAGE size_t strview_find_first_not_of(struct strview view, const char *reject, size_t pos) _attr_unused;
__AD_LINKAGE size_t strview_find_last_not_of(struct strview view, const char *reject, size_t pos) _attr_unused;
__AD_LINKAGE struct strview strview_strip(struct strview view, const char *strip) _attr_unused;
__AD_LINKAGE struct strview strview_lstrip(struct strview view, const char *strip) _attr_unused;
__AD_LINKAGE struct strview strview_rstrip(struct strview view, const char *strip) _attr_unused;
__AD_LINKAGE bool strview_startswith(struct strview view, struct strview prefix) _attr_unused _attr_pure;
__AD_LINKAGE bool strview_startswith_cstr(struct strview view, const char *prefix) _attr_unused _attr_pure;
__AD_LINKAGE bool strview_endswith(struct strview view, struct strview suffix) _attr_unused _attr_pure;
__AD_LINKAGE bool strview_endswith_cstr(struct strview view, const char *suffix) _attr_unused _attr_pure;
__AD_LINKAGE struct strview_list strview_split(struct strview view, char c, size_t max) _attr_unused _attr_nodiscard;
__AD_LINKAGE struct strview_list strview_rsplit(struct strview view, char c, size_t max) _attr_unused _attr_nodiscard;
__AD_LINKAGE void strview_list_free(struct strview_list *list) _attr_unused;


__AD_LINKAGE void *_dstr_debug_get_head_ptr(const dstr_t dstr) _attr_unused _attr_pure;

#ifdef __FORTIFY_ENABLED

static _attr_always_inline _attr_unused dstr_t _dstr_from_chars_fortified(const char *chars, size_t n)
{
	_fortify_check(_fortify_bos(chars) >= n);
	return dstr_from_chars(chars, n);
}
#define dstr_from_chars(chars, n) _dstr_from_chars_fortified(chars, n)

static _attr_always_inline _attr_unused void _dstr_append_chars_fortified(dstr_t *dstrp, const char *chars,
									  size_t n)
{
	_fortify_check(_fortify_bos(chars) >= n);
	dstr_append_chars(dstrp, chars, n);
}
#define dstr_append_chars(dstrp, chars, n) _dstr_append_chars_fortified(dstrp, chars, n)

static _attr_always_inline _attr_unused void _dstr_insert_chars_fortified(dstr_t *dstrp, size_t pos,
									  const char *chars, size_t n)
{
	_fortify_check(_fortify_bos(chars) >= n);
	dstr_insert_chars(dstrp, pos, chars, n);
}
#define dstr_insert_chars(dstrp, pos, chars, n) _dstr_insert_chars_fortified(dstrp, pos, chars, n)

static _attr_always_inline _attr_unused void _dstr_replace_chars_fortified(dstr_t *dstrp, size_t pos,
									   size_t len, const char *chars,
									   size_t n)
{
	_fortify_check(_fortify_bos(chars) >= n);
	dstr_replace_chars(dstrp, pos, len, chars, n);
}
#define dstr_replace_chars(dstrp, pos, len, chars, n) _dstr_replace_chars_fortified(dstrp, pos, len, chars, n)

static _attr_always_inline _attr_unused struct strview _strview_from_chars_fortified(const char *chars,
										     size_t n)
{
	_fortify_check(_fortify_bos(chars) >= n);
	return strview_from_chars(chars, n);
}
#define strview_from_chars(chars, n) _strview_from_chars_fortified(chars, n)

#endif

#endif
