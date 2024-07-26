#include <stdlib.h>
#include <string.h>
#include "dstring.h"
#include "stringview.h"
#include "testing.h"

static const char abc[] = "abcdefghijklmnopqrstuvwxyz";
static const char a256[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789`~!@#$%^&*()-=_+[]\\;',./{}|:\"<>?abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789`~!@#$%^&*()-=_+[]\\;',./{}|:\"<>?abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789`~!@#$";
#define STRLEN(s) (sizeof(s) - 1)
static const size_t abc_len = STRLEN(abc);
static const size_t a256_len = STRLEN(a256);
_Static_assert(STRLEN(abc) == 26, "");
_Static_assert(STRLEN(a256) == 256, "");
#undef STRLEN

static int sign(int x)
{
	return x < 0 ? -1 : (x > 0 ? 1 : 0);
}

SIMPLE_TEST(strview_compare)
{
	struct strview view = strview_from_cstr(abc);
	struct strview view2 = strview_from_chars(abc, abc_len);
	CHECK(strview_equal_cstr(view, abc));
	CHECK(strview_equal_cstr(view2, abc));
	CHECK(strview_equal(view, view2));

	view = strview_from_cstr(a256);
	view2 = strview_from_chars(a256, a256_len);
	CHECK(strview_equal_cstr(view, a256));
	CHECK(strview_equal_cstr(view2, a256));
	CHECK(strview_equal(view, view2));

	view = strview_from_cstr(a256);
	char *cstr = strview_to_cstr(view);
	CHECK(strcmp(cstr, a256) == 0);
	free(cstr);

	view = strview_from_cstr("abcdef");
	CHECK(strview_equal_cstr(strview_substring(view, 0, 0), ""));
	CHECK(strview_equal_cstr(strview_substring(view, 0, 1), "a"));
	CHECK(strview_equal_cstr(strview_substring(view, 0, 3), "abc"));
	CHECK(strview_equal_cstr(strview_substring(view, 0, 6), "abcdef"));
	CHECK(strview_equal_cstr(strview_substring(view, 1, 5), "bcdef"));
	CHECK(strview_equal_cstr(strview_substring(view, 2, 4), "cdef"));
	CHECK(strview_equal_cstr(strview_substring(view, 2, 2), "cd"));
	CHECK(strview_equal_cstr(strview_substring(view, 5, 1), "f"));
	CHECK(strview_equal_cstr(strview_substring(view, 0, STRVIEW_NPOS), "abcdef"));
	view = strview_from_cstr("");
	CHECK(strview_equal_cstr(strview_substring(view, 0, 0), ""));
	CHECK(strview_equal_cstr(strview_substring(view, 0, STRVIEW_NPOS), ""));

	view = strview_from_cstr("abcdef");
	CHECK(strview_equal_cstr(strview_narrow(view, 0, 0), "abcdef"));
	CHECK(strview_equal_cstr(strview_narrow(view, 1, 0), "bcdef"));
	CHECK(strview_equal_cstr(strview_narrow(view, 3, 0), "def"));
	CHECK(strview_equal_cstr(strview_narrow(view, 0, 1), "abcde"));
	CHECK(strview_equal_cstr(strview_narrow(view, 0, 3), "abc"));
	CHECK(strview_equal_cstr(strview_narrow(view, 1, 1), "bcde"));
	CHECK(strview_equal_cstr(strview_narrow(view, 2, 2), "cd"));
	CHECK(strview_equal_cstr(strview_narrow(view, 3, 3), ""));

	view = strview_from_cstr("abc");
	const char *str = "abc";
	int r = sign(strview_compare_cstr(view, str));
	CHECK(r == 0);
	CHECK(sign(strview_compare(view, strview_from_cstr(str))) == r);
	view2 = strview_from_cstr(str);
	CHECK(sign(strview_compare(view, view2)) == r);

	view = strview_from_cstr("");
	str = "";
	r = sign(strview_compare_cstr(view, str));
	CHECK(r == 0);
	CHECK(sign(strview_compare(view, strview_from_cstr(str))) == r);
	view2 = strview_from_cstr(str);
	CHECK(sign(strview_compare(view, view2)) == r);

	view = strview_from_cstr("abc");
	str = "";
	r = sign(strview_compare_cstr(view, str));
	CHECK(r > 0);
	CHECK(sign(strview_compare(view, strview_from_cstr(str))) == r);
	view2 = strview_from_cstr(str);
	CHECK(sign(strview_compare(view, view2)) == r);

	view = strview_from_cstr("");
	str = "abc";
	r = sign(strview_compare_cstr(view, str));
	CHECK(r < 0);
	CHECK(sign(strview_compare(view, strview_from_cstr(str))) == r);
	view2 = strview_from_cstr(str);
	CHECK(sign(strview_compare(view, view2)) == r);

	view = strview_from_cstr("abc");
	str = "def";
	r = sign(strview_compare_cstr(view, str));
	CHECK(r < 0);
	CHECK(sign(strview_compare(view, strview_from_cstr(str))) == r);
	view2 = strview_from_cstr(str);
	CHECK(sign(strview_compare(view, view2)) == r);

	view = strview_from_cstr("def");
	str = "abc";
	r = sign(strview_compare_cstr(view, str));
	CHECK(r > 0);
	CHECK(sign(strview_compare(view, strview_from_cstr(str))) == r);
	view2 = strview_from_cstr(str);
	CHECK(sign(strview_compare(view, view2)) == r);

	view = strview_from_cstr("abc");
	str = "abcd";
	r = sign(strview_compare_cstr(view, str));
	CHECK(r < 0);
	CHECK(sign(strview_compare(view, strview_from_cstr(str))) == r);
	view2 = strview_from_cstr(str);
	CHECK(sign(strview_compare(view, view2)) == r);

	view = strview_from_cstr("abc");
	str = "ab";
	r = sign(strview_compare_cstr(view, str));
	CHECK(r > 0);
	CHECK(sign(strview_compare(view, strview_from_cstr(str))) == r);
	view2 = strview_from_cstr(str);
	CHECK(sign(strview_compare(view, view2)) == r);

	view = strview_from_cstr("abc");
	str = "abd";
	r = sign(strview_compare_cstr(view, str));
	CHECK(r < 0);
	CHECK(sign(strview_compare(view, strview_from_cstr(str))) == r);
	view2 = strview_from_cstr(str);
	CHECK(sign(strview_compare(view, view2)) == r);

	view = strview_from_cstr("abc");
	str = "abb";
	r = sign(strview_compare_cstr(view, str));
	CHECK(r > 0);
	CHECK(sign(strview_compare(view, strview_from_cstr(str))) == r);
	view2 = strview_from_cstr(str);
	CHECK(sign(strview_compare(view, view2)) == r);

	view = strview_from_cstr(abc);
	str = abc;
	r = sign(strview_compare_cstr(view, str));
	CHECK(r == 0);
	CHECK(sign(strview_compare(view, strview_from_cstr(str))) == r);
	view2 = strview_from_cstr(str);
	CHECK(sign(strview_compare(view, view2)) == r);

	view = strview_from_cstr(a256);
	str = a256;
	r = sign(strview_compare_cstr(view, str));
	CHECK(r == 0);
	CHECK(sign(strview_compare(view, strview_from_cstr(str))) == r);
	view2 = strview_from_cstr(str);
	CHECK(sign(strview_compare(view, view2)) == r);

	view = strview_from_cstr(abc);
	str = a256;
	r = sign(strview_compare_cstr(view, str));
	CHECK(r < 0);
	CHECK(sign(strview_compare(view, strview_from_cstr(str))) == r);
	view2 = strview_from_cstr(str);
	CHECK(sign(strview_compare(view, view2)) == r);

	view = strview_from_cstr(a256);
	str = abc;
	r = sign(strview_compare_cstr(view, str));
	CHECK(r > 0);
	CHECK(sign(strview_compare(view, strview_from_cstr(str))) == r);
	view2 = strview_from_cstr(str);
	CHECK(sign(strview_compare(view, view2)) == r);

	view = strview_from_cstr("abc");
	str = "abc";
	r = strview_equal_cstr(view, str);
	CHECK(r);
	CHECK(strview_equal(view, strview_from_cstr(str)) == r);
	view2 = strview_from_cstr(str);
	CHECK(strview_equal(view, view2) == r);

	view = strview_from_cstr("abc");
	str = "ab";
	r = strview_equal_cstr(view, str);
	CHECK(!r);
	CHECK(strview_equal(view, strview_from_cstr(str)) == r);
	view2 = strview_from_cstr(str);
	CHECK(strview_equal(view, view2) == r);

	view = strview_from_cstr("abc");
	str = "abcd";
	r = strview_equal_cstr(view, str);
	CHECK(!r);
	CHECK(strview_equal(view, strview_from_cstr(str)) == r);
	view2 = strview_from_cstr(str);
	CHECK(strview_equal(view, view2) == r);

	view = strview_from_cstr("abc");
	str = "abd";
	r = strview_equal_cstr(view, str);
	CHECK(!r);
	CHECK(strview_equal(view, strview_from_cstr(str)) == r);
	view2 = strview_from_cstr(str);
	CHECK(strview_equal(view, view2) == r);

	view = strview_from_cstr("");
	str = "";
	r = strview_equal_cstr(view, str);
	CHECK(r);
	CHECK(strview_equal(view, strview_from_cstr(str)) == r);
	view2 = strview_from_cstr(str);
	CHECK(strview_equal(view, view2) == r);

	view = strview_from_cstr("abc");
	str = "";
	r = strview_equal_cstr(view, str);
	CHECK(!r);
	CHECK(strview_equal(view, strview_from_cstr(str)) == r);
	view2 = strview_from_cstr(str);
	CHECK(strview_equal(view, view2) == r);

	view = strview_from_cstr("");
	str = "abc";
	r = strview_equal_cstr(view, str);
	CHECK(!r);
	CHECK(strview_equal(view, strview_from_cstr(str)) == r);
	view2 = strview_from_cstr(str);
	CHECK(strview_equal(view, view2) == r);

	view = strview_from_cstr(abc);
	str = abc;
	r = strview_equal_cstr(view, str);
	CHECK(r);
	CHECK(strview_equal(view, strview_from_cstr(str)) == r);
	view2 = strview_from_cstr(str);
	CHECK(strview_equal(view, view2) == r);

	view = strview_from_cstr(a256);
	str = a256;
	r = strview_equal_cstr(view, str);
	CHECK(r);
	CHECK(strview_equal(view, strview_from_cstr(str)) == r);
	view2 = strview_from_cstr(str);
	CHECK(strview_equal(view, view2) == r);

	view = strview_from_cstr(abc);
	str = a256;
	r = strview_equal_cstr(view, str);
	CHECK(!r);
	CHECK(strview_equal(view, strview_from_cstr(str)) == r);
	view2 = strview_from_cstr(str);
	CHECK(strview_equal(view, view2) == r);

	view = strview_from_cstr(a256);
	str = abc;
	r = strview_equal_cstr(view, str);
	CHECK(!r);
	CHECK(strview_equal(view, strview_from_cstr(str)) == r);
	view2 = strview_from_cstr(str);
	CHECK(strview_equal(view, view2) == r);
	return true;
}

SIMPLE_TEST(strview_find)
{
	static const struct {
		const char *haystack;
		const char *needle;
		size_t s;
		size_t result;
		bool reverse;
	} test_cases[] = {
		{"abc", "abc", 0, 0, false},
		{"abc", "ab", 0, 0, false},
		{"abc", "a", 0, 0, false},
		{"abc", "", 0, 0, false},
		{"abc", "c", 0, 2, false},
		{"abcabc", "abc", 1, 3, false},
		{"abcabcabc", "abc", 3, 3, false},
		{"abcabcabc", "abc", 4, 6, false},
		{"abcabcabc", "abc", 7, STRVIEW_NPOS, false},
		{"", "", 0, 0, false},
		{"", "a", 0, STRVIEW_NPOS, false},
		{"abc", "x", 0, STRVIEW_NPOS, false},
		{"abc", "abcd", 0, STRVIEW_NPOS, false},
		{"abc", "abc", STRVIEW_NPOS, 0, true},
		{"abc", "ab", STRVIEW_NPOS, 0, true},
		{"abc", "a", STRVIEW_NPOS, 0, true},
		{"abc", "", STRVIEW_NPOS, 3, true},
		{"abc", "c", 1, STRVIEW_NPOS, true},
		{"abc", "c", STRVIEW_NPOS, 2, true},
		{"abcabc", "abc", STRVIEW_NPOS, 3, true},
		{"abcabc", "abc", 2, 0, true},
		{"abcabcabc", "abc", 3, 3, true},
		{"abcabcabc", "abc", 6, 6, true},
		{"abcabcabc", "abc", 0, 0, true},
		{"abcabcabc", "abc", STRVIEW_NPOS, 6, true},
		{"", "", STRVIEW_NPOS, 0, true},
		{"", "a", STRVIEW_NPOS, STRVIEW_NPOS, true},
		{"abc", "x", STRVIEW_NPOS, STRVIEW_NPOS, true},
		{"abc", "abcd", STRVIEW_NPOS, STRVIEW_NPOS, true},
	};

	for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++) {
		const char *haystack = test_cases[i].haystack;
		const char *needle = test_cases[i].needle;
		size_t s = test_cases[i].s;
		size_t result = test_cases[i].result;
		bool rev = test_cases[i].reverse;
		// test_log("%s %s %zu %zu %d\n", haystack, needle, s, result, rev);
		struct strview haystack_view = strview_from_cstr(haystack);
		size_t p = (rev ? strview_rfind_cstr : strview_find_cstr)(haystack_view, needle, s);
		CHECK(p == result);
		CHECK((rev ? strview_rfind : strview_find)(haystack_view, strview_from_cstr(needle), s) == p);
	}

	struct strview view = strview_from_cstr("abcdefghij0123456789");
	CHECK(strview_find_first_of(view, "", 0) == STRVIEW_NPOS);
	CHECK(strview_find_first_of(view, "a", 0) == 0);
	CHECK(strview_find_first_of(view, "b", 0) == 1);
	CHECK(strview_find_first_of(view, "c", 0) == 2);
	CHECK(strview_find_first_of(view, "d", 0) == 3);
	CHECK(strview_find_first_of(view, "e", 0) == 4);
	CHECK(strview_find_first_of(view, "f", 0) == 5);
	CHECK(strview_find_first_of(view, "g", 0) == 6);
	CHECK(strview_find_first_of(view, "h", 0) == 7);
	CHECK(strview_find_first_of(view, "i", 0) == 8);
	CHECK(strview_find_first_of(view, "j", 0) == 9);
	CHECK(strview_find_first_of(view, "0", 0) == 10);
	CHECK(strview_find_first_of(view, "1", 0) == 11);
	CHECK(strview_find_first_of(view, "2", 0) == 12);
	CHECK(strview_find_first_of(view, "3", 0) == 13);
	CHECK(strview_find_first_of(view, "4", 0) == 14);
	CHECK(strview_find_first_of(view, "5", 0) == 15);
	CHECK(strview_find_first_of(view, "6", 0) == 16);
	CHECK(strview_find_first_of(view, "7", 0) == 17);
	CHECK(strview_find_first_of(view, "8", 0) == 18);
	CHECK(strview_find_first_of(view, "9", 0) == 19);
	CHECK(strview_find_first_of(view, "a", 0) == 0);
	CHECK(strview_find_first_of(view, "b", 1) == 1);
	CHECK(strview_find_first_of(view, "c", 2) == 2);
	CHECK(strview_find_first_of(view, "d", 3) == 3);
	CHECK(strview_find_first_of(view, "e", 4) == 4);
	CHECK(strview_find_first_of(view, "f", 5) == 5);
	CHECK(strview_find_first_of(view, "g", 6) == 6);
	CHECK(strview_find_first_of(view, "h", 7) == 7);
	CHECK(strview_find_first_of(view, "i", 8) == 8);
	CHECK(strview_find_first_of(view, "j", 9) == 9);
	CHECK(strview_find_first_of(view, "0", 10) == 10);
	CHECK(strview_find_first_of(view, "1", 11) == 11);
	CHECK(strview_find_first_of(view, "2", 12) == 12);
	CHECK(strview_find_first_of(view, "3", 13) == 13);
	CHECK(strview_find_first_of(view, "4", 14) == 14);
	CHECK(strview_find_first_of(view, "5", 15) == 15);
	CHECK(strview_find_first_of(view, "6", 16) == 16);
	CHECK(strview_find_first_of(view, "7", 17) == 17);
	CHECK(strview_find_first_of(view, "8", 18) == 18);
	CHECK(strview_find_first_of(view, "9", 19) == 19);
	CHECK(strview_find_first_of(view, "a", 1) == STRVIEW_NPOS);
	CHECK(strview_find_first_of(view, "b", 2) == STRVIEW_NPOS);
	CHECK(strview_find_first_of(view, "c", 3) == STRVIEW_NPOS);
	CHECK(strview_find_first_of(view, "d", 4) == STRVIEW_NPOS);
	CHECK(strview_find_first_of(view, "e", 5) == STRVIEW_NPOS);
	CHECK(strview_find_first_of(view, "f", 6) == STRVIEW_NPOS);
	CHECK(strview_find_first_of(view, "g", 7) == STRVIEW_NPOS);
	CHECK(strview_find_first_of(view, "h", 8) == STRVIEW_NPOS);
	CHECK(strview_find_first_of(view, "i", 9) == STRVIEW_NPOS);
	CHECK(strview_find_first_of(view, "j", 10) == STRVIEW_NPOS);
	CHECK(strview_find_first_of(view, "0", 11) == STRVIEW_NPOS);
	CHECK(strview_find_first_of(view, "1", 12) == STRVIEW_NPOS);
	CHECK(strview_find_first_of(view, "2", 13) == STRVIEW_NPOS);
	CHECK(strview_find_first_of(view, "3", 14) == STRVIEW_NPOS);
	CHECK(strview_find_first_of(view, "4", 15) == STRVIEW_NPOS);
	CHECK(strview_find_first_of(view, "5", 16) == STRVIEW_NPOS);
	CHECK(strview_find_first_of(view, "6", 17) == STRVIEW_NPOS);
	CHECK(strview_find_first_of(view, "7", 18) == STRVIEW_NPOS);
	CHECK(strview_find_first_of(view, "8", 19) == STRVIEW_NPOS);
	CHECK(strview_find_first_of(view, "9", 20) == STRVIEW_NPOS);
	CHECK(strview_find_last_of(view, "", STRVIEW_NPOS) == STRVIEW_NPOS);
	CHECK(strview_find_last_of(view, "a", STRVIEW_NPOS) == 0);
	CHECK(strview_find_last_of(view, "b", STRVIEW_NPOS) == 1);
	CHECK(strview_find_last_of(view, "c", STRVIEW_NPOS) == 2);
	CHECK(strview_find_last_of(view, "d", STRVIEW_NPOS) == 3);
	CHECK(strview_find_last_of(view, "e", STRVIEW_NPOS) == 4);
	CHECK(strview_find_last_of(view, "f", STRVIEW_NPOS) == 5);
	CHECK(strview_find_last_of(view, "g", STRVIEW_NPOS) == 6);
	CHECK(strview_find_last_of(view, "h", STRVIEW_NPOS) == 7);
	CHECK(strview_find_last_of(view, "i", STRVIEW_NPOS) == 8);
	CHECK(strview_find_last_of(view, "j", STRVIEW_NPOS) == 9);
	CHECK(strview_find_last_of(view, "0", STRVIEW_NPOS) == 10);
	CHECK(strview_find_last_of(view, "1", STRVIEW_NPOS) == 11);
	CHECK(strview_find_last_of(view, "2", STRVIEW_NPOS) == 12);
	CHECK(strview_find_last_of(view, "3", STRVIEW_NPOS) == 13);
	CHECK(strview_find_last_of(view, "4", STRVIEW_NPOS) == 14);
	CHECK(strview_find_last_of(view, "5", STRVIEW_NPOS) == 15);
	CHECK(strview_find_last_of(view, "6", STRVIEW_NPOS) == 16);
	CHECK(strview_find_last_of(view, "7", STRVIEW_NPOS) == 17);
	CHECK(strview_find_last_of(view, "8", STRVIEW_NPOS) == 18);
	CHECK(strview_find_last_of(view, "9", STRVIEW_NPOS) == 19);
	CHECK(strview_find_last_of(view, "a", 0) == 0);
	CHECK(strview_find_last_of(view, "b", 1) == 1);
	CHECK(strview_find_last_of(view, "c", 2) == 2);
	CHECK(strview_find_last_of(view, "d", 3) == 3);
	CHECK(strview_find_last_of(view, "e", 4) == 4);
	CHECK(strview_find_last_of(view, "f", 5) == 5);
	CHECK(strview_find_last_of(view, "g", 6) == 6);
	CHECK(strview_find_last_of(view, "h", 7) == 7);
	CHECK(strview_find_last_of(view, "i", 8) == 8);
	CHECK(strview_find_last_of(view, "j", 9) == 9);
	CHECK(strview_find_last_of(view, "0", 10) == 10);
	CHECK(strview_find_last_of(view, "1", 11) == 11);
	CHECK(strview_find_last_of(view, "2", 12) == 12);
	CHECK(strview_find_last_of(view, "3", 13) == 13);
	CHECK(strview_find_last_of(view, "4", 14) == 14);
	CHECK(strview_find_last_of(view, "5", 15) == 15);
	CHECK(strview_find_last_of(view, "6", 16) == 16);
	CHECK(strview_find_last_of(view, "7", 17) == 17);
	CHECK(strview_find_last_of(view, "8", 18) == 18);
	CHECK(strview_find_last_of(view, "9", 19) == 19);
	CHECK(strview_find_last_of(view, "a", 0) == 0);
	CHECK(strview_find_last_of(view, "b", 0) == STRVIEW_NPOS);
	CHECK(strview_find_last_of(view, "c", 1) == STRVIEW_NPOS);
	CHECK(strview_find_last_of(view, "d", 2) == STRVIEW_NPOS);
	CHECK(strview_find_last_of(view, "e", 3) == STRVIEW_NPOS);
	CHECK(strview_find_last_of(view, "f", 4) == STRVIEW_NPOS);
	CHECK(strview_find_last_of(view, "g", 5) == STRVIEW_NPOS);
	CHECK(strview_find_last_of(view, "h", 6) == STRVIEW_NPOS);
	CHECK(strview_find_last_of(view, "i", 7) == STRVIEW_NPOS);
	CHECK(strview_find_last_of(view, "j", 8) == STRVIEW_NPOS);
	CHECK(strview_find_last_of(view, "0", 9) == STRVIEW_NPOS);
	CHECK(strview_find_last_of(view, "1", 10) == STRVIEW_NPOS);
	CHECK(strview_find_last_of(view, "2", 11) == STRVIEW_NPOS);
	CHECK(strview_find_last_of(view, "3", 12) == STRVIEW_NPOS);
	CHECK(strview_find_last_of(view, "4", 13) == STRVIEW_NPOS);
	CHECK(strview_find_last_of(view, "5", 14) == STRVIEW_NPOS);
	CHECK(strview_find_last_of(view, "6", 15) == STRVIEW_NPOS);
	CHECK(strview_find_last_of(view, "7", 16) == STRVIEW_NPOS);
	CHECK(strview_find_last_of(view, "8", 17) == STRVIEW_NPOS);
	CHECK(strview_find_last_of(view, "9", 18) == STRVIEW_NPOS);
	CHECK(strview_find_first_not_of(view, "a", 0) == 1);
	CHECK(strview_find_first_not_of(view, "a", 1) == 1);
	CHECK(strview_find_first_not_of(view, "b", 0) == 0);
	CHECK(strview_find_first_not_of(view, "b", 1) == 2);
	CHECK(strview_find_first_not_of(view, "c", 0) == 0);
	CHECK(strview_find_first_not_of(view, "c", 2) == 3);
	CHECK(strview_find_last_not_of(view, "9", STRVIEW_NPOS) == 18);
	CHECK(strview_find_last_not_of(view, "9", 19) == 18);
	CHECK(strview_find_last_not_of(view, "8", STRVIEW_NPOS) == 19);
	CHECK(strview_find_last_not_of(view, "8", 18) == 17);
	CHECK(strview_find_last_not_of(view, "7", STRVIEW_NPOS) == 19);
	CHECK(strview_find_last_not_of(view, "7", 17) == 16);
	CHECK(strview_find_first_of(view, "abcdefghij", 0) == 0);
	CHECK(strview_find_first_of(view, "0123456789", 0) == 10);
	CHECK(strview_find_last_of(view, "abcdefghij", STRVIEW_NPOS) == 9);
	CHECK(strview_find_last_of(view, "0123456789", STRVIEW_NPOS) == 19);
	CHECK(strview_find_first_of(view, "abcdefghij", 5) == 5);
	CHECK(strview_find_first_of(view, "0123456789", 15) == 15);
	CHECK(strview_find_last_of(view, "abcdefghij", 5) == 5);
	CHECK(strview_find_last_of(view, "0123456789", 15) == 15);
	CHECK(strview_find_first_not_of(view, "abcdefghij", 0) == 10);
	CHECK(strview_find_first_not_of(view, "0123456789", 0) == 0);
	CHECK(strview_find_last_not_of(view, "abcdefghij", STRVIEW_NPOS) == 19);
	CHECK(strview_find_last_not_of(view, "0123456789", STRVIEW_NPOS) == 9);
	CHECK(strview_find_first_not_of(view, "abcdefghij", 15) == 15);
	CHECK(strview_find_first_not_of(view, "0123456789", 5) == 5);
	CHECK(strview_find_last_not_of(view, "abcdefghij", 15) == 15);
	CHECK(strview_find_last_not_of(view, "0123456789", 5) == 5);

	view = strview_from_cstr("abc123def456ghi789");
	CHECK(strview_find_first_of(view, "abcdefghij", 0) == 0);
	CHECK(strview_find_first_of(view, "abcdefghij", 3) == 6);
	CHECK(strview_find_first_of(view, "abcdefghij", 8) == 8);
	CHECK(strview_find_first_of(view, "0123456789", 0) == 3);
	CHECK(strview_find_first_of(view, "0123456789", 6) == 9);
	CHECK(strview_find_first_of(view, "0123456789", 11) == 11);
	CHECK(strview_find_last_of(view, "0123456789", STRVIEW_NPOS) == 17);
	CHECK(strview_find_last_of(view, "0123456789", 14) == 11);
	CHECK(strview_find_last_of(view, "0123456789", 9) == 9);
	CHECK(strview_find_last_of(view, "abcdefghij", STRVIEW_NPOS) == 14);
	CHECK(strview_find_last_of(view, "abcdefghij", 11) == 8);
	CHECK(strview_find_last_of(view, "abcdefghij", 6) == 6);
	CHECK(strview_find_first_not_of(view, "abcdefghij", 0) == 3);
	CHECK(strview_find_first_not_of(view, "abcdefghij", 5) == 5);
	CHECK(strview_find_first_not_of(view, "abcdefghij", 6) == 9);
	CHECK(strview_find_first_not_of(view, "0123456789", 0) == 0);
	CHECK(strview_find_first_not_of(view, "0123456789", 2) == 2);
	CHECK(strview_find_first_not_of(view, "0123456789", 3) == 6);
	CHECK(strview_find_last_not_of(view, "abcdefghij", STRVIEW_NPOS) == 17);
	CHECK(strview_find_last_not_of(view, "abcdefghij", 14) == 11);
	CHECK(strview_find_last_not_of(view, "abcdefghij", 9) == 9);
	CHECK(strview_find_last_not_of(view, "0123456789", STRVIEW_NPOS) == 14);
	CHECK(strview_find_last_not_of(view, "0123456789", 11) == 8);
	CHECK(strview_find_last_not_of(view, "0123456789", 6) == 6);

	view = strview_from_cstr("");
	CHECK(strview_find_first_of(view, "abc123def456ghi789", 0) == STRVIEW_NPOS);
	CHECK(strview_find_first_not_of(view, "abc123def456ghi789", 0) == STRVIEW_NPOS);
	CHECK(strview_find_last_of(view, "abc123def456ghi789", STRVIEW_NPOS) == STRVIEW_NPOS);
	CHECK(strview_find_last_not_of(view, "abc123def456ghi789", STRVIEW_NPOS) == STRVIEW_NPOS);

	view = strview_from_cstr("xxxxx");
	CHECK(strview_find_first_of(view, "abc123def456ghi789", 0) == STRVIEW_NPOS);
	CHECK(strview_find_first_not_of(view, "abc123def456ghi789", 0) == 0);
	CHECK(strview_find_last_of(view, "abc123def456ghi789", STRVIEW_NPOS) == STRVIEW_NPOS);
	CHECK(strview_find_last_not_of(view, "abc123def456ghi789", STRVIEW_NPOS) == 4);

	view = strview_from_cstr(a256);
	for (size_t i = 0; i < abc_len; i++) {
		CHECK(strview_find_first_of(view, a256, i) == i);
		CHECK(strview_find_first_not_of(view, a256, i) == STRVIEW_NPOS);
	}

	view = strview_from_cstr(a256);
	for (size_t i = abc_len - 1; i-- > 0;) {
		CHECK(strview_find_last_of(view, a256, i) == i);
		CHECK(strview_find_last_not_of(view, a256, i) == STRVIEW_NPOS);
	}
	return true;
}

