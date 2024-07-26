#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "dstring.h"
#include "testing.h"

#ifdef HAVE_MALLOC_USABLE_SIZE
#include <malloc.h>
#endif

static bool sanity_check(const dstr_t dstr)
{
	CHECK(dstr_length(dstr) <= dstr_capacity(dstr));
	CHECK(strlen(dstr) == dstr_length(dstr));
#ifdef HAVE_MALLOC_USABLE_SIZE
	CHECK(malloc_usable_size(_dstr_debug_get_head_ptr(dstr)) >= dstr_capacity(dstr));
#endif
	return true;
}

static int sign(int x)
{
	return x < 0 ? -1 : (x > 0 ? 1 : 0);
}

static const char abc[] = "abcdefghijklmnopqrstuvwxyz";
static const char a256[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789`~!@#$%^&*()-=_+[]\\;',./{}|:\"<>?abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789`~!@#$%^&*()-=_+[]\\;',./{}|:\"<>?abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789`~!@#$";
#define STRLEN(s) (sizeof(s) - 1)
static const size_t abc_len = STRLEN(abc);
static const size_t a256_len = STRLEN(a256);
_Static_assert(STRLEN(abc) == 26, "");
_Static_assert(STRLEN(a256) == 256, "");
#undef STRLEN

SIMPLE_TEST(dstring)
{
	dstr_t dstr = dstr_from_cstr("abc");
	CHECK(sanity_check(dstr));
	dstr_append_cstr(&dstr, "def");
	CHECK(sanity_check(dstr));
	dstr_append_cstr(&dstr, "ghi");
	CHECK(sanity_check(dstr));
	dstr_append_cstr(&dstr, "jkl");
	CHECK(sanity_check(dstr));
	dstr_append_cstr(&dstr, "mno");
	CHECK(sanity_check(dstr));
	dstr_append_cstr(&dstr, "pqr");
	CHECK(sanity_check(dstr));
	dstr_append_cstr(&dstr, "stu");
	CHECK(sanity_check(dstr));
	dstr_append_cstr(&dstr, "vwx");
	CHECK(sanity_check(dstr));
	dstr_append_cstr(&dstr, "yz");
	CHECK(sanity_check(dstr));
	CHECK(dstr_equals_cstr(dstr, abc));
	dstr_free(&dstr);
	CHECK(!dstr);

	dstr = dstr_new();
	for (size_t n = 0; n < 100; n++) {
		for (size_t i = 0; i < 26; i++) {
			char c = 'a' + i;
			dstr_append_char(&dstr, c);
		}
		CHECK(sanity_check(dstr));
	}

	for (size_t n = 0; n < 100; n++) {
		dstr_t d = dstr_substring_copy(dstr, n * 26, 26);
		CHECK(sanity_check(d));
		CHECK(dstr_equals_cstr(d, abc));
		dstr_free(&d);
	}

	CHECK(dstr_find_cstr(dstr, "abc", 0) == 0);
	CHECK(dstr_find_cstr(dstr, "def", 0) == 3);
	CHECK(dstr_rfind_cstr(dstr, "xyz", DSTR_NPOS) == 2597);
	CHECK(dstr_rfind_cstr(dstr, "uvw", DSTR_NPOS) == 2594);
	dstr_free(&dstr);
	return true;
}

SIMPLE_TEST(dstring_constructors)
{
	dstr_t dstr = dstr_new();
	CHECK(dstr_is_empty(dstr));
	CHECK(sanity_check(dstr));
	dstr_free(&dstr);

	dstr = dstr_with_capacity(0);
	CHECK(dstr_is_empty(dstr));
	CHECK(dstr_capacity(dstr) == 0);
	CHECK(sanity_check(dstr));
	dstr_free(&dstr);

	dstr = dstr_with_capacity(123);
	CHECK(dstr_is_empty(dstr));
	CHECK(dstr_capacity(dstr) >= 123);
	CHECK(sanity_check(dstr));
	dstr_free(&dstr);

	dstr = dstr_from_chars(abc, abc_len);
	CHECK(dstr_length(dstr) == abc_len);
	CHECK(sanity_check(dstr));
	dstr_free(&dstr);

	dstr = dstr_from_chars(a256, a256_len);
	CHECK(dstr_length(dstr) == a256_len);
	CHECK(sanity_check(dstr));
	dstr_free(&dstr);

	dstr = dstr_from_cstr(abc);
	CHECK(dstr_length(dstr) == abc_len);
	CHECK(sanity_check(dstr));
	dstr_free(&dstr);

	dstr = dstr_from_cstr(a256);
	CHECK(dstr_length(dstr) == a256_len);
	CHECK(sanity_check(dstr));
	dstr_free(&dstr);

	dstr = dstr_from_view(strview_from_cstr(abc));
	CHECK(dstr_length(dstr) == abc_len);
	CHECK(sanity_check(dstr));
	dstr_free(&dstr);

	dstr = dstr_from_view(strview_from_cstr(a256));
	CHECK(dstr_length(dstr) == a256_len);
	CHECK(sanity_check(dstr));
	dstr_free(&dstr);

	dstr = dstr_from_fmt("%u", 123);
	CHECK(dstr_length(dstr) == 3);
	CHECK(sanity_check(dstr));
	dstr_free(&dstr);

	dstr = dstr_from_fmt("%s", abc);
	CHECK(dstr_length(dstr) == abc_len);
	CHECK(sanity_check(dstr));
	dstr_free(&dstr);

	dstr = dstr_from_fmt("%s", a256);
	CHECK(dstr_length(dstr) == a256_len);
	CHECK(sanity_check(dstr));
	dstr_free(&dstr);
	return true;
}

SIMPLE_TEST(dstring_resize)
{
	dstr_t dstr = dstr_new();
	dstr_resize(&dstr, 0);
	CHECK(dstr_is_empty(dstr));
	CHECK(sanity_check(dstr));
	dstr_resize(&dstr, 123);
	CHECK(dstr_is_empty(dstr));
	CHECK(dstr_capacity(dstr) >= 123);
	CHECK(sanity_check(dstr));
	dstr_resize(&dstr, 1234);
	CHECK(dstr_is_empty(dstr));
	CHECK(dstr_capacity(dstr) >= 1234);
	CHECK(sanity_check(dstr));
	dstr_resize(&dstr, 0);
	CHECK(dstr_is_empty(dstr));
	CHECK(sanity_check(dstr));
	dstr_append_cstr(&dstr, abc);
	dstr_resize(&dstr, abc_len / 2);
	CHECK(dstr_equals_view(dstr, strview_from_chars(abc, abc_len / 2)));
	CHECK(sanity_check(dstr));
	dstr_clear(&dstr);
	dstr_append_cstr(&dstr, a256);
	dstr_resize(&dstr, a256_len / 2);
	CHECK(dstr_equals_view(dstr, strview_from_chars(a256, a256_len / 2)));
	CHECK(sanity_check(dstr));
	dstr_free(&dstr);

	dstr = dstr_new();
	dstr_append_cstr(&dstr, abc);
	dstr_resize(&dstr, abc_len);
	CHECK(dstr_equals_cstr(dstr, abc));
	CHECK(sanity_check(dstr));
	dstr_resize(&dstr, UINT8_MAX + 1);
	CHECK(dstr_equals_cstr(dstr, abc));
	CHECK(sanity_check(dstr));
	dstr_append_cstr(&dstr, abc);
	CHECK(dstr_startswith_cstr(dstr, abc));
	CHECK(strcmp(dstr + abc_len, abc) == 0);
	CHECK(sanity_check(dstr));
	dstr_resize(&dstr, UINT16_MAX + 1);
	CHECK(dstr_startswith_cstr(dstr, abc));
	CHECK(strcmp(dstr + abc_len, abc) == 0);
	CHECK(sanity_check(dstr));
	dstr_append_cstr(&dstr, abc);
	CHECK(dstr_startswith_cstr(dstr, abc));
	CHECK(memcmp(dstr + abc_len, abc, abc_len) == 0);
	CHECK(strcmp(dstr + 2 * abc_len, abc) == 0);
	CHECK(sanity_check(dstr));
	dstr_resize(&dstr, UINT16_MAX - 1);
	CHECK(dstr_startswith_cstr(dstr, abc));
	CHECK(memcmp(dstr + abc_len, abc, abc_len) == 0);
	CHECK(strcmp(dstr + 2 * abc_len, abc) == 0);
	CHECK(sanity_check(dstr));
	dstr_resize(&dstr, UINT8_MAX - 1);
	CHECK(dstr_startswith_cstr(dstr, abc));
	CHECK(memcmp(dstr + abc_len, abc, abc_len) == 0);
	CHECK(strcmp(dstr + 2 * abc_len, abc) == 0);
	CHECK(sanity_check(dstr));
	dstr_resize(&dstr, UINT16_MAX + 1);
	CHECK(dstr_startswith_cstr(dstr, abc));
	CHECK(memcmp(dstr + abc_len, abc, abc_len) == 0);
	CHECK(strcmp(dstr + 2 * abc_len, abc) == 0);
	CHECK(sanity_check(dstr));
	dstr_resize(&dstr, UINT8_MAX - 1);
	CHECK(dstr_startswith_cstr(dstr, abc));
	CHECK(memcmp(dstr + abc_len, abc, abc_len) == 0);
	CHECK(strcmp(dstr + 2 * abc_len, abc) == 0);
	CHECK(sanity_check(dstr));
	dstr_resize(&dstr, 0);
	CHECK(dstr_is_empty(dstr));
	CHECK(sanity_check(dstr));
	dstr_resize(&dstr, UINT8_MAX + 1);
	CHECK(dstr_is_empty(dstr));
	CHECK(sanity_check(dstr));
	dstr_resize(&dstr, UINT16_MAX + 1);
	CHECK(dstr_is_empty(dstr));
	CHECK(sanity_check(dstr));
	dstr_resize(&dstr, UINT8_MAX + 1);
	CHECK(dstr_is_empty(dstr));
	CHECK(sanity_check(dstr));
	dstr_resize(&dstr, 0);
	CHECK(dstr_is_empty(dstr));
	CHECK(sanity_check(dstr));
	dstr_resize(&dstr, UINT16_MAX + 1);
	CHECK(dstr_is_empty(dstr));
	CHECK(sanity_check(dstr));
	dstr_resize(&dstr, 0);
	CHECK(dstr_is_empty(dstr));
	CHECK(sanity_check(dstr));
	dstr_free(&dstr);
	return true;
}

SIMPLE_TEST(dstring_reserve_shrink)
{
	dstr_t dstr = dstr_new();
	dstr_reserve(&dstr, abc_len);
	CHECK(dstr_capacity(dstr) - dstr_length(dstr) >= abc_len);
	CHECK(sanity_check(dstr));
	dstr_reserve(&dstr, abc_len);
	CHECK(dstr_capacity(dstr) - dstr_length(dstr) >= abc_len);
	CHECK(sanity_check(dstr));
	dstr_append_cstr(&dstr, abc);
	CHECK(sanity_check(dstr));
	dstr_reserve(&dstr, abc_len);
	CHECK(dstr_capacity(dstr) - dstr_length(dstr) >= abc_len);
	CHECK(dstr_capacity(dstr) >= 2 * abc_len);
	CHECK(sanity_check(dstr));
	dstr_shrink_to_fit(&dstr);
	CHECK(dstr_length(dstr) == abc_len);
	CHECK(dstr_capacity(dstr) == abc_len);
	CHECK(sanity_check(dstr));
	dstr_clear(&dstr);
	CHECK(dstr_is_empty(dstr));
	CHECK(dstr_capacity(dstr) == abc_len);
	CHECK(sanity_check(dstr));
	dstr_clear(&dstr);
	CHECK(dstr_is_empty(dstr));
	CHECK(dstr_capacity(dstr) == abc_len);
	CHECK(sanity_check(dstr));
	dstr_shrink_to_fit(&dstr);
	CHECK(dstr_capacity(dstr) == 0);
	CHECK(sanity_check(dstr));
	dstr_clear(&dstr);
	CHECK(dstr_is_empty(dstr));
	CHECK(dstr_capacity(dstr) == 0);
	CHECK(sanity_check(dstr));
	dstr_shrink_to_fit(&dstr);
	CHECK(dstr_capacity(dstr) == 0);
	CHECK(sanity_check(dstr));
	dstr_free(&dstr);
	return true;
}

SIMPLE_TEST(dstring_append)
{
	dstr_t dstr = dstr_new();
	dstr_t dstr2 = dstr_copy(dstr);
	CHECK(dstr_is_empty(dstr2));
	CHECK(sanity_check(dstr2));
	dstr_append_cstr(&dstr2, abc);
	dstr_free(&dstr);
	dstr = dstr_copy(dstr2);
	CHECK(dstr_length(dstr) == abc_len);
	CHECK(strcmp(dstr, abc) == 0);
	CHECK(sanity_check(dstr));
	for (size_t i = 0; i < 99; i++) {
		dstr_append_cstr(&dstr, abc);
	}
	dstr_free(&dstr2);
	dstr2 = dstr_copy(dstr);
	CHECK(dstr_length(dstr2) == 100 * abc_len);
	CHECK(dstr_equals_dstr(dstr, dstr2));
	CHECK(sanity_check(dstr));
	CHECK(sanity_check(dstr2));
	dstr_free(&dstr);
	dstr_free(&dstr2);

	dstr = dstr_new();
	dstr_append_char(&dstr, 'a');
	CHECK(strcmp(dstr, "a") == 0);
	CHECK(sanity_check(dstr));
	dstr_append_char(&dstr, 'a');
	CHECK(strcmp(dstr, "aa") == 0);
	CHECK(sanity_check(dstr));
	dstr_clear(&dstr);
	for (size_t i = 0; i < 256; i++) {
		dstr_append_char(&dstr, 'a');
	}
	for (size_t i = 0; i < 256; i++) {
		CHECK(dstr[i] == 'a');
	}
	CHECK(sanity_check(dstr));
	for (size_t i = 0; i < 256; i++) {
		dstr_append_char(&dstr, 'b');
	}
	CHECK(dstr_length(dstr) == 512);
	for (size_t i = 0; i < 256; i++) {
		CHECK(dstr[i] == 'a');
	}
	for (size_t i = 0; i < 256; i++) {
		CHECK(dstr[256 + i] == 'b');
	}
	CHECK(sanity_check(dstr));
	dstr_free(&dstr);

	dstr = dstr_new();
	dstr_append_chars(&dstr, abc, abc_len);
	CHECK(dstr_length(dstr) == abc_len);
	CHECK(sanity_check(dstr));
	dstr_append_chars(&dstr, a256, a256_len);
	CHECK(dstr_length(dstr) == abc_len + a256_len);
	CHECK(sanity_check(dstr));
	CHECK(strncmp(dstr, abc, abc_len) == 0);
	CHECK(strcmp(dstr + abc_len, a256) == 0);
	dstr_free(&dstr);

	dstr = dstr_new();
	dstr_append_cstr(&dstr, abc);
	CHECK(sanity_check(dstr));
	CHECK(dstr_equals_cstr(dstr, abc));
	dstr_append_cstr(&dstr, a256);
	CHECK(sanity_check(dstr));
	CHECK(strncmp(dstr, abc, abc_len) == 0);
	CHECK(strcmp(dstr + abc_len, a256) == 0);
	dstr_free(&dstr);

	dstr = dstr_new();
	dstr_append_view(&dstr, strview_from_cstr(abc));
	CHECK(sanity_check(dstr));
	CHECK(dstr_equals_cstr(dstr, abc));
	dstr_append_view(&dstr, strview_from_cstr(a256));
	CHECK(sanity_check(dstr));
	CHECK(strncmp(dstr, abc, abc_len) == 0);
	CHECK(strcmp(dstr + abc_len, a256) == 0);
	dstr_free(&dstr);

	dstr = dstr_new();
	dstr_append_fmt(&dstr, "%s", abc);
	CHECK(sanity_check(dstr));
	CHECK(dstr_equals_cstr(dstr, abc));
	dstr_append_fmt(&dstr, "%s", a256);
	CHECK(sanity_check(dstr));
	CHECK(strncmp(dstr, abc, abc_len) == 0);
	CHECK(strcmp(dstr + abc_len, a256) == 0);
	dstr_free(&dstr);

	dstr = dstr_new();
	dstr2 = dstr_from_cstr(abc);
	dstr_append_dstr(&dstr, dstr2);
	dstr_free(&dstr2);
	CHECK(sanity_check(dstr));
	CHECK(dstr_equals_cstr(dstr, abc));
	dstr2 = dstr_from_cstr(a256);
	dstr_append_dstr(&dstr, dstr2);
	dstr_free(&dstr2);
	CHECK(sanity_check(dstr));
	CHECK(strncmp(dstr, abc, abc_len) == 0);
	CHECK(strcmp(dstr + abc_len, a256) == 0);
	dstr_free(&dstr);

	dstr = dstr_from_cstr(a256);
	CHECK(sanity_check(dstr));
	dstr_append_cstr(&dstr, abc);
	CHECK(sanity_check(dstr));
	CHECK(strncmp(dstr, a256, a256_len) == 0);
	CHECK(strcmp(dstr + a256_len, abc) == 0);
	dstr_free(&dstr);

	dstr = dstr_from_view(strview_from_cstr(a256));
	CHECK(sanity_check(dstr));
	dstr_append_view(&dstr, strview_from_cstr(abc));
	CHECK(sanity_check(dstr));
	CHECK(strncmp(dstr, a256, a256_len) == 0);
	CHECK(strcmp(dstr + a256_len, abc) == 0);
	dstr_free(&dstr);

	dstr = dstr_from_chars(a256, a256_len);
	CHECK(sanity_check(dstr));
	dstr_append_chars(&dstr, abc, abc_len);
	CHECK(sanity_check(dstr));
	CHECK(strncmp(dstr, a256, a256_len) == 0);
	CHECK(strcmp(dstr + a256_len, abc) == 0);
	dstr_free(&dstr);

	dstr2 = dstr_from_cstr(a256);
	dstr = dstr_copy(dstr2);
	dstr_free(&dstr2);
	CHECK(sanity_check(dstr));
	dstr2 = dstr_from_cstr(abc);
	dstr_append_dstr(&dstr, dstr2);
	dstr_free(&dstr2);
	CHECK(sanity_check(dstr));
	CHECK(strncmp(dstr, a256, a256_len) == 0);
	CHECK(strcmp(dstr + a256_len, abc) == 0);
	dstr_free(&dstr);

	dstr = dstr_from_fmt("%s", a256);
	CHECK(sanity_check(dstr));
	dstr_append_fmt(&dstr, "%s", abc);
	CHECK(sanity_check(dstr));
	CHECK(strncmp(dstr, a256, a256_len) == 0);
	CHECK(strcmp(dstr + a256_len, abc) == 0);
	dstr_free(&dstr);

	dstr = dstr_from_cstr("");
	CHECK(sanity_check(dstr));
	CHECK(dstr_is_empty(dstr));
	dstr_append_cstr(&dstr, "");
	CHECK(dstr_is_empty(dstr));
	CHECK(sanity_check(dstr));
	dstr_append_cstr(&dstr, abc);
	CHECK(dstr_equals_cstr(dstr, abc));
	CHECK(sanity_check(dstr));
	dstr_append_cstr(&dstr, "");
	CHECK(dstr_equals_cstr(dstr, abc));
	CHECK(sanity_check(dstr));
	dstr_append_cstr(&dstr, a256);
	CHECK(strncmp(dstr, abc, abc_len) == 0);
	CHECK(strcmp(dstr + abc_len, a256) == 0);
	CHECK(sanity_check(dstr));
	dstr_append_cstr(&dstr, "");
	CHECK(strncmp(dstr, abc, abc_len) == 0);
	CHECK(strcmp(dstr + abc_len, a256) == 0);
	CHECK(sanity_check(dstr));
	dstr_free(&dstr);

	dstr = dstr_from_view(strview_from_cstr(""));
	CHECK(sanity_check(dstr));
	CHECK(dstr_is_empty(dstr));
	dstr_append_view(&dstr, strview_from_cstr(""));
	CHECK(dstr_is_empty(dstr));
	CHECK(sanity_check(dstr));
	dstr_append_cstr(&dstr, abc);
	CHECK(dstr_equals_cstr(dstr, abc));
	CHECK(sanity_check(dstr));
	dstr_append_view(&dstr, strview_from_cstr(""));
	CHECK(dstr_equals_cstr(dstr, abc));
	CHECK(sanity_check(dstr));
	dstr_append_cstr(&dstr, a256);
	CHECK(strncmp(dstr, abc, abc_len) == 0);
	CHECK(strcmp(dstr + abc_len, a256) == 0);
	CHECK(sanity_check(dstr));
	dstr_append_view(&dstr, strview_from_cstr(""));
	CHECK(strncmp(dstr, abc, abc_len) == 0);
	CHECK(strcmp(dstr + abc_len, a256) == 0);
	CHECK(sanity_check(dstr));
	dstr_free(&dstr);

	dstr2 = dstr_new();
	dstr = dstr_copy(dstr2);
	CHECK(sanity_check(dstr));
	CHECK(dstr_is_empty(dstr));
	dstr_append_dstr(&dstr, dstr2);
	CHECK(dstr_is_empty(dstr));
	CHECK(sanity_check(dstr));
	dstr_append_cstr(&dstr, abc);
	CHECK(dstr_equals_cstr(dstr, abc));
	CHECK(sanity_check(dstr));
	dstr_append_dstr(&dstr, dstr2);
	CHECK(dstr_equals_cstr(dstr, abc));
	CHECK(sanity_check(dstr));
	dstr_append_cstr(&dstr, a256);
	CHECK(strncmp(dstr, abc, abc_len) == 0);
	CHECK(strcmp(dstr + abc_len, a256) == 0);
	CHECK(sanity_check(dstr));
	dstr_append_dstr(&dstr, dstr2);
	CHECK(strncmp(dstr, abc, abc_len) == 0);
	CHECK(strcmp(dstr + abc_len, a256) == 0);
	CHECK(sanity_check(dstr));
	dstr_free(&dstr2);
	dstr_free(&dstr);

	dstr = dstr_from_chars("", 0);
	CHECK(sanity_check(dstr));
	CHECK(dstr_is_empty(dstr));
	dstr_append_chars(&dstr, "", 0);
	CHECK(dstr_is_empty(dstr));
	CHECK(sanity_check(dstr));
	dstr_append_cstr(&dstr, abc);
	CHECK(dstr_equals_cstr(dstr, abc));
	CHECK(sanity_check(dstr));
	dstr_append_chars(&dstr, "", 0);
	CHECK(dstr_equals_cstr(dstr, abc));
	CHECK(sanity_check(dstr));
	dstr_append_cstr(&dstr, a256);
	CHECK(strncmp(dstr, abc, abc_len) == 0);
	CHECK(strcmp(dstr + abc_len, a256) == 0);
	CHECK(sanity_check(dstr));
	dstr_append_chars(&dstr, "", 0);
	CHECK(strncmp(dstr, abc, abc_len) == 0);
	CHECK(strcmp(dstr + abc_len, a256) == 0);
	CHECK(sanity_check(dstr));
	dstr_free(&dstr);

	dstr = dstr_from_fmt("%s", "");
	CHECK(sanity_check(dstr));
	CHECK(dstr_is_empty(dstr));
	dstr_append_fmt(&dstr, "%s", "");
	CHECK(dstr_is_empty(dstr));
	CHECK(sanity_check(dstr));
	dstr_append_cstr(&dstr, abc);
	CHECK(dstr_equals_cstr(dstr, abc));
	CHECK(sanity_check(dstr));
	dstr_append_fmt(&dstr, "%s", "");
	CHECK(dstr_equals_cstr(dstr, abc));
	CHECK(sanity_check(dstr));
	dstr_append_cstr(&dstr, a256);
	CHECK(strncmp(dstr, abc, abc_len) == 0);
	CHECK(strcmp(dstr + abc_len, a256) == 0);
	CHECK(sanity_check(dstr));
	dstr_append_fmt(&dstr, "%s", "");
	CHECK(strncmp(dstr, abc, abc_len) == 0);
	CHECK(strcmp(dstr + abc_len, a256) == 0);
	CHECK(sanity_check(dstr));
	dstr_free(&dstr);
	return true;
}

SIMPLE_TEST(dstring_uninitialized)
{
	dstr_t dstr = dstr_new();
	char *m = dstr_append_uninitialized(&dstr, abc_len);
	strcpy(m, abc);
	CHECK(dstr_equals_cstr(dstr, abc));
	CHECK(sanity_check(dstr));
	m = dstr_append_uninitialized(&dstr, a256_len);
	strcpy(m, a256);
	CHECK(memcmp(dstr, abc, abc_len) == 0);
	CHECK(strcmp(dstr + abc_len, a256) == 0);
	CHECK(sanity_check(dstr));
	CHECK(*dstr_append_uninitialized(&dstr, 0) == '\0'); // TODO should this return NULL?
	CHECK(sanity_check(dstr));
	CHECK(*dstr_insert_uninitialized(&dstr, 0, 0) == abc[0]); // TODO should this return NULL?
	CHECK(sanity_check(dstr));
	CHECK(*dstr_insert_uninitialized(&dstr, abc_len, 0) == a256[0]); // TODO should this return NULL?
	CHECK(sanity_check(dstr));
	m = dstr_insert_uninitialized(&dstr, abc_len, abc_len);
	memcpy(m, abc, abc_len);
	CHECK(memcmp(dstr, abc, abc_len) == 0);
	CHECK(memcmp(dstr + abc_len, abc, abc_len) == 0);
	CHECK(strcmp(dstr + 2 * abc_len, a256) == 0);
	CHECK(sanity_check(dstr));
	m = dstr_insert_uninitialized(&dstr, 2 * abc_len, a256_len);
	memcpy(m, a256, a256_len);
	CHECK(memcmp(dstr, abc, abc_len) == 0);
	CHECK(memcmp(dstr + abc_len, abc, abc_len) == 0);
	CHECK(memcmp(dstr + 2 * abc_len, a256, a256_len) == 0);
	CHECK(strcmp(dstr + 2 * abc_len + a256_len, a256) == 0);
	CHECK(sanity_check(dstr));
	m = dstr_replace_uninitialized(&dstr, 2 * abc_len, a256_len, 0);
	CHECK(strcmp(m, a256) == 0);
	CHECK(sanity_check(dstr));
	m = dstr_replace_uninitialized(&dstr, abc_len, abc_len, a256_len);
	memcpy(m, a256, a256_len);
	CHECK(memcmp(dstr, abc, abc_len) == 0);
	CHECK(memcmp(dstr + abc_len, a256, a256_len) == 0);
	CHECK(strcmp(dstr + abc_len + a256_len, a256) == 0);
	CHECK(sanity_check(dstr));
	m = dstr_replace_uninitialized(&dstr, 0, DSTR_NPOS, abc_len);
	memcpy(m, abc, abc_len);
	CHECK(strcmp(dstr, abc) == 0);
	CHECK(sanity_check(dstr));
	dstr_replace_uninitialized(&dstr, 0, DSTR_NPOS, 0);
	CHECK(dstr_is_empty(dstr));
	CHECK(sanity_check(dstr));
	dstr_free(&dstr);
	return true;
}

SIMPLE_TEST(dstring_insert)
{
	dstr_t dstr = dstr_new();
	dstr_insert_cstr(&dstr, 0, "");
	dstr_insert_char(&dstr, 0, 'a');
	CHECK(strcmp(dstr, "a") == 0);
	CHECK(sanity_check(dstr));
	dstr_insert_char(&dstr, 1, 'b');
	CHECK(strcmp(dstr, "ab") == 0);
	CHECK(sanity_check(dstr));
	dstr_insert_char(&dstr, 0, 'c');
	CHECK(strcmp(dstr, "cab") == 0);
	CHECK(sanity_check(dstr));
	dstr_insert_char(&dstr, 1, 'd');
	CHECK(strcmp(dstr, "cdab") == 0);
	CHECK(sanity_check(dstr));
	dstr_insert_char(&dstr, 3, 'e');
	CHECK(strcmp(dstr, "cdaeb") == 0);
	CHECK(sanity_check(dstr));
	dstr_insert_char(&dstr, 2, 'f');
	CHECK(strcmp(dstr, "cdfaeb") == 0);
	CHECK(sanity_check(dstr));
	dstr_insert_chars(&dstr, 0, a256, a256_len);
	CHECK(dstr_endswith_cstr(dstr, "cdfaeb"));
	CHECK(sanity_check(dstr));
	dstr_insert_char(&dstr, a256_len, 'g');
	CHECK(dstr_endswith_cstr(dstr, "gcdfaeb"));
	CHECK(sanity_check(dstr));
	dstr_free(&dstr);

	for (size_t i = 0; i <= abc_len; i++) {
		dstr_t dstr = dstr_new();
		dstr_insert_cstr(&dstr, 0, abc);
		CHECK(sanity_check(dstr));
		dstr_insert_cstr(&dstr, i, a256);
		CHECK(sanity_check(dstr));
		CHECK(strncmp(dstr, abc, i) == 0);
		CHECK(strncmp(dstr + i, a256, a256_len) == 0);
		CHECK(strcmp(dstr + i + a256_len, abc + i) == 0);
		dstr_free(&dstr);

		dstr = dstr_new();
		dstr_insert_view(&dstr, 0, strview_from_cstr(abc));
		CHECK(sanity_check(dstr));
		dstr_insert_view(&dstr, i, strview_from_cstr(a256));
		CHECK(sanity_check(dstr));
		CHECK(strncmp(dstr, abc, i) == 0);
		CHECK(strncmp(dstr + i, a256, a256_len) == 0);
		CHECK(strcmp(dstr + i + a256_len, abc + i) == 0);
		dstr_free(&dstr);

		dstr = dstr_new();
		dstr_insert_chars(&dstr, 0, abc, abc_len);
		CHECK(sanity_check(dstr));
		dstr_insert_chars(&dstr, i, a256, a256_len);
		CHECK(sanity_check(dstr));
		CHECK(strncmp(dstr, abc, i) == 0);
		CHECK(strncmp(dstr + i, a256, a256_len) == 0);
		CHECK(strcmp(dstr + i + a256_len, abc + i) == 0);
		dstr_free(&dstr);

		dstr = dstr_new();
		dstr_t dstr2 = dstr_from_cstr(abc);
		dstr_insert_dstr(&dstr, 0, dstr2);
		dstr_free(&dstr2);
		CHECK(sanity_check(dstr));
		dstr2 = dstr_from_cstr(a256);
		dstr_insert_dstr(&dstr, i, dstr2);
		dstr_free(&dstr2);
		CHECK(sanity_check(dstr));
		CHECK(strncmp(dstr, abc, i) == 0);
		CHECK(strncmp(dstr + i, a256, a256_len) == 0);
		CHECK(strcmp(dstr + i + a256_len, abc + i) == 0);
		dstr_free(&dstr);

		dstr = dstr_new();
		dstr_insert_fmt(&dstr, 0, "%s", abc);
		CHECK(sanity_check(dstr));
		dstr_insert_fmt(&dstr, i, "%s", a256);
		CHECK(sanity_check(dstr));
		CHECK(strncmp(dstr, abc, i) == 0);
		CHECK(strncmp(dstr + i, a256, a256_len) == 0);
		CHECK(strcmp(dstr + i + a256_len, abc + i) == 0);
		dstr_free(&dstr);
	}

	for (size_t i = 0; i <= a256_len; i++) {
		dstr_t dstr = dstr_new();
		dstr_insert_cstr(&dstr, 0, a256);
		CHECK(sanity_check(dstr));
		dstr_insert_cstr(&dstr, i, abc);
		CHECK(sanity_check(dstr));
		CHECK(strncmp(dstr, a256, i) == 0);
		CHECK(strncmp(dstr + i, abc, abc_len) == 0);
		CHECK(strcmp(dstr + i + abc_len, a256 + i) == 0);
		dstr_free(&dstr);

		dstr = dstr_new();
		dstr_insert_view(&dstr, 0, strview_from_cstr(a256));
		CHECK(sanity_check(dstr));
		dstr_insert_view(&dstr, i, strview_from_cstr(abc));
		CHECK(sanity_check(dstr));
		CHECK(strncmp(dstr, a256, i) == 0);
		CHECK(strncmp(dstr + i, abc, abc_len) == 0);
		CHECK(strcmp(dstr + i + abc_len, a256 + i) == 0);
		dstr_free(&dstr);

		dstr = dstr_new();
		dstr_insert_chars(&dstr, 0, a256, a256_len);
		CHECK(sanity_check(dstr));
		dstr_insert_chars(&dstr, i, abc, abc_len);
		CHECK(sanity_check(dstr));
		CHECK(strncmp(dstr, a256, i) == 0);
		CHECK(strncmp(dstr + i, abc, abc_len) == 0);
		CHECK(strcmp(dstr + i + abc_len, a256 + i) == 0);
		dstr_free(&dstr);

		dstr = dstr_new();
		dstr_t dstr2 = dstr_from_cstr(a256);
		dstr_insert_dstr(&dstr, 0, dstr2);
		dstr_free(&dstr2);
		CHECK(sanity_check(dstr));
		dstr2 = dstr_from_cstr(abc);
		dstr_insert_dstr(&dstr, i, dstr2);
		dstr_free(&dstr2);
		CHECK(sanity_check(dstr));
		CHECK(strncmp(dstr, a256, i) == 0);
		CHECK(strncmp(dstr + i, abc, abc_len) == 0);
		CHECK(strcmp(dstr + i + abc_len, a256 + i) == 0);
		dstr_free(&dstr);

		dstr = dstr_new();
		dstr_insert_fmt(&dstr, 0, "%s", a256);
		CHECK(sanity_check(dstr));
		dstr_insert_fmt(&dstr, i, "%s", abc);
		CHECK(sanity_check(dstr));
		CHECK(strncmp(dstr, a256, i) == 0);
		CHECK(strncmp(dstr + i, abc, abc_len) == 0);
		CHECK(strcmp(dstr + i + abc_len, a256 + i) == 0);
		dstr_free(&dstr);
	}

	dstr = dstr_from_cstr(abc);
	for (size_t i = 0; i <= abc_len; i++) {
		dstr_insert_chars(&dstr, i, "", 0);
		dstr_insert_cstr(&dstr, i, "");
		dstr_insert_view(&dstr, i, strview_from_cstr(""));
		dstr_insert_fmt(&dstr, i, "%s", "");
		CHECK(sanity_check(dstr));
		CHECK(dstr_equals_cstr(dstr, abc));
	}
	dstr_free(&dstr);

	dstr = dstr_from_cstr(a256);
	for (size_t i = 0; i <= a256_len; i++) {
		dstr_insert_chars(&dstr, i, "", 0);
		dstr_insert_cstr(&dstr, i, "");
		dstr_insert_view(&dstr, i, strview_from_cstr(""));
		dstr_insert_fmt(&dstr, i, "%s", "");
		CHECK(sanity_check(dstr));
		CHECK(dstr_equals_cstr(dstr, a256));
	}
	dstr_free(&dstr);
	return true;
}

SIMPLE_TEST(dstring_replace)
{
	for (size_t i = 0; i <= abc_len; i++) {
		dstr_t dstr = dstr_new();
		dstr_replace_cstr(&dstr, 0, 0, abc);
		CHECK(sanity_check(dstr));
		dstr_replace_cstr(&dstr, i, DSTR_NPOS, a256);
		CHECK(sanity_check(dstr));
		CHECK(strncmp(dstr, abc, i) == 0);
		CHECK(strcmp(dstr + i, a256) == 0);
		dstr_free(&dstr);
		dstr = dstr_new();
		dstr_replace_cstr(&dstr, 0, 0, abc);
		CHECK(sanity_check(dstr));
		dstr_replace_cstr(&dstr, 0, i, a256);
		CHECK(sanity_check(dstr));
		CHECK(strncmp(dstr, a256, a256_len) == 0);
		CHECK(strcmp(dstr + a256_len, abc + i) == 0);
		dstr_free(&dstr);
		dstr = dstr_new();
		dstr_replace_cstr(&dstr, 0, 0, abc);
		CHECK(sanity_check(dstr));
		dstr_replace_cstr(&dstr, i / 2, abc_len - i, a256);
		CHECK(sanity_check(dstr));
		CHECK(strncmp(dstr, abc, i / 2) == 0);
		CHECK(strncmp(dstr + i / 2, a256, a256_len) == 0);
		CHECK(strcmp(dstr + i / 2 + a256_len, abc + i / 2 + (abc_len - i)) == 0);
		dstr_free(&dstr);

		dstr = dstr_new();
		dstr_replace_view(&dstr, 0, 0, strview_from_cstr(abc));
		CHECK(sanity_check(dstr));
		dstr_replace_view(&dstr, i, DSTR_NPOS, strview_from_cstr(a256));
		CHECK(sanity_check(dstr));
		CHECK(strncmp(dstr, abc, i) == 0);
		CHECK(strcmp(dstr + i, a256) == 0);
		dstr_free(&dstr);
		dstr = dstr_new();
		dstr_replace_view(&dstr, 0, 0, strview_from_cstr(abc));
		CHECK(sanity_check(dstr));
		dstr_replace_view(&dstr, 0, i, strview_from_cstr(a256));
		CHECK(sanity_check(dstr));
		CHECK(strncmp(dstr, a256, a256_len) == 0);
		CHECK(strcmp(dstr + a256_len, abc + i) == 0);
		dstr_free(&dstr);
		dstr = dstr_new();
		dstr_replace_view(&dstr, 0, 0, strview_from_cstr(abc));
		CHECK(sanity_check(dstr));
		dstr_replace_view(&dstr, i / 2, abc_len - i, strview_from_cstr(a256));
		CHECK(sanity_check(dstr));
		CHECK(strncmp(dstr, abc, i / 2) == 0);
		CHECK(strncmp(dstr + i / 2, a256, a256_len) == 0);
		CHECK(strcmp(dstr + i / 2 + a256_len, abc + i / 2 + (abc_len - i)) == 0);
		dstr_free(&dstr);

		dstr = dstr_new();
		dstr_replace_chars(&dstr, 0, 0, abc, abc_len);
		CHECK(sanity_check(dstr));
		dstr_replace_chars(&dstr, i, DSTR_NPOS, a256, a256_len);
		CHECK(sanity_check(dstr));
		CHECK(strncmp(dstr, abc, i) == 0);
		CHECK(strcmp(dstr + i, a256) == 0);
		dstr_free(&dstr);
		dstr = dstr_new();
		dstr_replace_chars(&dstr, 0, 0, abc, abc_len);
		CHECK(sanity_check(dstr));
		dstr_replace_chars(&dstr, 0, i, a256, a256_len);
		CHECK(sanity_check(dstr));
		CHECK(strncmp(dstr, a256, a256_len) == 0);
		CHECK(strcmp(dstr + a256_len, abc + i) == 0);
		dstr_free(&dstr);
		dstr = dstr_new();
		dstr_replace_chars(&dstr, 0, 0, abc, abc_len);
		CHECK(sanity_check(dstr));
		dstr_replace_chars(&dstr, i / 2, abc_len - i, a256, a256_len);
		CHECK(sanity_check(dstr));
		CHECK(strncmp(dstr, abc, i / 2) == 0);
		CHECK(strncmp(dstr + i / 2, a256, a256_len) == 0);
		CHECK(strcmp(dstr + i / 2 + a256_len, abc + i / 2 + (abc_len - i)) == 0);
		dstr_free(&dstr);

		dstr = dstr_new();
		dstr_t dstr2 = dstr_from_cstr(abc);
		dstr_replace_dstr(&dstr, 0, 0, dstr2);
		dstr_free(&dstr2);
		CHECK(sanity_check(dstr));
		dstr2 = dstr_from_cstr(a256);
		dstr_replace_dstr(&dstr, i, DSTR_NPOS, dstr2);
		dstr_free(&dstr2);
		CHECK(sanity_check(dstr));
		CHECK(strncmp(dstr, abc, i) == 0);
		CHECK(strcmp(dstr + i, a256) == 0);
		dstr_free(&dstr);
		dstr = dstr_new();
		dstr2 = dstr_from_cstr(abc);
		dstr_replace_dstr(&dstr, 0, 0, dstr2);
		dstr_free(&dstr2);
		CHECK(sanity_check(dstr));
		dstr2 = dstr_from_cstr(a256);
		dstr_replace_dstr(&dstr, 0, i, dstr2);
		dstr_free(&dstr2);
		CHECK(sanity_check(dstr));
		CHECK(strncmp(dstr, a256, a256_len) == 0);
		CHECK(strcmp(dstr + a256_len, abc + i) == 0);
		dstr_free(&dstr);
		dstr = dstr_new();
		dstr2 = dstr_from_cstr(abc);
		dstr_replace_dstr(&dstr, 0, 0, dstr2);
		dstr_free(&dstr2);
		CHECK(sanity_check(dstr));
		dstr2 = dstr_from_cstr(a256);
		dstr_replace_dstr(&dstr, i / 2, abc_len - i, dstr2);
		dstr_free(&dstr2);
		CHECK(sanity_check(dstr));
		CHECK(strncmp(dstr, abc, i / 2) == 0);
		CHECK(strncmp(dstr + i / 2, a256, a256_len) == 0);
		CHECK(strcmp(dstr + i / 2 + a256_len, abc + i / 2 + (abc_len - i)) == 0);
		dstr_free(&dstr);

		dstr = dstr_new();
		dstr_replace_fmt(&dstr, 0, 0, "%s", abc);
		CHECK(sanity_check(dstr));
		dstr_replace_fmt(&dstr, i, DSTR_NPOS, "%s", a256);
		CHECK(sanity_check(dstr));
		CHECK(strncmp(dstr, abc, i) == 0);
		CHECK(strcmp(dstr + i, a256) == 0);
		dstr_free(&dstr);
		dstr = dstr_new();
		dstr_replace_fmt(&dstr, 0, 0, "%s", abc);
		CHECK(sanity_check(dstr));
		dstr_replace_fmt(&dstr, 0, i, "%s", a256);
		CHECK(sanity_check(dstr));
		CHECK(strncmp(dstr, a256, a256_len) == 0);
		CHECK(strcmp(dstr + a256_len, abc + i) == 0);
		dstr_free(&dstr);
		dstr = dstr_new();
		dstr_replace_fmt(&dstr, 0, 0, "%s", abc);
		CHECK(sanity_check(dstr));
		dstr_replace_fmt(&dstr, i / 2, abc_len - i, "%s", a256);
		CHECK(sanity_check(dstr));
		CHECK(strncmp(dstr, abc, i / 2) == 0);
		CHECK(strncmp(dstr + i / 2, a256, a256_len) == 0);
		CHECK(strcmp(dstr + i / 2 + a256_len, abc + i / 2 + (abc_len - i)) == 0);
		dstr_free(&dstr);
	}

	for (size_t i = 0; i <= a256_len; i++) {
		dstr_t dstr = dstr_new();
		dstr_replace_cstr(&dstr, 0, 0, a256);
		CHECK(sanity_check(dstr));
		dstr_replace_cstr(&dstr, i, DSTR_NPOS, abc);
		CHECK(sanity_check(dstr));
		CHECK(strncmp(dstr, a256, i) == 0);
		CHECK(strcmp(dstr + i, abc) == 0);
		dstr_free(&dstr);
		dstr = dstr_new();
		dstr_replace_cstr(&dstr, 0, 0, a256);
		CHECK(sanity_check(dstr));
		dstr_replace_cstr(&dstr, 0, i, abc);
		CHECK(sanity_check(dstr));
		CHECK(strncmp(dstr, abc, abc_len) == 0);
		CHECK(strcmp(dstr + abc_len, a256 + i) == 0);
		dstr_free(&dstr);
		dstr = dstr_new();
		dstr_replace_cstr(&dstr, 0, 0, a256);
		CHECK(sanity_check(dstr));
		dstr_replace_cstr(&dstr, i / 2, a256_len - i, abc);
		CHECK(sanity_check(dstr));
		CHECK(strncmp(dstr, a256, i / 2) == 0);
		CHECK(strncmp(dstr + i / 2, abc, abc_len) == 0);
		CHECK(strcmp(dstr + i / 2 + abc_len, a256 + i / 2 + (a256_len - i)) == 0);
		dstr_free(&dstr);

		dstr = dstr_new();
		dstr_replace_view(&dstr, 0, 0, strview_from_cstr(a256));
		CHECK(sanity_check(dstr));
		dstr_replace_view(&dstr, i, DSTR_NPOS, strview_from_cstr(abc));
		CHECK(sanity_check(dstr));
		CHECK(strncmp(dstr, a256, i) == 0);
		CHECK(strcmp(dstr + i, abc) == 0);
		dstr_free(&dstr);
		dstr = dstr_new();
		dstr_replace_view(&dstr, 0, 0, strview_from_cstr(a256));
		CHECK(sanity_check(dstr));
		dstr_replace_view(&dstr, 0, i, strview_from_cstr(abc));
		CHECK(sanity_check(dstr));
		CHECK(strncmp(dstr, abc, abc_len) == 0);
		CHECK(strcmp(dstr + abc_len, a256 + i) == 0);
		dstr_free(&dstr);
		dstr = dstr_new();
		dstr_replace_view(&dstr, 0, 0, strview_from_cstr(a256));
		CHECK(sanity_check(dstr));
		dstr_replace_view(&dstr, i / 2, a256_len - i, strview_from_cstr(abc));
		CHECK(sanity_check(dstr));
		CHECK(strncmp(dstr, a256, i / 2) == 0);
		CHECK(strncmp(dstr + i / 2, abc, abc_len) == 0);
		CHECK(strcmp(dstr + i / 2 + abc_len, a256 + i / 2 + (a256_len - i)) == 0);
		dstr_free(&dstr);

		dstr = dstr_new();
		dstr_replace_chars(&dstr, 0, 0, a256, a256_len);
		CHECK(sanity_check(dstr));
		dstr_replace_chars(&dstr, i, DSTR_NPOS, abc, abc_len);
		CHECK(sanity_check(dstr));
		CHECK(strncmp(dstr, a256, i) == 0);
		CHECK(strcmp(dstr + i, abc) == 0);
		dstr_free(&dstr);
		dstr = dstr_new();
		dstr_replace_chars(&dstr, 0, 0, a256, a256_len);
		CHECK(sanity_check(dstr));
		dstr_replace_chars(&dstr, 0, i, abc, abc_len);
		CHECK(sanity_check(dstr));
		CHECK(strncmp(dstr, abc, abc_len) == 0);
		CHECK(strcmp(dstr + abc_len, a256 + i) == 0);
		dstr_free(&dstr);
		dstr = dstr_new();
		dstr_replace_chars(&dstr, 0, 0, a256, a256_len);
		CHECK(sanity_check(dstr));
		dstr_replace_chars(&dstr, i / 2, a256_len - i, abc, abc_len);
		CHECK(sanity_check(dstr));
		CHECK(strncmp(dstr, a256, i / 2) == 0);
		CHECK(strncmp(dstr + i / 2, abc, abc_len) == 0);
		CHECK(strcmp(dstr + i / 2 + abc_len, a256 + i / 2 + (a256_len - i)) == 0);
		dstr_free(&dstr);

		dstr = dstr_new();
		dstr_t dstr2 = dstr_from_cstr(a256);
		dstr_replace_dstr(&dstr, 0, 0, dstr2);
		dstr_free(&dstr2);
		CHECK(sanity_check(dstr));
		dstr2 = dstr_from_cstr(abc);
		dstr_replace_dstr(&dstr, i, DSTR_NPOS, dstr2);
		dstr_free(&dstr2);
		CHECK(sanity_check(dstr));
		CHECK(strncmp(dstr, a256, i) == 0);
		CHECK(strcmp(dstr + i, abc) == 0);
		dstr_free(&dstr);
		dstr = dstr_new();
		dstr2 = dstr_from_cstr(a256);
		dstr_replace_dstr(&dstr, 0, 0, dstr2);
		dstr_free(&dstr2);
		CHECK(sanity_check(dstr));
		dstr2 = dstr_from_cstr(abc);
		dstr_replace_dstr(&dstr, 0, i, dstr2);
		dstr_free(&dstr2);
		CHECK(sanity_check(dstr));
		CHECK(strncmp(dstr, abc, abc_len) == 0);
		CHECK(strcmp(dstr + abc_len, a256 + i) == 0);
		dstr_free(&dstr);
		dstr = dstr_new();
		dstr2 = dstr_from_cstr(a256);
		dstr_replace_dstr(&dstr, 0, 0, dstr2);
		dstr_free(&dstr2);
		CHECK(sanity_check(dstr));
		CHECK(dstr2 = dstr_from_cstr(abc));
		dstr_replace_dstr(&dstr, i / 2, a256_len - i, dstr2);
		dstr_free(&dstr2);
		CHECK(sanity_check(dstr));
		CHECK(strncmp(dstr, a256, i / 2) == 0);
		CHECK(strncmp(dstr + i / 2, abc, abc_len) == 0);
		CHECK(strcmp(dstr + i / 2 + abc_len, a256 + i / 2 + (a256_len - i)) == 0);
		dstr_free(&dstr);

		dstr = dstr_new();
		dstr_replace_fmt(&dstr, 0, 0, "%s", a256);
		CHECK(sanity_check(dstr));
		dstr_replace_fmt(&dstr, i, DSTR_NPOS, "%s", abc);
		CHECK(sanity_check(dstr));
		CHECK(strncmp(dstr, a256, i) == 0);
		CHECK(strcmp(dstr + i, abc) == 0);
		dstr_free(&dstr);
		dstr = dstr_new();
		dstr_replace_fmt(&dstr, 0, 0, "%s", a256);
		CHECK(sanity_check(dstr));
		dstr_replace_fmt(&dstr, 0, i, "%s", abc);
		CHECK(sanity_check(dstr));
		CHECK(strncmp(dstr, abc, abc_len) == 0);
		CHECK(strcmp(dstr + abc_len, a256 + i) == 0);
		dstr_free(&dstr);
		dstr = dstr_new();
		dstr_replace_fmt(&dstr, 0, 0, "%s", a256);
		CHECK(sanity_check(dstr));
		dstr_replace_fmt(&dstr, i / 2, a256_len - i, "%s", abc);
		CHECK(sanity_check(dstr));
		CHECK(strncmp(dstr, a256, i / 2) == 0);
		CHECK(strncmp(dstr + i / 2, abc, abc_len) == 0);
		CHECK(strcmp(dstr + i / 2 + abc_len, a256 + i / 2 + (a256_len - i)) == 0);
		dstr_free(&dstr);
	}

	dstr_t dstr = dstr_from_cstr(abc);
	for (size_t i = 0; i < abc_len; i++) {
		dstr_replace_cstr(&dstr, i % dstr_length(dstr), 1, "");
		CHECK(sanity_check(dstr));
	}
	CHECK(dstr_is_empty(dstr));
	dstr_free(&dstr);

	dstr = dstr_from_cstr(a256);
	for (size_t i = 0; i < a256_len; i++) {
		dstr_replace_view(&dstr, i % dstr_length(dstr), 1, strview_from_cstr(""));
		CHECK(sanity_check(dstr));
	}
	CHECK(dstr_is_empty(dstr));
	dstr_free(&dstr);

	dstr = dstr_from_cstr(abc);
	for (size_t i = 0; i < abc_len; i++) {
		dstr_replace_chars(&dstr, i % dstr_length(dstr), 1, "", 0);
		CHECK(sanity_check(dstr));
	}
	CHECK(dstr_is_empty(dstr));
	dstr_free(&dstr);

	dstr = dstr_from_cstr(a256);
	for (size_t i = 0; i < a256_len; i++) {
		dstr_replace_fmt(&dstr, i % dstr_length(dstr), 1, "%s", "");
		CHECK(sanity_check(dstr));
	}
	CHECK(dstr_is_empty(dstr));
	dstr_free(&dstr);

	dstr = dstr_new();
	dstr_replace_cstr(&dstr, 0, 0, "");
	CHECK(sanity_check(dstr));
	CHECK(dstr_is_empty(dstr));
	dstr_free(&dstr);
	return true;
}

SIMPLE_TEST(dstring_erase)
{
	dstr_t dstr = dstr_new();
	dstr_erase(&dstr, 0, 0);
	CHECK(sanity_check(dstr));
	dstr_append_cstr(&dstr, a256);
	dstr_append_cstr(&dstr, a256);
	CHECK(strncmp(dstr, a256, a256_len) == 0);
	CHECK(strcmp(dstr + a256_len, a256) == 0);
	CHECK(sanity_check(dstr));
	dstr_erase(&dstr, 0, a256_len / 4);
	CHECK(dstr_length(dstr) == 7 * a256_len / 4);
	CHECK(strncmp(dstr, a256 + a256_len / 4, 3 * (a256_len / 4)) == 0);
	CHECK(strcmp(dstr + 3 * (a256_len / 4), a256) == 0);
	CHECK(sanity_check(dstr));
	dstr_erase(&dstr, dstr_length(dstr) - a256_len / 4, a256_len / 4);
	CHECK(dstr_length(dstr) == 3 * a256_len / 2);
	CHECK(strncmp(dstr, a256 + a256_len / 4, 3 * (a256_len / 4)) == 0);
	CHECK(strncmp(dstr + 3 * (a256_len / 4), a256, 3 * (a256_len / 4)) == 0);
	CHECK(sanity_check(dstr));
	dstr_erase(&dstr, a256_len / 2, a256_len / 2);
	CHECK(dstr_length(dstr) == a256_len);
	CHECK(strncmp(dstr, a256 + a256_len / 4, a256_len / 2) == 0);
	CHECK(strncmp(dstr + a256_len / 2, a256 + a256_len / 4, a256_len / 2) == 0);
	CHECK(sanity_check(dstr));
	dstr_free(&dstr);

	dstr = dstr_new();
	dstr_append_cstr(&dstr, a256);
	CHECK(strcmp(dstr, a256) == 0);
	CHECK(sanity_check(dstr));
	dstr_erase(&dstr, 0, a256_len / 8);
	CHECK(dstr_length(dstr) == 7 * a256_len / 8);
	CHECK(strncmp(dstr, a256 + a256_len / 8, 7 * a256_len / 8) == 0);
	CHECK(sanity_check(dstr));
	dstr_erase(&dstr, dstr_length(dstr) - a256_len / 8, a256_len / 8);
	CHECK(dstr_length(dstr) == 3 * a256_len / 4);
	CHECK(strncmp(dstr, a256 + a256_len / 8, 3 * a256_len / 4) == 0);
	CHECK(sanity_check(dstr));
	dstr_erase(&dstr, a256_len / 4, a256_len / 4);
	CHECK(dstr_length(dstr) == a256_len / 2);
	CHECK(strncmp(dstr, a256 + a256_len / 8, a256_len / 4) == 0);
	CHECK(strncmp(dstr + a256_len / 4, a256 + 5 * a256_len / 8, a256_len / 4) == 0);
	CHECK(sanity_check(dstr));
	dstr_free(&dstr);

	dstr = dstr_new();
	dstr_append_chars(&dstr, abc, 24);
	CHECK(strncmp(dstr, abc, 24) == 0);
	CHECK(sanity_check(dstr));
	dstr_erase(&dstr, 0, 24 / 8);
	CHECK(dstr_length(dstr) == 7 * 24 / 8);
	CHECK(strncmp(dstr, abc + 24 / 8, 7 * 24 / 8) == 0);
	CHECK(sanity_check(dstr));
	dstr_erase(&dstr, dstr_length(dstr) - 24 / 8, 24 / 8);
	CHECK(dstr_length(dstr) == 3 * 24 / 4);
	CHECK(strncmp(dstr, abc + 24 / 8, 3 * 24 / 4) == 0);
	CHECK(sanity_check(dstr));
	dstr_erase(&dstr, 24 / 4, 24 / 4);
	CHECK(dstr_length(dstr) == 24 / 2);
	CHECK(strncmp(dstr, abc + 24 / 8, 24 / 4) == 0);
	CHECK(strncmp(dstr + 24 / 4, abc + 5 * 24 / 8, 24 / 4) == 0);
	CHECK(sanity_check(dstr));
	dstr_free(&dstr);

	for (size_t i = 0; i + 3 <= abc_len; i++) {
		dstr = dstr_from_cstr(abc);
		dstr_erase(&dstr, i, 3);
		CHECK(dstr_length(dstr) == abc_len - 3);
		CHECK(strncmp(dstr, abc, i) == 0);
		CHECK(strcmp(dstr + i, abc + i + 3) == 0);
		CHECK(sanity_check(dstr));
		dstr_free(&dstr);
	}

	for (size_t i = 0; i + 2 <= a256_len; i++) {
		dstr = dstr_from_cstr(a256);
		dstr_erase(&dstr, i, 2);
		CHECK(dstr_length(dstr) == a256_len - 2);
		CHECK(strncmp(dstr, a256, i) == 0);
		CHECK(strcmp(dstr + i, a256 + i + 2) == 0);
		CHECK(sanity_check(dstr));
		dstr_free(&dstr);
	}

	dstr = dstr_from_cstr(a256);
	for (size_t i = 0; i < a256_len; i++) {
		dstr_erase(&dstr, i % dstr_length(dstr), 1);
		CHECK(sanity_check(dstr));
	}
	CHECK(dstr_is_empty(dstr));
	dstr_erase(&dstr, 0, 0);
	CHECK(dstr_is_empty(dstr));
	CHECK(sanity_check(dstr));
	dstr_free(&dstr);

	dstr = dstr_from_cstr(abc);
	for (size_t i = 0; i < abc_len; i++) {
		dstr_erase(&dstr, i, 0);
		CHECK(sanity_check(dstr));
	}
	CHECK(dstr_equals_cstr(dstr, abc));
	dstr_free(&dstr);

	dstr = dstr_from_cstr(a256);
	for (size_t i = 0; i < a256_len; i++) {
		dstr_erase(&dstr, i, 0);
		CHECK(sanity_check(dstr));
	}
	CHECK(dstr_equals_cstr(dstr, a256));
	dstr_free(&dstr);
	return true;
}

SIMPLE_TEST(dstring_strip)
{
	dstr_t dstr = dstr_from_cstr("---aaa---");
	dstr_strip(&dstr, "-");
	CHECK(dstr_equals_cstr(dstr, "aaa"));
	CHECK(sanity_check(dstr));
	dstr_free(&dstr);

	dstr = dstr_from_cstr("---aaa---");
	dstr_lstrip(&dstr, "-");
	CHECK(dstr_equals_cstr(dstr, "aaa---"));
	CHECK(sanity_check(dstr));
	dstr_rstrip(&dstr, "-");
	CHECK(dstr_equals_cstr(dstr, "aaa"));
	CHECK(sanity_check(dstr));
	dstr_free(&dstr);

	dstr = dstr_from_cstr("---aaa---");
	dstr_rstrip(&dstr, "-");
	CHECK(dstr_equals_cstr(dstr, "---aaa"));
	CHECK(sanity_check(dstr));
	dstr_lstrip(&dstr, "-");
	CHECK(dstr_equals_cstr(dstr, "aaa"));
	CHECK(sanity_check(dstr));
	dstr_free(&dstr);

	dstr = dstr_from_cstr("abcabacba");
	dstr_strip(&dstr, "ab");
	CHECK(dstr_equals_cstr(dstr, "cabac"));
	CHECK(sanity_check(dstr));
	dstr_strip(&dstr, "ca");
	CHECK(dstr_equals_cstr(dstr, "b"));
	CHECK(sanity_check(dstr));
	dstr_free(&dstr);

	dstr = dstr_from_cstr("abcdeffedcba");
	dstr_strip(&dstr, "bcdef");
	CHECK(dstr_equals_cstr(dstr, "abcdeffedcba"));
	CHECK(sanity_check(dstr));
	dstr_strip(&dstr, "");
	CHECK(dstr_equals_cstr(dstr, "abcdeffedcba"));
	CHECK(sanity_check(dstr));
	dstr_strip(&dstr, "ac");
	CHECK(dstr_equals_cstr(dstr, "bcdeffedcb"));
	CHECK(sanity_check(dstr));
	dstr_lstrip(&dstr, "efcb");
	CHECK(dstr_equals_cstr(dstr, "deffedcb"));
	CHECK(sanity_check(dstr));
	dstr_strip(&dstr, "bcdf");
	CHECK(dstr_equals_cstr(dstr, "effe"));
	CHECK(sanity_check(dstr));
	dstr_rstrip(&dstr, "ef");
	CHECK(dstr_equals_cstr(dstr, ""));
	CHECK(sanity_check(dstr));
	dstr_strip(&dstr, "");
	CHECK(dstr_equals_cstr(dstr, ""));
	CHECK(sanity_check(dstr));
	dstr_strip(&dstr, "abcdef");
	CHECK(dstr_equals_cstr(dstr, ""));
	CHECK(sanity_check(dstr));
	dstr_free(&dstr);
	return true;
}

SIMPLE_TEST(dstring_to_cstr)
{
	dstr_t dstr = dstr_from_cstr(abc);
	char *cstr = dstr_to_cstr(&dstr);
	CHECK(!dstr);
	CHECK(strcmp(cstr, abc) == 0);
	free(cstr);

	dstr = dstr_from_cstr(abc);
	cstr = dstr_to_cstr_copy(dstr);
	CHECK(strcmp(cstr, abc) == 0);
	free(cstr);
	dstr_free(&dstr);

	dstr = dstr_from_cstr(a256);
	cstr = dstr_to_cstr(&dstr);
	CHECK(!dstr);
	CHECK(strcmp(cstr, a256) == 0);
	free(cstr);

	dstr = dstr_from_cstr(a256);
	cstr = dstr_to_cstr_copy(dstr);
	CHECK(strcmp(cstr, a256) == 0);
	free(cstr);
	dstr_free(&dstr);
	return true;
}

SIMPLE_TEST(dstring_from_cstr)
{
	dstr_t dstr = dstr_from_cstr(abc);
	struct strview view = dstr_view(dstr);
	CHECK(strview_equal(view, strview_from_cstr(abc)));
	dstr_free(&dstr);

	dstr = dstr_from_cstr(a256);
	view = dstr_view(dstr);
	CHECK(strview_equal(view, strview_from_cstr(a256)));
	dstr_free(&dstr);
	return true;
}

SIMPLE_TEST(dstring_substring)
{
	dstr_t dstr = dstr_from_cstr(abc);
	for (size_t i = 0; i < abc_len / 2; i++) {
		struct strview view = dstr_substring_view(dstr, i, abc_len - 2 * i);
		dstr_t dstr2 = dstr_substring_copy(dstr, i, abc_len - 2 * i);
		CHECK(sanity_check(dstr2));
		CHECK(strview_equal(view,
				    strview_substring(strview_from_cstr(abc), i, abc_len - 2 * i)));
		CHECK(dstr_equals_view(dstr2, view));
		dstr_free(&dstr2);
	}
	dstr_free(&dstr);

	dstr = dstr_from_cstr(a256);
	for (size_t i = 0; i < a256_len; i++) {
		struct strview view = dstr_substring_view(dstr, i, a256_len - 2 * i);
		dstr_t dstr2 = dstr_substring_copy(dstr, i, a256_len - 2 * i);
		CHECK(sanity_check(dstr2));
		CHECK(strview_equal(view,
				    strview_substring(strview_from_cstr(a256), i, a256_len - 2 * i)));
		CHECK(dstr_equals_view(dstr2, view));
		dstr_free(&dstr2);
	}
	dstr_free(&dstr);

	for (size_t i = 0; i < abc_len / 2; i++) {
		dstr = dstr_from_cstr(abc);
		dstr_t dstr2 = dstr_substring_copy(dstr, i, abc_len - 2 * i);
		dstr_substring(&dstr, i, abc_len - 2 * i);
		CHECK(sanity_check(dstr));
		CHECK(sanity_check(dstr2));
		CHECK(dstr_equals_view(dstr,
				      strview_substring(strview_from_cstr(abc), i, abc_len - 2 * i)));
		CHECK(dstr_equals_dstr(dstr, dstr2));
		dstr_free(&dstr2);
		dstr_free(&dstr);
	}

	for (size_t i = 0; i < a256_len; i++) {
		dstr = dstr_from_cstr(a256);
		dstr_t dstr2 = dstr_substring_copy(dstr, i, a256_len - 2 * i);
		dstr_substring(&dstr, i, a256_len - 2 * i);
		CHECK(sanity_check(dstr));
		CHECK(sanity_check(dstr2));
		CHECK(dstr_equals_view(dstr,
				       strview_substring(strview_from_cstr(a256), i, a256_len - 2 * i)));
		CHECK(dstr_equals_dstr(dstr, dstr2));
		dstr_free(&dstr2);
		dstr_free(&dstr);
	}

	dstr = dstr_new();
	dstr_t dstr2 = dstr_substring_copy(dstr, 0, 0);
	dstr_substring(&dstr, 0, 0);
	CHECK(dstr_equals_cstr(dstr, ""));
	CHECK(dstr_equals_dstr(dstr, dstr2));
	CHECK(sanity_check(dstr));
	CHECK(sanity_check(dstr2));
	dstr_free(&dstr2);
	dstr_free(&dstr);
	return true;
}

SIMPLE_TEST(dstring_compare)
{
	dstr_t dstr = dstr_from_cstr("abc");
	const char *str = "abc";
	int r = sign(dstr_compare_cstr(dstr, str));
	CHECK(r == 0);
	CHECK(sign(dstr_compare_view(dstr, strview_from_cstr(str))) == r);
	dstr_t dstr2 = dstr_from_cstr(str);
	CHECK(sign(dstr_compare_dstr(dstr, dstr2)) == r);
	dstr_free(&dstr2);
	dstr_free(&dstr);

	dstr = dstr_from_cstr("");
	str = "";
	r = sign(dstr_compare_cstr(dstr, str));
	CHECK(r == 0);
	CHECK(sign(dstr_compare_view(dstr, strview_from_cstr(str))) == r);
	dstr2 = dstr_from_cstr(str);
	CHECK(sign(dstr_compare_dstr(dstr, dstr2)) == r);
	dstr_free(&dstr2);
	dstr_free(&dstr);

	dstr = dstr_from_cstr("abc");
	str = "";
	r = sign(dstr_compare_cstr(dstr, str));
	CHECK(r > 0);
	CHECK(sign(dstr_compare_view(dstr, strview_from_cstr(str))) == r);
	dstr2 = dstr_from_cstr(str);
	CHECK(sign(dstr_compare_dstr(dstr, dstr2)) == r);
	dstr_free(&dstr2);
	dstr_free(&dstr);

	dstr = dstr_from_cstr("");
	str = "abc";
	r = sign(dstr_compare_cstr(dstr, str));
	CHECK(r < 0);
	CHECK(sign(dstr_compare_view(dstr, strview_from_cstr(str))) == r);
	dstr2 = dstr_from_cstr(str);
	CHECK(sign(dstr_compare_dstr(dstr, dstr2)) == r);
	dstr_free(&dstr2);
	dstr_free(&dstr);

	dstr = dstr_from_cstr("abc");
	str = "def";
	r = sign(dstr_compare_cstr(dstr, str));
	CHECK(r < 0);
	CHECK(sign(dstr_compare_view(dstr, strview_from_cstr(str))) == r);
	dstr2 = dstr_from_cstr(str);
	CHECK(sign(dstr_compare_dstr(dstr, dstr2)) == r);
	dstr_free(&dstr2);
	dstr_free(&dstr);

	dstr = dstr_from_cstr("def");
	str = "abc";
	r = sign(dstr_compare_cstr(dstr, str));
	CHECK(r > 0);
	CHECK(sign(dstr_compare_view(dstr, strview_from_cstr(str))) == r);
	dstr2 = dstr_from_cstr(str);
	CHECK(sign(dstr_compare_dstr(dstr, dstr2)) == r);
	dstr_free(&dstr2);
	dstr_free(&dstr);

	dstr = dstr_from_cstr("abc");
	str = "abcd";
	r = sign(dstr_compare_cstr(dstr, str));
	CHECK(r < 0);
	CHECK(sign(dstr_compare_view(dstr, strview_from_cstr(str))) == r);
	dstr2 = dstr_from_cstr(str);
	CHECK(sign(dstr_compare_dstr(dstr, dstr2)) == r);
	dstr_free(&dstr2);
	dstr_free(&dstr);

	dstr = dstr_from_cstr("abc");
	str = "ab";
	r = sign(dstr_compare_cstr(dstr, str));
	CHECK(r > 0);
	CHECK(sign(dstr_compare_view(dstr, strview_from_cstr(str))) == r);
	dstr2 = dstr_from_cstr(str);
	CHECK(sign(dstr_compare_dstr(dstr, dstr2)) == r);
	dstr_free(&dstr2);
	dstr_free(&dstr);

	dstr = dstr_from_cstr("abc");
	str = "abd";
	r = sign(dstr_compare_cstr(dstr, str));
	CHECK(r < 0);
	CHECK(sign(dstr_compare_view(dstr, strview_from_cstr(str))) == r);
	dstr2 = dstr_from_cstr(str);
	CHECK(sign(dstr_compare_dstr(dstr, dstr2)) == r);
	dstr_free(&dstr2);
	dstr_free(&dstr);

	dstr = dstr_from_cstr("abc");
	str = "abb";
	r = sign(dstr_compare_cstr(dstr, str));
	CHECK(r > 0);
	CHECK(sign(dstr_compare_view(dstr, strview_from_cstr(str))) == r);
	dstr2 = dstr_from_cstr(str);
	CHECK(sign(dstr_compare_dstr(dstr, dstr2)) == r);
	dstr_free(&dstr2);
	dstr_free(&dstr);

	dstr = dstr_from_cstr(abc);
	str = abc;
	r = sign(dstr_compare_cstr(dstr, str));
	CHECK(r == 0);
	CHECK(sign(dstr_compare_view(dstr, strview_from_cstr(str))) == r);
	dstr2 = dstr_from_cstr(str);
	CHECK(sign(dstr_compare_dstr(dstr, dstr2)) == r);
	dstr_free(&dstr2);
	dstr_free(&dstr);

	dstr = dstr_from_cstr(a256);
	str = a256;
	r = sign(dstr_compare_cstr(dstr, str));
	CHECK(r == 0);
	CHECK(sign(dstr_compare_view(dstr, strview_from_cstr(str))) == r);
	dstr2 = dstr_from_cstr(str);
	CHECK(sign(dstr_compare_dstr(dstr, dstr2)) == r);
	dstr_free(&dstr2);
	dstr_free(&dstr);

	dstr = dstr_from_cstr(abc);
	str = a256;
	r = sign(dstr_compare_cstr(dstr, str));
	CHECK(r < 0);
	CHECK(sign(dstr_compare_view(dstr, strview_from_cstr(str))) == r);
	dstr2 = dstr_from_cstr(str);
	CHECK(sign(dstr_compare_dstr(dstr, dstr2)) == r);
	dstr_free(&dstr2);
	dstr_free(&dstr);

	dstr = dstr_from_cstr(a256);
	str = abc;
	r = sign(dstr_compare_cstr(dstr, str));
	CHECK(r > 0);
	CHECK(sign(dstr_compare_view(dstr, strview_from_cstr(str))) == r);
	dstr2 = dstr_from_cstr(str);
	CHECK(sign(dstr_compare_dstr(dstr, dstr2)) == r);
	dstr_free(&dstr2);
	dstr_free(&dstr);

	dstr = dstr_from_cstr("abc");
	str = "abc";
	r = dstr_equals_cstr(dstr, str);
	CHECK(r);
	CHECK(dstr_equals_view(dstr, strview_from_cstr(str)) == r);
	dstr2 = dstr_from_cstr(str);
	CHECK(dstr_equals_dstr(dstr, dstr2) == r);
	dstr_free(&dstr2);
	dstr_free(&dstr);

	dstr = dstr_from_cstr("abc");
	str = "ab";
	r = dstr_equals_cstr(dstr, str);
	CHECK(!r);
	CHECK(dstr_equals_view(dstr, strview_from_cstr(str)) == r);
	dstr2 = dstr_from_cstr(str);
	CHECK(dstr_equals_dstr(dstr, dstr2) == r);
	dstr_free(&dstr2);
	dstr_free(&dstr);

	dstr = dstr_from_cstr("abc");
	str = "abcd";
	r = dstr_equals_cstr(dstr, str);
	CHECK(!r);
	CHECK(dstr_equals_view(dstr, strview_from_cstr(str)) == r);
	dstr2 = dstr_from_cstr(str);
	CHECK(dstr_equals_dstr(dstr, dstr2) == r);
	dstr_free(&dstr2);
	dstr_free(&dstr);

	dstr = dstr_from_cstr("abc");
	str = "abd";
	r = dstr_equals_cstr(dstr, str);
	CHECK(!r);
	CHECK(dstr_equals_view(dstr, strview_from_cstr(str)) == r);
	dstr2 = dstr_from_cstr(str);
	CHECK(dstr_equals_dstr(dstr, dstr2) == r);
	dstr_free(&dstr2);
	dstr_free(&dstr);

	dstr = dstr_from_cstr("");
	str = "";
	r = dstr_equals_cstr(dstr, str);
	CHECK(r);
	CHECK(dstr_equals_view(dstr, strview_from_cstr(str)) == r);
	dstr2 = dstr_from_cstr(str);
	CHECK(dstr_equals_dstr(dstr, dstr2) == r);
	dstr_free(&dstr2);
	dstr_free(&dstr);

	dstr = dstr_from_cstr("abc");
	str = "";
	r = dstr_equals_cstr(dstr, str);
	CHECK(!r);
	CHECK(dstr_equals_view(dstr, strview_from_cstr(str)) == r);
	dstr2 = dstr_from_cstr(str);
	CHECK(dstr_equals_dstr(dstr, dstr2) == r);
	dstr_free(&dstr2);
	dstr_free(&dstr);

	dstr = dstr_from_cstr("");
	str = "abc";
	r = dstr_equals_cstr(dstr, str);
	CHECK(!r);
	CHECK(dstr_equals_view(dstr, strview_from_cstr(str)) == r);
	dstr2 = dstr_from_cstr(str);
	CHECK(dstr_equals_dstr(dstr, dstr2) == r);
	dstr_free(&dstr2);
	dstr_free(&dstr);

	dstr = dstr_from_cstr(abc);
	str = abc;
	r = dstr_equals_cstr(dstr, str);
	CHECK(r);
	CHECK(dstr_equals_view(dstr, strview_from_cstr(str)) == r);
	dstr2 = dstr_from_cstr(str);
	CHECK(dstr_equals_dstr(dstr, dstr2) == r);
	dstr_free(&dstr2);
	dstr_free(&dstr);

	dstr = dstr_from_cstr(a256);
	str = a256;
	r = dstr_equals_cstr(dstr, str);
	CHECK(r);
	CHECK(dstr_equals_view(dstr, strview_from_cstr(str)) == r);
	dstr2 = dstr_from_cstr(str);
	CHECK(dstr_equals_dstr(dstr, dstr2) == r);
	dstr_free(&dstr2);
	dstr_free(&dstr);

	dstr = dstr_from_cstr(abc);
	str = a256;
	r = dstr_equals_cstr(dstr, str);
	CHECK(!r);
	CHECK(dstr_equals_view(dstr, strview_from_cstr(str)) == r);
	dstr2 = dstr_from_cstr(str);
	CHECK(dstr_equals_dstr(dstr, dstr2) == r);
	dstr_free(&dstr2);
	dstr_free(&dstr);

	dstr = dstr_from_cstr(a256);
	str = abc;
	r = dstr_equals_cstr(dstr, str);
	CHECK(!r);
	CHECK(dstr_equals_view(dstr, strview_from_cstr(str)) == r);
	dstr2 = dstr_from_cstr(str);
	CHECK(dstr_equals_dstr(dstr, dstr2) == r);
	dstr_free(&dstr2);
	dstr_free(&dstr);
	return true;
}

SIMPLE_TEST(dstring_find)
{
	dstr_t 	dstr = dstr_from_cstr("abc");
	const char *str = "abc";
	size_t s = 0;
	size_t p = dstr_find_cstr(dstr, str, s);
	CHECK(p == 0);
	CHECK(dstr_find_view(dstr, strview_from_cstr(str), s) == p);
	dstr_t dstr2 = dstr_from_cstr(str);
	CHECK(dstr_find_dstr(dstr, dstr2, s) == p);
	dstr_free(&dstr2);
	dstr_free(&dstr);

	dstr = dstr_from_cstr("abc");
	str = "ab";
	s = 0;
	p = dstr_find_cstr(dstr, str, s);
	CHECK(p == 0);
	CHECK(dstr_find_view(dstr, strview_from_cstr(str), s) == p);
	dstr2 = dstr_from_cstr(str);
	CHECK(dstr_find_dstr(dstr, dstr2, s) == p);
	dstr_free(&dstr2);
	dstr_free(&dstr);

	dstr = dstr_from_cstr("abc");
	str = "a";
	s = 0;
	p = dstr_find_cstr(dstr, str, s);
	CHECK(p == 0);
	CHECK(dstr_find_view(dstr, strview_from_cstr(str), s) == p);
	dstr2 = dstr_from_cstr(str);
	CHECK(dstr_find_dstr(dstr, dstr2, s) == p);
	dstr_free(&dstr2);
	dstr_free(&dstr);

	dstr = dstr_from_cstr("abc");
	str = "";
	s = 0;
	p = dstr_find_cstr(dstr, str, s);
	CHECK(p == 0);
	CHECK(dstr_find_view(dstr, strview_from_cstr(str), s) == p);
	dstr2 = dstr_from_cstr(str);
	CHECK(dstr_find_dstr(dstr, dstr2, s) == p);
	dstr_free(&dstr2);
	dstr_free(&dstr);

	dstr = dstr_from_cstr("abc");
	str = "c";
	s = 0;
	p = dstr_find_cstr(dstr, str, s);
	CHECK(p == 2);
	CHECK(dstr_find_view(dstr, strview_from_cstr(str), s) == p);
	dstr2 = dstr_from_cstr(str);
	CHECK(dstr_find_dstr(dstr, dstr2, s) == p);
	dstr_free(&dstr2);
	dstr_free(&dstr);

	dstr = dstr_from_cstr("abcabc");
	str = "abc";
	s = 1;
	p = dstr_find_cstr(dstr, str, s);
	CHECK(p == 3);
	CHECK(dstr_find_view(dstr, strview_from_cstr(str), s) == p);
	dstr2 = dstr_from_cstr(str);
	CHECK(dstr_find_dstr(dstr, dstr2, s) == p);
	dstr_free(&dstr2);
	dstr_free(&dstr);

	dstr = dstr_from_cstr("abcabcabc");
	str = "abc";
	s = 3;
	p = dstr_find_cstr(dstr, str, s);
	CHECK(p == 3);
	CHECK(dstr_find_view(dstr, strview_from_cstr(str), s) == p);
	dstr2 = dstr_from_cstr(str);
	CHECK(dstr_find_dstr(dstr, dstr2, s) == p);
	dstr_free(&dstr2);
	dstr_free(&dstr);

	dstr = dstr_from_cstr("abcabcabc");
	str = "abc";
	s = 4;
	p = dstr_find_cstr(dstr, str, s);
	CHECK(p == 6);
	CHECK(dstr_find_view(dstr, strview_from_cstr(str), s) == p);
	dstr2 = dstr_from_cstr(str);
	CHECK(dstr_find_dstr(dstr, dstr2, s) == p);
	dstr_free(&dstr2);
	dstr_free(&dstr);

	dstr = dstr_from_cstr("abcabcabc");
	str = "abc";
	s = 7;
	p = dstr_find_cstr(dstr, str, s);
	CHECK(p == DSTR_NPOS);
	CHECK(dstr_find_view(dstr, strview_from_cstr(str), s) == p);
	dstr2 = dstr_from_cstr(str);
	CHECK(dstr_find_dstr(dstr, dstr2, s) == p);
	dstr_free(&dstr2);
	dstr_free(&dstr);

	dstr = dstr_from_cstr("");
	str = "";
	s = 0;
	p = dstr_find_cstr(dstr, str, s);
	CHECK(p == 0);
	CHECK(dstr_find_view(dstr, strview_from_cstr(str), s) == p);
	dstr2 = dstr_from_cstr(str);
	CHECK(dstr_find_dstr(dstr, dstr2, s) == p);
	dstr_free(&dstr2);
	dstr_free(&dstr);

	dstr = dstr_from_cstr("");
	str = "a";
	s = 0;
	p = dstr_find_cstr(dstr, str, s);
	CHECK(p == DSTR_NPOS);
	CHECK(dstr_find_view(dstr, strview_from_cstr(str), s) == p);
	dstr2 = dstr_from_cstr(str);
	CHECK(dstr_find_dstr(dstr, dstr2, s) == p);
	dstr_free(&dstr2);
	dstr_free(&dstr);

	dstr = dstr_from_cstr("abc");
	str = "x";
	s = 0;
	p = dstr_find_cstr(dstr, str, s);
	CHECK(p == DSTR_NPOS);
	CHECK(dstr_find_view(dstr, strview_from_cstr(str), s) == p);
	dstr2 = dstr_from_cstr(str);
	CHECK(dstr_find_dstr(dstr, dstr2, s) == p);
	dstr_free(&dstr2);
	dstr_free(&dstr);

	dstr = dstr_from_cstr("abc");
	str = "abcd";
	s = 0;
	p = dstr_find_cstr(dstr, str, s);
	CHECK(p == DSTR_NPOS);
	CHECK(dstr_find_view(dstr, strview_from_cstr(str), s) == p);
	dstr2 = dstr_from_cstr(str);
	CHECK(dstr_find_dstr(dstr, dstr2, s) == p);
	dstr_free(&dstr2);
	dstr_free(&dstr);

	dstr = dstr_from_cstr("abc");
	str = "abc";
	s = DSTR_NPOS;
	p = dstr_rfind_cstr(dstr, str, s);
	CHECK(p == 0);
	CHECK(dstr_rfind_view(dstr, strview_from_cstr(str), s) == p);
	dstr2 = dstr_from_cstr(str);
	CHECK(dstr_rfind_dstr(dstr, dstr2, s) == p);
	dstr_free(&dstr2);
	dstr_free(&dstr);

	dstr = dstr_from_cstr("abc");
	str = "ab";
	s = DSTR_NPOS;
	p = dstr_rfind_cstr(dstr, str, s);
	CHECK(p == 0);
	CHECK(dstr_rfind_view(dstr, strview_from_cstr(str), s) == p);
	dstr2 = dstr_from_cstr(str);
	CHECK(dstr_rfind_dstr(dstr, dstr2, s) == p);
	dstr_free(&dstr2);
	dstr_free(&dstr);

	dstr = dstr_from_cstr("abc");
	str = "a";
	s = DSTR_NPOS;
	p = dstr_rfind_cstr(dstr, str, s);
	CHECK(p == 0);
	CHECK(dstr_rfind_view(dstr, strview_from_cstr(str), s) == p);
	dstr2 = dstr_from_cstr(str);
	CHECK(dstr_rfind_dstr(dstr, dstr2, s) == p);
	dstr_free(&dstr2);
	dstr_free(&dstr);

	dstr = dstr_from_cstr("abc");
	str = "";
	s = DSTR_NPOS;
	p = dstr_rfind_cstr(dstr, str, s);
	CHECK(p == 3);
	CHECK(dstr_rfind_view(dstr, strview_from_cstr(str), s) == p);
	dstr2 = dstr_from_cstr(str);
	CHECK(dstr_rfind_dstr(dstr, dstr2, s) == p);
	dstr_free(&dstr2);
	dstr_free(&dstr);

	dstr = dstr_from_cstr("abc");
	str = "c";
	s = 1;
	p = dstr_rfind_cstr(dstr, str, s);
	CHECK(p == DSTR_NPOS);
	CHECK(dstr_rfind_view(dstr, strview_from_cstr(str), s) == p);
	dstr2 = dstr_from_cstr(str);
	CHECK(dstr_rfind_dstr(dstr, dstr2, s) == p);
	dstr_free(&dstr2);
	dstr_free(&dstr);

	dstr = dstr_from_cstr("abc");
	str = "c";
	s = DSTR_NPOS;
	p = dstr_rfind_cstr(dstr, str, s);
	CHECK(p == 2);
	CHECK(dstr_rfind_view(dstr, strview_from_cstr(str), s) == p);
	dstr2 = dstr_from_cstr(str);
	CHECK(dstr_rfind_dstr(dstr, dstr2, s) == p);
	dstr_free(&dstr2);
	dstr_free(&dstr);

	dstr = dstr_from_cstr("abcabc");
	str = "abc";
	s = DSTR_NPOS;
	p = dstr_rfind_cstr(dstr, str, s);
	CHECK(p == 3);
	CHECK(dstr_rfind_view(dstr, strview_from_cstr(str), s) == p);
	dstr2 = dstr_from_cstr(str);
	CHECK(dstr_rfind_dstr(dstr, dstr2, s) == p);
	dstr_free(&dstr2);
	dstr_free(&dstr);

	dstr = dstr_from_cstr("abcabc");
	str = "abc";
	s = 2;
	p = dstr_rfind_cstr(dstr, str, s);
	CHECK(p == 0);
	CHECK(dstr_rfind_view(dstr, strview_from_cstr(str), s) == p);
	dstr2 = dstr_from_cstr(str);
	CHECK(dstr_rfind_dstr(dstr, dstr2, s) == p);
	dstr_free(&dstr2);
	dstr_free(&dstr);

	dstr = dstr_from_cstr("abcabcabc");
	str = "abc";
	s = 3;
	p = dstr_rfind_cstr(dstr, str, s);
	CHECK(p == 3);
	CHECK(dstr_rfind_view(dstr, strview_from_cstr(str), s) == p);
	dstr2 = dstr_from_cstr(str);
	CHECK(dstr_rfind_dstr(dstr, dstr2, s) == p);
	dstr_free(&dstr2);
	dstr_free(&dstr);

	dstr = dstr_from_cstr("abcabcabc");
	str = "abc";
	s = 6;
	p = dstr_rfind_cstr(dstr, str, s);
	CHECK(p == 6);
	CHECK(dstr_rfind_view(dstr, strview_from_cstr(str), s) == p);
	dstr2 = dstr_from_cstr(str);
	CHECK(dstr_rfind_dstr(dstr, dstr2, s) == p);
	dstr_free(&dstr2);
	dstr_free(&dstr);

	dstr = dstr_from_cstr("abcabcabc");
	str = "abc";
	s = 0;
	p = dstr_rfind_cstr(dstr, str, s);
	CHECK(p == 0);
	CHECK(dstr_rfind_view(dstr, strview_from_cstr(str), s) == p);
	dstr2 = dstr_from_cstr(str);
	CHECK(dstr_rfind_dstr(dstr, dstr2, s) == p);
	dstr_free(&dstr2);
	dstr_free(&dstr);

	dstr = dstr_from_cstr("abcabcabc");
	str = "abc";
	s = DSTR_NPOS;
	p = dstr_rfind_cstr(dstr, str, s);
	CHECK(p == 6);
	CHECK(dstr_rfind_view(dstr, strview_from_cstr(str), s) == p);
	dstr2 = dstr_from_cstr(str);
	CHECK(dstr_rfind_dstr(dstr, dstr2, s) == p);
	dstr_free(&dstr2);
	dstr_free(&dstr);

	dstr = dstr_from_cstr("");
	str = "";
	s = DSTR_NPOS;
	p = dstr_rfind_cstr(dstr, str, s);
	CHECK(p == 0);
	CHECK(dstr_rfind_view(dstr, strview_from_cstr(str), s) == p);
	dstr2 = dstr_from_cstr(str);
	CHECK(dstr_rfind_dstr(dstr, dstr2, s) == p);
	dstr_free(&dstr2);
	dstr_free(&dstr);

	dstr = dstr_from_cstr("");
	str = "a";
	s = DSTR_NPOS;
	p = dstr_rfind_cstr(dstr, str, s);
	CHECK(p == DSTR_NPOS);
	CHECK(dstr_rfind_view(dstr, strview_from_cstr(str), s) == p);
	dstr2 = dstr_from_cstr(str);
	CHECK(dstr_rfind_dstr(dstr, dstr2, s) == p);
	dstr_free(&dstr2);
	dstr_free(&dstr);

	dstr = dstr_from_cstr("abc");
	str = "x";
	s = DSTR_NPOS;
	p = dstr_rfind_cstr(dstr, str, s);
	CHECK(p == DSTR_NPOS);
	CHECK(dstr_rfind_view(dstr, strview_from_cstr(str), s) == p);
	dstr2 = dstr_from_cstr(str);
	CHECK(dstr_rfind_dstr(dstr, dstr2, s) == p);
	dstr_free(&dstr2);
	dstr_free(&dstr);

	dstr = dstr_from_cstr("abc");
	str = "abcd";
	s = DSTR_NPOS;
	p = dstr_rfind_cstr(dstr, str, s);
	CHECK(p == DSTR_NPOS);
	CHECK(dstr_rfind_view(dstr, strview_from_cstr(str), s) == p);
	dstr2 = dstr_from_cstr(str);
	CHECK(dstr_rfind_dstr(dstr, dstr2, s) == p);
	dstr_free(&dstr2);
	dstr_free(&dstr);
	return true;
}

SIMPLE_TEST(dstring_find_replace)
{
	static const struct {
		const char *haystack;
		const char *needle;
		const char *replacement;
		size_t max;
		const char *result;
		bool reverse;
	} test_cases[] = {
		{"abc", "abc", "abc", SIZE_MAX, "abc", false},
		{"abc", "abc", "x", SIZE_MAX, "x", false},
		{"abc", "abc", a256, SIZE_MAX, a256, false},
		{"abc", "b", "xxxxx", SIZE_MAX, "axxxxxc", false},
		{"abc", "a", "xxxxx", SIZE_MAX, "xxxxxbc", false},
		{"abc", "c", "xxxxx", SIZE_MAX, "abxxxxx", false},
		{"aaa", "a", "aa", SIZE_MAX, "aaaaaa", false},
		{"aaa", "a", "aa", 0, "aaa", false},
		{"aaa", "a", "aa", 1, "aaaa", false},
		{"aaa", "a", "aa", 2, "aaaaa", false},
		{"aaa", "a", "aa", 3, "aaaaaa", false},
		{"", "", "", SIZE_MAX, "", false},
		{"", "", "a", SIZE_MAX, "a", false},
		{"", "x", "y", SIZE_MAX, "", false},
		{"x", "", "a", SIZE_MAX, "axa", false},
		{"xx", "", "a", SIZE_MAX, "axaxa", false},
		{"abcabcabc", "abc", "a", SIZE_MAX, "aaa", false},
		{"abcabcabc", "abc", "a", 1, "aabcabc", false},
		{"abcabcabc", "abc", "abcdef", SIZE_MAX, "abcdefabcdefabcdef", false},
		{"abc", "abc", "abc", SIZE_MAX, "abc", true},
		{"abc", "abc", "x", SIZE_MAX, "x", true},
		{"abc", "abc", a256, SIZE_MAX, a256, true},
		{"abc", "b", "xxxxx", SIZE_MAX, "axxxxxc", true},
		{"abc", "a", "xxxxx", SIZE_MAX, "xxxxxbc", true},
		{"abc", "c", "xxxxx", SIZE_MAX, "abxxxxx", true},
		{"aaa", "a", "aa", SIZE_MAX, "aaaaaa", true},
		{"aaa", "a", "aa", 0, "aaa", true},
		{"aaa", "a", "aa", 1, "aaaa", true},
		{"aaa", "a", "aa", 2, "aaaaa", true},
		{"aaa", "a", "aa", 3, "aaaaaa", true},
		{"", "", "", SIZE_MAX, "", true},
		{"", "", "a", SIZE_MAX, "a", true},
		{"", "x", "y", SIZE_MAX, "", true},
		{"x", "", "a", SIZE_MAX, "axa", true},
		{"xx", "", "a", SIZE_MAX, "axaxa", true},
		{"abcabcabc", "abc", "a", SIZE_MAX, "aaa", true},
		{"abcabcabc", "abc", "a", 1, "abcabca", true},
		{"abcabcabc", "abc", "abcdef", SIZE_MAX, "abcdefabcdefabcdef", true},
		{"abcabcabc", "abc", "a", 2, "abcaa", true},
	};

	for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++) {
		const char *haystack = test_cases[i].haystack;
		const char *needle = test_cases[i].needle;
		const char *replacement = test_cases[i].replacement;
		size_t max = test_cases[i].max;
		const char *result = test_cases[i].result;
		bool reverse = test_cases[i].reverse;
		// test_log("%s %s %s %zu %s\n", haystack, needle, replacement, max, result);

		dstr_t dstr = dstr_from_cstr(haystack);
		if (reverse) {
			dstr_rfind_replace_cstr(&dstr, needle, replacement, max);
		} else {
			dstr_find_replace_cstr(&dstr, needle, replacement, max);
		}
		CHECK(dstr_equals_cstr(dstr, result));
		dstr_free(&dstr);
		dstr = dstr_from_view(strview_from_cstr(haystack));
		if (reverse) {
			dstr_rfind_replace_view(&dstr, strview_from_cstr(needle),
						strview_from_cstr(replacement), max);
		} else {
			dstr_find_replace_view(&dstr, strview_from_cstr(needle),
					       strview_from_cstr(replacement), max);
		}
		CHECK(dstr_equals_view(dstr, strview_from_cstr(result)));
		dstr_free(&dstr);
		dstr = dstr_from_chars(haystack, strlen(haystack));
		dstr_t dstr2 = dstr_from_cstr(needle);
		dstr_t dstr3 = dstr_from_cstr(replacement);
		if (reverse) {
			dstr_rfind_replace_dstr(&dstr, dstr2, dstr3, max);
		} else {
			dstr_find_replace_dstr(&dstr, dstr2, dstr3, max);
		}
		dstr_t dstr4 = dstr_from_cstr(result);
		CHECK(dstr_equals_dstr(dstr, dstr4));
		dstr_free(&dstr2);
		dstr_free(&dstr3);
		dstr_free(&dstr4);
		dstr_free(&dstr);
	}
	return true;
}