SIMPLE_TEST(strview_strip)
{
	struct strview view = strview_from_cstr("---aaa---");
	view = strview_strip(view, "-");
	CHECK(strview_equal_cstr(view, "aaa"));

	view = strview_from_cstr("---aaa---");
	view = strview_lstrip(view, "-");
	CHECK(strview_equal_cstr(view, "aaa---"));
	view = strview_rstrip(view, "-");
	CHECK(strview_equal_cstr(view, "aaa"));

	view = strview_from_cstr("---aaa---");
	view = strview_rstrip(view, "-");
	CHECK(strview_equal_cstr(view, "---aaa"));
	view = strview_lstrip(view, "-");
	CHECK(strview_equal_cstr(view, "aaa"));

	view = strview_from_cstr("abcabacba");
	view = strview_strip(view, "ab");
	CHECK(strview_equal_cstr(view, "cabac"));
	view = strview_strip(view, "ca");
	CHECK(strview_equal_cstr(view, "b"));

	view = strview_from_cstr("abcdeffedcba");
	view = strview_strip(view, "bcdef");
	CHECK(strview_equal_cstr(view, "abcdeffedcba"));
	view = strview_strip(view, "");
	CHECK(strview_equal_cstr(view, "abcdeffedcba"));
	view = strview_strip(view, "ac");
	CHECK(strview_equal_cstr(view, "bcdeffedcb"));
	view = strview_lstrip(view, "efcb");
	CHECK(strview_equal_cstr(view, "deffedcb"));
	view = strview_strip(view, "bcdf");
	CHECK(strview_equal_cstr(view, "effe"));
	view = strview_rstrip(view, "ef");
	CHECK(strview_equal_cstr(view, ""));
	view = strview_strip(view, "");
	CHECK(strview_equal_cstr(view, ""));
	view = strview_strip(view, "abcdef");
	CHECK(strview_equal_cstr(view, ""));

	return true;
}