SIMPLE_TEST(dstring_find_of)
{
	dstr_t dstr = dstr_from_cstr("abcdefghij0123456789");
	CHECK(dstr_find_first_of(dstr, "", 0) == DSTR_NPOS);
	CHECK(dstr_find_first_of(dstr, "a", 0) == 0);
	CHECK(dstr_find_first_of(dstr, "b", 0) == 1);
	CHECK(dstr_find_first_of(dstr, "c", 0) == 2);
	CHECK(dstr_find_first_of(dstr, "d", 0) == 3);
	CHECK(dstr_find_first_of(dstr, "e", 0) == 4);
	CHECK(dstr_find_first_of(dstr, "f", 0) == 5);
	CHECK(dstr_find_first_of(dstr, "g", 0) == 6);
	CHECK(dstr_find_first_of(dstr, "h", 0) == 7);
	CHECK(dstr_find_first_of(dstr, "i", 0) == 8);
	CHECK(dstr_find_first_of(dstr, "j", 0) == 9);
	CHECK(dstr_find_first_of(dstr, "0", 0) == 10);
	CHECK(dstr_find_first_of(dstr, "1", 0) == 11);
	CHECK(dstr_find_first_of(dstr, "2", 0) == 12);
	CHECK(dstr_find_first_of(dstr, "3", 0) == 13);
	CHECK(dstr_find_first_of(dstr, "4", 0) == 14);
	CHECK(dstr_find_first_of(dstr, "5", 0) == 15);
	CHECK(dstr_find_first_of(dstr, "6", 0) == 16);
	CHECK(dstr_find_first_of(dstr, "7", 0) == 17);
	CHECK(dstr_find_first_of(dstr, "8", 0) == 18);
	CHECK(dstr_find_first_of(dstr, "9", 0) == 19);
	CHECK(dstr_find_first_of(dstr, "a", 0) == 0);
	CHECK(dstr_find_first_of(dstr, "b", 1) == 1);
	CHECK(dstr_find_first_of(dstr, "c", 2) == 2);
	CHECK(dstr_find_first_of(dstr, "d", 3) == 3);
	CHECK(dstr_find_first_of(dstr, "e", 4) == 4);
	CHECK(dstr_find_first_of(dstr, "f", 5) == 5);
	CHECK(dstr_find_first_of(dstr, "g", 6) == 6);
	CHECK(dstr_find_first_of(dstr, "h", 7) == 7);
	CHECK(dstr_find_first_of(dstr, "i", 8) == 8);
	CHECK(dstr_find_first_of(dstr, "j", 9) == 9);
	CHECK(dstr_find_first_of(dstr, "0", 10) == 10);
	CHECK(dstr_find_first_of(dstr, "1", 11) == 11);
	CHECK(dstr_find_first_of(dstr, "2", 12) == 12);
	CHECK(dstr_find_first_of(dstr, "3", 13) == 13);
	CHECK(dstr_find_first_of(dstr, "4", 14) == 14);
	CHECK(dstr_find_first_of(dstr, "5", 15) == 15);
	CHECK(dstr_find_first_of(dstr, "6", 16) == 16);
	CHECK(dstr_find_first_of(dstr, "7", 17) == 17);
	CHECK(dstr_find_first_of(dstr, "8", 18) == 18);
	CHECK(dstr_find_first_of(dstr, "9", 19) == 19);
	CHECK(dstr_find_first_of(dstr, "a", 1) == DSTR_NPOS);
	CHECK(dstr_find_first_of(dstr, "b", 2) == DSTR_NPOS);
	CHECK(dstr_find_first_of(dstr, "c", 3) == DSTR_NPOS);
	CHECK(dstr_find_first_of(dstr, "d", 4) == DSTR_NPOS);
	CHECK(dstr_find_first_of(dstr, "e", 5) == DSTR_NPOS);
	CHECK(dstr_find_first_of(dstr, "f", 6) == DSTR_NPOS);
	CHECK(dstr_find_first_of(dstr, "g", 7) == DSTR_NPOS);
	CHECK(dstr_find_first_of(dstr, "h", 8) == DSTR_NPOS);
	CHECK(dstr_find_first_of(dstr, "i", 9) == DSTR_NPOS);
	CHECK(dstr_find_first_of(dstr, "j", 10) == DSTR_NPOS);
	CHECK(dstr_find_first_of(dstr, "0", 11) == DSTR_NPOS);
	CHECK(dstr_find_first_of(dstr, "1", 12) == DSTR_NPOS);
	CHECK(dstr_find_first_of(dstr, "2", 13) == DSTR_NPOS);
	CHECK(dstr_find_first_of(dstr, "3", 14) == DSTR_NPOS);
	CHECK(dstr_find_first_of(dstr, "4", 15) == DSTR_NPOS);
	CHECK(dstr_find_first_of(dstr, "5", 16) == DSTR_NPOS);
	CHECK(dstr_find_first_of(dstr, "6", 17) == DSTR_NPOS);
	CHECK(dstr_find_first_of(dstr, "7", 18) == DSTR_NPOS);
	CHECK(dstr_find_first_of(dstr, "8", 19) == DSTR_NPOS);
	CHECK(dstr_find_first_of(dstr, "9", 20) == DSTR_NPOS);
	CHECK(dstr_find_last_of(dstr, "", DSTR_NPOS) == DSTR_NPOS);
	CHECK(dstr_find_last_of(dstr, "a", DSTR_NPOS) == 0);
	CHECK(dstr_find_last_of(dstr, "b", DSTR_NPOS) == 1);
	CHECK(dstr_find_last_of(dstr, "c", DSTR_NPOS) == 2);
	CHECK(dstr_find_last_of(dstr, "d", DSTR_NPOS) == 3);
	CHECK(dstr_find_last_of(dstr, "e", DSTR_NPOS) == 4);
	CHECK(dstr_find_last_of(dstr, "f", DSTR_NPOS) == 5);
	CHECK(dstr_find_last_of(dstr, "g", DSTR_NPOS) == 6);
	CHECK(dstr_find_last_of(dstr, "h", DSTR_NPOS) == 7);
	CHECK(dstr_find_last_of(dstr, "i", DSTR_NPOS) == 8);
	CHECK(dstr_find_last_of(dstr, "j", DSTR_NPOS) == 9);
	CHECK(dstr_find_last_of(dstr, "0", DSTR_NPOS) == 10);
	CHECK(dstr_find_last_of(dstr, "1", DSTR_NPOS) == 11);
	CHECK(dstr_find_last_of(dstr, "2", DSTR_NPOS) == 12);
	CHECK(dstr_find_last_of(dstr, "3", DSTR_NPOS) == 13);
	CHECK(dstr_find_last_of(dstr, "4", DSTR_NPOS) == 14);
	CHECK(dstr_find_last_of(dstr, "5", DSTR_NPOS) == 15);
	CHECK(dstr_find_last_of(dstr, "6", DSTR_NPOS) == 16);
	CHECK(dstr_find_last_of(dstr, "7", DSTR_NPOS) == 17);
	CHECK(dstr_find_last_of(dstr, "8", DSTR_NPOS) == 18);
	CHECK(dstr_find_last_of(dstr, "9", DSTR_NPOS) == 19);
	CHECK(dstr_find_last_of(dstr, "a", 0) == 0);
	CHECK(dstr_find_last_of(dstr, "b", 1) == 1);
	CHECK(dstr_find_last_of(dstr, "c", 2) == 2);
	CHECK(dstr_find_last_of(dstr, "d", 3) == 3);
	CHECK(dstr_find_last_of(dstr, "e", 4) == 4);
	CHECK(dstr_find_last_of(dstr, "f", 5) == 5);
	CHECK(dstr_find_last_of(dstr, "g", 6) == 6);
	CHECK(dstr_find_last_of(dstr, "h", 7) == 7);
	CHECK(dstr_find_last_of(dstr, "i", 8) == 8);
	CHECK(dstr_find_last_of(dstr, "j", 9) == 9);
	CHECK(dstr_find_last_of(dstr, "0", 10) == 10);
	CHECK(dstr_find_last_of(dstr, "1", 11) == 11);
	CHECK(dstr_find_last_of(dstr, "2", 12) == 12);
	CHECK(dstr_find_last_of(dstr, "3", 13) == 13);
	CHECK(dstr_find_last_of(dstr, "4", 14) == 14);
	CHECK(dstr_find_last_of(dstr, "5", 15) == 15);
	CHECK(dstr_find_last_of(dstr, "6", 16) == 16);
	CHECK(dstr_find_last_of(dstr, "7", 17) == 17);
	CHECK(dstr_find_last_of(dstr, "8", 18) == 18);
	CHECK(dstr_find_last_of(dstr, "9", 19) == 19);
	CHECK(dstr_find_last_of(dstr, "a", 0) == 0);
	CHECK(dstr_find_last_of(dstr, "b", 0) == DSTR_NPOS);
	CHECK(dstr_find_last_of(dstr, "c", 1) == DSTR_NPOS);
	CHECK(dstr_find_last_of(dstr, "d", 2) == DSTR_NPOS);
	CHECK(dstr_find_last_of(dstr, "e", 3) == DSTR_NPOS);
	CHECK(dstr_find_last_of(dstr, "f", 4) == DSTR_NPOS);
	CHECK(dstr_find_last_of(dstr, "g", 5) == DSTR_NPOS);
	CHECK(dstr_find_last_of(dstr, "h", 6) == DSTR_NPOS);
	CHECK(dstr_find_last_of(dstr, "i", 7) == DSTR_NPOS);
	CHECK(dstr_find_last_of(dstr, "j", 8) == DSTR_NPOS);
	CHECK(dstr_find_last_of(dstr, "0", 9) == DSTR_NPOS);
	CHECK(dstr_find_last_of(dstr, "1", 10) == DSTR_NPOS);
	CHECK(dstr_find_last_of(dstr, "2", 11) == DSTR_NPOS);
	CHECK(dstr_find_last_of(dstr, "3", 12) == DSTR_NPOS);
	CHECK(dstr_find_last_of(dstr, "4", 13) == DSTR_NPOS);
	CHECK(dstr_find_last_of(dstr, "5", 14) == DSTR_NPOS);
	CHECK(dstr_find_last_of(dstr, "6", 15) == DSTR_NPOS);
	CHECK(dstr_find_last_of(dstr, "7", 16) == DSTR_NPOS);
	CHECK(dstr_find_last_of(dstr, "8", 17) == DSTR_NPOS);
	CHECK(dstr_find_last_of(dstr, "9", 18) == DSTR_NPOS);
	CHECK(dstr_find_first_not_of(dstr, "a", 0) == 1);
	CHECK(dstr_find_first_not_of(dstr, "a", 1) == 1);
	CHECK(dstr_find_first_not_of(dstr, "b", 0) == 0);
	CHECK(dstr_find_first_not_of(dstr, "b", 1) == 2);
	CHECK(dstr_find_first_not_of(dstr, "c", 0) == 0);
	CHECK(dstr_find_first_not_of(dstr, "c", 2) == 3);
	CHECK(dstr_find_last_not_of(dstr, "9", DSTR_NPOS) == 18);
	CHECK(dstr_find_last_not_of(dstr, "9", 19) == 18);
	CHECK(dstr_find_last_not_of(dstr, "8", DSTR_NPOS) == 19);
	CHECK(dstr_find_last_not_of(dstr, "8", 18) == 17);
	CHECK(dstr_find_last_not_of(dstr, "7", DSTR_NPOS) == 19);
	CHECK(dstr_find_last_not_of(dstr, "7", 17) == 16);
	CHECK(dstr_find_first_of(dstr, "abcdefghij", 0) == 0);
	CHECK(dstr_find_first_of(dstr, "0123456789", 0) == 10);
	CHECK(dstr_find_last_of(dstr, "abcdefghij", DSTR_NPOS) == 9);
	CHECK(dstr_find_last_of(dstr, "0123456789", DSTR_NPOS) == 19);
	CHECK(dstr_find_first_of(dstr, "abcdefghij", 5) == 5);
	CHECK(dstr_find_first_of(dstr, "0123456789", 15) == 15);
	CHECK(dstr_find_last_of(dstr, "abcdefghij", 5) == 5);
	CHECK(dstr_find_last_of(dstr, "0123456789", 15) == 15);
	CHECK(dstr_find_first_not_of(dstr, "abcdefghij", 0) == 10);
	CHECK(dstr_find_first_not_of(dstr, "0123456789", 0) == 0);
	CHECK(dstr_find_last_not_of(dstr, "abcdefghij", DSTR_NPOS) == 19);
	CHECK(dstr_find_last_not_of(dstr, "0123456789", DSTR_NPOS) == 9);
	CHECK(dstr_find_first_not_of(dstr, "abcdefghij", 15) == 15);
	CHECK(dstr_find_first_not_of(dstr, "0123456789", 5) == 5);
	CHECK(dstr_find_last_not_of(dstr, "abcdefghij", 15) == 15);
	CHECK(dstr_find_last_not_of(dstr, "0123456789", 5) == 5);
	dstr_free(&dstr);

	dstr = dstr_from_cstr("abc123def456ghi789");
	CHECK(dstr_find_first_of(dstr, "abcdefghij", 0) == 0);
	CHECK(dstr_find_first_of(dstr, "abcdefghij", 3) == 6);
	CHECK(dstr_find_first_of(dstr, "abcdefghij", 8) == 8);
	CHECK(dstr_find_first_of(dstr, "0123456789", 0) == 3);
	CHECK(dstr_find_first_of(dstr, "0123456789", 6) == 9);
	CHECK(dstr_find_first_of(dstr, "0123456789", 11) == 11);
	CHECK(dstr_find_last_of(dstr, "0123456789", DSTR_NPOS) == 17);
	CHECK(dstr_find_last_of(dstr, "0123456789", 14) == 11);
	CHECK(dstr_find_last_of(dstr, "0123456789", 9) == 9);
	CHECK(dstr_find_last_of(dstr, "abcdefghij", DSTR_NPOS) == 14);
	CHECK(dstr_find_last_of(dstr, "abcdefghij", 11) == 8);
	CHECK(dstr_find_last_of(dstr, "abcdefghij", 6) == 6);
	CHECK(dstr_find_first_not_of(dstr, "abcdefghij", 0) == 3);
	CHECK(dstr_find_first_not_of(dstr, "abcdefghij", 5) == 5);
	CHECK(dstr_find_first_not_of(dstr, "abcdefghij", 6) == 9);
	CHECK(dstr_find_first_not_of(dstr, "0123456789", 0) == 0);
	CHECK(dstr_find_first_not_of(dstr, "0123456789", 2) == 2);
	CHECK(dstr_find_first_not_of(dstr, "0123456789", 3) == 6);
	CHECK(dstr_find_last_not_of(dstr, "abcdefghij", DSTR_NPOS) == 17);
	CHECK(dstr_find_last_not_of(dstr, "abcdefghij", 14) == 11);
	CHECK(dstr_find_last_not_of(dstr, "abcdefghij", 9) == 9);
	CHECK(dstr_find_last_not_of(dstr, "0123456789", DSTR_NPOS) == 14);
	CHECK(dstr_find_last_not_of(dstr, "0123456789", 11) == 8);
	CHECK(dstr_find_last_not_of(dstr, "0123456789", 6) == 6);
	dstr_free(&dstr);

	dstr = dstr_from_cstr("");
	CHECK(dstr_find_first_of(dstr, "abc123def456ghi789", 0) == DSTR_NPOS);
	CHECK(dstr_find_first_not_of(dstr, "abc123def456ghi789", 0) == DSTR_NPOS);
	CHECK(dstr_find_last_of(dstr, "abc123def456ghi789", DSTR_NPOS) == DSTR_NPOS);
	CHECK(dstr_find_last_not_of(dstr, "abc123def456ghi789", DSTR_NPOS) == DSTR_NPOS);
	dstr_free(&dstr);

	dstr = dstr_from_cstr("xxxxx");
	CHECK(dstr_find_first_of(dstr, "abc123def456ghi789", 0) == DSTR_NPOS);
	CHECK(dstr_find_first_not_of(dstr, "abc123def456ghi789", 0) == 0);
	CHECK(dstr_find_last_of(dstr, "abc123def456ghi789", DSTR_NPOS) == DSTR_NPOS);
	CHECK(dstr_find_last_not_of(dstr, "abc123def456ghi789", DSTR_NPOS) == 4);
	dstr_free(&dstr);

	dstr = dstr_from_cstr(a256);
	for (size_t i = 0; i < abc_len; i++) {
		CHECK(dstr_find_first_of(dstr, a256, i) == i);
		CHECK(dstr_find_first_not_of(dstr, a256, i) == DSTR_NPOS);
	}
	dstr_free(&dstr);

	dstr = dstr_from_cstr(a256);
	for (size_t i = abc_len - 1; i-- > 0;) {
		CHECK(dstr_find_last_of(dstr, a256, i) == i);
		CHECK(dstr_find_last_not_of(dstr, a256, i) == DSTR_NPOS);
	}
	dstr_free(&dstr);
	return true;
}

SIMPLE_TEST(dstring_starts_ends_with)
{
	dstr_t dstr = dstr_from_cstr(abc);
	CHECK(dstr_startswith_cstr(dstr, abc));
	CHECK(dstr_startswith_view(dstr, strview_from_cstr(abc)));
	CHECK(dstr_endswith_cstr(dstr, abc));
	CHECK(dstr_endswith_view(dstr, strview_from_cstr(abc)));
	CHECK(dstr_startswith_cstr(dstr, ""));
	CHECK(dstr_startswith_view(dstr, strview_from_cstr("")));
	CHECK(dstr_endswith_cstr(dstr, ""));
	CHECK(dstr_endswith_view(dstr, strview_from_cstr("")));
	dstr_free(&dstr);

	dstr = dstr_from_cstr(a256);
	CHECK(dstr_startswith_cstr(dstr, a256));
	CHECK(dstr_startswith_view(dstr, strview_from_cstr(a256)));
	CHECK(dstr_endswith_cstr(dstr, a256));
	CHECK(dstr_endswith_view(dstr, strview_from_cstr(a256)));
	CHECK(dstr_startswith_cstr(dstr, ""));
	CHECK(dstr_startswith_view(dstr, strview_from_cstr("")));
	CHECK(dstr_endswith_cstr(dstr, ""));
	CHECK(dstr_endswith_view(dstr, strview_from_cstr("")));
	dstr_free(&dstr);

	dstr = dstr_from_cstr("");
	CHECK(!dstr_startswith_cstr(dstr, "a"));
	CHECK(!dstr_startswith_view(dstr, strview_from_cstr("a")));
	CHECK(!dstr_endswith_cstr(dstr, "a"));
	CHECK(!dstr_endswith_view(dstr, strview_from_cstr("a")));
	CHECK(dstr_startswith_cstr(dstr, ""));
	CHECK(dstr_startswith_view(dstr, strview_from_cstr("")));
	CHECK(dstr_endswith_cstr(dstr, ""));
	CHECK(dstr_endswith_view(dstr, strview_from_cstr("")));
	dstr_free(&dstr);

	dstr = dstr_from_cstr("axb");
	CHECK(dstr_startswith_cstr(dstr, "a"));
	CHECK(dstr_startswith_view(dstr, strview_from_cstr("a")));
	CHECK(dstr_endswith_cstr(dstr, "b"));
	CHECK(dstr_endswith_view(dstr, strview_from_cstr("b")));
	CHECK(dstr_startswith_cstr(dstr, "ax"));
	CHECK(dstr_startswith_view(dstr, strview_from_cstr("ax")));
	CHECK(dstr_endswith_cstr(dstr, "xb"));
	CHECK(dstr_endswith_view(dstr, strview_from_cstr("xb")));
	CHECK(!dstr_startswith_cstr(dstr, "b"));
	CHECK(!dstr_startswith_view(dstr, strview_from_cstr("b")));
	CHECK(!dstr_endswith_cstr(dstr, "a"));
	CHECK(!dstr_endswith_view(dstr, strview_from_cstr("a")));
	CHECK(!dstr_startswith_cstr(dstr, "xb"));
	CHECK(!dstr_startswith_view(dstr, strview_from_cstr("xb")));
	CHECK(!dstr_endswith_cstr(dstr, "ax"));
	CHECK(!dstr_endswith_view(dstr, strview_from_cstr("ax")));
	dstr_free(&dstr);

	dstr = dstr_from_cstr(a256);
	dstr_append_cstr(&dstr, a256);
	CHECK(dstr_startswith_cstr(dstr, a256));
	CHECK(dstr_startswith_view(dstr, strview_from_cstr(a256)));
	CHECK(dstr_endswith_cstr(dstr, a256));
	CHECK(dstr_endswith_view(dstr, strview_from_cstr(a256)));
	dstr_free(&dstr);

	dstr = dstr_from_cstr(abc);
	dstr_append_char(&dstr, '-');
	dstr_append_cstr(&dstr, abc);
	CHECK(dstr_startswith_cstr(dstr, abc));
	CHECK(dstr_startswith_view(dstr, strview_from_cstr(abc)));
	CHECK(dstr_endswith_cstr(dstr, abc));
	CHECK(dstr_endswith_view(dstr, strview_from_cstr(abc)));
	CHECK(!dstr_startswith_cstr(dstr, "-"));
	CHECK(!dstr_startswith_view(dstr, strview_from_cstr("-")));
	CHECK(!dstr_endswith_cstr(dstr, "-"));
	CHECK(!dstr_endswith_view(dstr, strview_from_cstr("-")));
	dstr_t dstr2 = dstr_substring_copy(dstr, abc_len, DSTR_NPOS);
	CHECK(dstr_startswith_cstr(dstr2, "-"));
	CHECK(dstr_startswith_view(dstr2, strview_from_cstr("-")));
	CHECK(!dstr_startswith_cstr(dstr2, abc));
	CHECK(!dstr_startswith_view(dstr2, strview_from_cstr(abc)));
	dstr_free(&dstr2);
	dstr2 = dstr_substring_copy(dstr, abc_len + 1, DSTR_NPOS);
	CHECK(dstr_startswith_cstr(dstr2, abc));
	CHECK(dstr_startswith_view(dstr2, strview_from_cstr(abc)));
	CHECK(!dstr_startswith_cstr(dstr2, "-"));
	CHECK(!dstr_startswith_view(dstr2, strview_from_cstr("-")));
	dstr_free(&dstr2);
	dstr2 = dstr_substring_copy(dstr, 0, abc_len + 1);
	CHECK(dstr_endswith_cstr(dstr2, "-"));
	CHECK(dstr_endswith_view(dstr2, strview_from_cstr("-")));
	CHECK(!dstr_endswith_cstr(dstr2, abc));
	CHECK(!dstr_endswith_view(dstr2, strview_from_cstr(abc)));
	dstr_free(&dstr2);
	dstr2 = dstr_substring_copy(dstr, 0, abc_len);
	CHECK(dstr_endswith_cstr(dstr2, abc));
	CHECK(dstr_endswith_view(dstr2, strview_from_cstr(abc)));
	CHECK(!dstr_endswith_cstr(dstr2, "-"));
	CHECK(!dstr_endswith_view(dstr2, strview_from_cstr("-")));
	dstr_free(&dstr2);
	dstr_free(&dstr);

	dstr = dstr_from_cstr(a256);
	dstr2 = dstr_from_cstr(a256);
	CHECK(dstr_startswith_dstr(dstr, dstr2));
	CHECK(dstr_endswith_dstr(dstr, dstr2));
	dstr_free(&dstr2);
	dstr2 = dstr_from_cstr("***");
	CHECK(!dstr_startswith_dstr(dstr, dstr2));
	CHECK(!dstr_endswith_dstr(dstr, dstr2));
	dstr_free(&dstr2);
	dstr_free(&dstr);
	return true;
}