SIMPLE_TEST(strview_starts_ends_with)
{
	struct strview view = strview_from_cstr(abc);
	CHECK(strview_startswith_cstr(view, abc));
	CHECK(strview_startswith(view, strview_from_cstr(abc)));
	CHECK(strview_endswith_cstr(view, abc));
	CHECK(strview_endswith(view, strview_from_cstr(abc)));
	CHECK(strview_startswith_cstr(view, ""));
	CHECK(strview_startswith(view, strview_from_cstr("")));
	CHECK(strview_endswith_cstr(view, ""));
	CHECK(strview_endswith(view, strview_from_cstr("")));

	view = strview_from_cstr(a256);
	CHECK(strview_startswith_cstr(view, a256));
	CHECK(strview_startswith(view, strview_from_cstr(a256)));
	CHECK(strview_endswith_cstr(view, a256));
	CHECK(strview_endswith(view, strview_from_cstr(a256)));
	CHECK(strview_startswith_cstr(view, ""));
	CHECK(strview_startswith(view, strview_from_cstr("")));
	CHECK(strview_endswith_cstr(view, ""));
	CHECK(strview_endswith(view, strview_from_cstr("")));

	view = strview_from_cstr("");
	CHECK(!strview_startswith_cstr(view, "a"));
	CHECK(!strview_startswith(view, strview_from_cstr("a")));
	CHECK(!strview_endswith_cstr(view, "a"));
	CHECK(!strview_endswith(view, strview_from_cstr("a")));
	CHECK(strview_startswith_cstr(view, ""));
	CHECK(strview_startswith(view, strview_from_cstr("")));
	CHECK(strview_endswith_cstr(view, ""));
	CHECK(strview_endswith(view, strview_from_cstr("")));

	view = strview_from_cstr("axb");
	CHECK(strview_startswith_cstr(view, "a"));
	CHECK(strview_startswith(view, strview_from_cstr("a")));
	CHECK(strview_endswith_cstr(view, "b"));
	CHECK(strview_endswith(view, strview_from_cstr("b")));
	CHECK(strview_startswith_cstr(view, "ax"));
	CHECK(strview_startswith(view, strview_from_cstr("ax")));
	CHECK(strview_endswith_cstr(view, "xb"));
	CHECK(strview_endswith(view, strview_from_cstr("xb")));
	CHECK(!strview_startswith_cstr(view, "b"));
	CHECK(!strview_startswith(view, strview_from_cstr("b")));
	CHECK(!strview_endswith_cstr(view, "a"));
	CHECK(!strview_endswith(view, strview_from_cstr("a")));
	CHECK(!strview_startswith_cstr(view, "xb"));
	CHECK(!strview_startswith(view, strview_from_cstr("xb")));
	CHECK(!strview_endswith_cstr(view, "ax"));
	CHECK(!strview_endswith(view, strview_from_cstr("ax")));

	view = strview_from_chars(abc, abc_len - 1);
	CHECK(!strview_startswith_cstr(view, abc));
	CHECK(!strview_startswith(view, strview_from_cstr(abc)));

	dstr_t dstr = dstr_from_fmt("%s%s", a256, a256);
	view = dstr_view(dstr);
	CHECK(strview_startswith_cstr(view, a256));
	CHECK(strview_startswith(view, strview_from_cstr(a256)));
	CHECK(strview_endswith_cstr(view, a256));
	CHECK(strview_endswith(view, strview_from_cstr(a256)));
	dstr_free(&dstr);

	dstr = dstr_from_fmt("%s-%s", abc, abc);
	view = dstr_view(dstr);
	CHECK(strview_startswith_cstr(view, abc));
	CHECK(strview_startswith(view, strview_from_cstr(abc)));
	CHECK(strview_endswith_cstr(view, abc));
	CHECK(strview_endswith(view, strview_from_cstr(abc)));
	CHECK(!strview_startswith_cstr(view, "-"));
	CHECK(!strview_startswith(view, strview_from_cstr("-")));
	CHECK(!strview_endswith_cstr(view, "-"));
	CHECK(!strview_endswith(view, strview_from_cstr("-")));
	struct strview view2 = strview_substring(view, abc_len, STRVIEW_NPOS);
	CHECK(strview_startswith_cstr(view2, "-"));
	CHECK(strview_startswith(view2, strview_from_cstr("-")));
	CHECK(!strview_startswith_cstr(view2, abc));
	CHECK(!strview_startswith(view2, strview_from_cstr(abc)));
	view2 = strview_substring(view, abc_len + 1, STRVIEW_NPOS);
	CHECK(strview_startswith_cstr(view2, abc));
	CHECK(strview_startswith(view2, strview_from_cstr(abc)));
	CHECK(!strview_startswith_cstr(view2, "-"));
	CHECK(!strview_startswith(view2, strview_from_cstr("-")));
	view2 = strview_substring(view, 0, abc_len + 1);
	CHECK(strview_endswith_cstr(view2, "-"));
	CHECK(strview_endswith(view2, strview_from_cstr("-")));
	CHECK(!strview_endswith_cstr(view2, abc));
	CHECK(!strview_endswith(view2, strview_from_cstr(abc)));
	view2 = strview_substring(view, 0, abc_len);
	CHECK(strview_endswith_cstr(view2, abc));
	CHECK(strview_endswith(view2, strview_from_cstr(abc)));
	CHECK(!strview_endswith_cstr(view2, "-"));
	CHECK(!strview_endswith(view2, strview_from_cstr("-")));
	dstr_free(&dstr);

	view = strview_from_cstr(a256);
	view2 = strview_from_cstr(a256);
	CHECK(strview_startswith(view, view2));
	CHECK(strview_endswith(view, view2));
	view2 = strview_from_cstr("***");
	CHECK(!strview_startswith(view, view2));
	CHECK(!strview_endswith(view, view2));

	return true;
}

SIMPLE_TEST(strview_split)
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
		struct strview view = strview_from_cstr(str);
		struct strview_list strview_list = (rev ? strview_rsplit : strview_split)(view, c, max);
		CHECK(strview_list.count == count);
		for (size_t j = 0; j < count; j++) {
			CHECK(strview_equal_cstr(strview_list.strings[j], results[j]));
		}
		strview_list_free(&strview_list);
	}

	return true;
}

#if _FORTIFY_SOURCE >= 1 && (defined(HAVE_BUILTIN_DYNAMIC_OBJECT_SIZE) || defined(HAVE_BUILTIN_OBJECT_SIZE))

NEGATIVE_SIMPLE_TEST(fortify_strview_from_chars)
{
	char chars[16] = {0};
	strview_from_chars(chars, sizeof(chars) + 1);
	return true;
}

#endif