SIMPLE_TEST(dstring_split)
{
	const struct {
		const char *str;
		char c;
		bool reverse;
		size_t max;
		size_t count;
		const char **results;
	} test_cases[] = {
#define TEST_CASE(reverse, str, c, max, count, ...)			\
		{str, c, reverse, max, count, (const char *[]){__VA_ARGS__}}

		TEST_CASE(false, "", 'x', SIZE_MAX, 1, ""),
		TEST_CASE(false, "x", 'x', SIZE_MAX, 2, "", ""),
		TEST_CASE(false, "xx", 'x', SIZE_MAX, 3, "", "", ""),
		TEST_CASE(false, "axax", 'x', SIZE_MAX, 3, "a", "a", ""),
		TEST_CASE(false, "axaxa", 'x', SIZE_MAX, 3, "a", "a", "a"),
		TEST_CASE(false, "xaxa", 'x', SIZE_MAX, 3, "", "a", "a"),
		TEST_CASE(false, "xax", 'x', SIZE_MAX, 3, "", "a", ""),
		TEST_CASE(false, "", 'x', 0, 0, NULL),
		TEST_CASE(false, "x", 'x', 1, 1, ""),
		TEST_CASE(false, "xx", 'x', 2, 2, "", ""),
		TEST_CASE(false, "xx", 'x', 1, 1, ""),
		TEST_CASE(false, "axax", 'x', 2, 2, "a", "a"),
		TEST_CASE(false, "axax", 'x', 1, 1, "a"),
		TEST_CASE(false, "axaxa", 'x', 2, 2, "a", "a"),
		TEST_CASE(false, "axaxa", 'x', 1, 1, "a"),
		TEST_CASE(false, "xaxa", 'x', 2, 2, "", "a"),
		TEST_CASE(false, "xaxa", 'x', 1, 1, ""),
		TEST_CASE(false, "xax", 'x', 2, 2, "", "a"),
		TEST_CASE(false, "xax", 'x', 1, 1, ""),
		TEST_CASE(true, "", 'x', SIZE_MAX, 1, ""),
		TEST_CASE(true, "x", 'x', SIZE_MAX, 2, "", ""),
		TEST_CASE(true, "xx", 'x', SIZE_MAX, 3, "", "", ""),
		TEST_CASE(true, "axax", 'x', SIZE_MAX, 3, "", "a", "a"),
		TEST_CASE(true, "axaxa", 'x', SIZE_MAX, 3, "a", "a", "a"),
		TEST_CASE(true, "xaxa", 'x', SIZE_MAX, 3, "a", "a", ""),
		TEST_CASE(true, "xax", 'x', SIZE_MAX, 3, "", "a", ""),
		TEST_CASE(true, "", 'x', 0, 0, NULL),
		TEST_CASE(true, "x", 'x', 1, 1, ""),
		TEST_CASE(true, "xx", 'x', 2, 2, "", ""),
		TEST_CASE(true, "xx", 'x', 1, 1, ""),
		TEST_CASE(true, "axax", 'x', 2, 2, "", "a"),
		TEST_CASE(true, "axax", 'x', 1, 1, ""),
		TEST_CASE(true, "axaxa", 'x', 2, 2, "a", "a"),
		TEST_CASE(true, "axaxa", 'x', 1, 1, "a"),
		TEST_CASE(true, "xaxa", 'x', 2, 2, "a", "a"),
		TEST_CASE(true, "xaxa", 'x', 1, 1, "a"),
		TEST_CASE(true, "xax", 'x', 2, 2, "", "a"),
		TEST_CASE(true, "xax", 'x', 1, 1, ""),
#undef TEST_CASE
	};

	for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++) {
		const char *str = test_cases[i].str;
		char c = test_cases[i].c;
		bool rev = test_cases[i].reverse;
		size_t max = test_cases[i].max;
		size_t count = test_cases[i].count;
		const char **results = test_cases[i].results;
		// test_log("%s %c %zu %zu\n", str, c, max, count);
		dstr_t dstr = dstr_from_cstr(str);
		struct dstr_list dstr_list = (rev ? dstr_rsplit : dstr_split)(dstr, c, max);
		struct strview_list strview_list = (rev ? dstr_rsplit_views : dstr_split_views)(dstr, c, max);
		CHECK(dstr_list.count == count);
		CHECK(strview_list.count == count);
		for (size_t j = 0; j < count; j++) {
			CHECK(dstr_equals_cstr(dstr_list.strings[j], results[j]));
			CHECK(strview_equal_cstr(strview_list.strings[j], results[j]));
		}
		dstr_list_free(&dstr_list);
		strview_list_free(&strview_list);
		dstr_free(&dstr);
	}
	return true;
}

SIMPLE_TEST(dstring_reserve)
{
	dstr_t dstr = dstr_new();
	dstr_reserve(&dstr, UINT8_MAX + 1);
	CHECK(sanity_check(dstr));
	dstr_reserve(&dstr, UINT16_MAX + 1);
	CHECK(sanity_check(dstr));
	dstr_free(&dstr);
	dstr = dstr_new();
	dstr_reserve(&dstr, UINT16_MAX + 1);
	CHECK(sanity_check(dstr));
	dstr_free(&dstr);
	return true;
}

#if _FORTIFY_SOURCE >= 1 && (defined(HAVE_BUILTIN_DYNAMIC_OBJECT_SIZE) || defined(HAVE_BUILTIN_OBJECT_SIZE))

NEGATIVE_SIMPLE_TEST(fortify_from_chars)
{
	char chars[16] = {0};
	dstr_t dstr = dstr_from_chars(chars, sizeof(chars) + 1);
	dstr_free(&dstr);
	return true;
}

NEGATIVE_SIMPLE_TEST(fortify_append_chars)
{
	dstr_t dstr = dstr_new();
	char chars[16] = {0};
	dstr_append_chars(&dstr, chars, sizeof(chars) + 1);
	return true;
}

NEGATIVE_SIMPLE_TEST(fortify_insert_chars)
{
	dstr_t dstr = dstr_new();
	char chars[16] = {0};
	dstr_insert_chars(&dstr, 0, chars, sizeof(chars) + 1);
	return true;
}

NEGATIVE_SIMPLE_TEST(fortify_replace_chars)
{
	dstr_t dstr = dstr_new();
	char chars[16] = {0};
	dstr_replace_chars(&dstr, 0, 0, chars, sizeof(chars) + 1);
	return true;
}

NEGATIVE_SIMPLE_TEST(fortify_strview_from_chars)
{
	char chars[16] = {0};
	strview_from_chars(chars, sizeof(chars) + 1);
	return true;
}

#endif
