#include <assert.h>
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "charconv.h"
#include "compiler.h"
#include "fortify.h"
#include "testing.h"
#include "utils.h"

static bool check_from_number_binary(uint64_t val)
{
	// printf doesn't support binary yet...
	char str[128];
	size_t len;
	uint64_t u64 = val;
	len = to_chars(str, sizeof(str), u64, TO_CHARS_BINARY);
	CHECK(len == 64 || (u64 >> len) == 0);
	for (size_t i = 0; i < len; i++) {
		CHECK(str[i] == '0' || str[i] == '1');
		uint32_t b1 = (u64 >> i) & 1;
		uint32_t b2 = str[len - i - 1] - '0';
		CHECK(b1 == b2); // TODO
	}
	int64_t s64 = val;
	len = to_chars(str, sizeof(str), s64, TO_CHARS_BINARY | TO_CHARS_LEADING_ZEROS |
		       TO_CHARS_PLUS_SIGN | TO_CHARS_UPPERCASE);
	CHECK(len == 65);
	CHECK(str[0] == (s64 < 0 ? '-' : '+'));
	if (s64 < 0) {
		s64 = -s64;
	}
	for (size_t i = 0; i < len - 1; i++) {
		CHECK(str[len - 1 - i] == '0' || str[len - 1 - i] == '1');
		uint32_t b1 = (s64 >> i) & 1;
		uint32_t b2 = str[len - 1 - i] - '0';
		CHECK(b1 == b2);
	}

	uint32_t u32 = val;
	len = to_chars(str, sizeof(str), u32, TO_CHARS_BINARY);
	CHECK(len == 32 || (u32 >> len) == 0);
	for (size_t i = 0; i < len; i++) {
		CHECK(str[i] == '0' || str[i] == '1');
		uint32_t b1 = (u32 >> i) & 1;
		uint32_t b2 = str[len - i - 1] - '0';
		CHECK(b1 == b2); // TODO
	}
	int32_t s32 = val;
	len = to_chars(str, sizeof(str), s32, TO_CHARS_BINARY | TO_CHARS_LEADING_ZEROS |
		       TO_CHARS_PLUS_SIGN | TO_CHARS_UPPERCASE);
	CHECK(len == 33);
	CHECK(str[0] == (s32 < 0 ? '-' : '+'));
	if (s32 < 0) {
		s32 = -s32;
	}
	for (size_t i = 0; i < len - 1; i++) {
		CHECK(str[len - 1 - i] == '0' || str[len - 1 - i] == '1');
		uint32_t b1 = (s32 >> i) & 1;
		uint32_t b2 = str[len - 1 - i] - '0';
		CHECK(b1 == b2);
	}
	return true;
}

static bool check_from_number(uint64_t x)
{
	uint32_t u32 = x;
	int32_t  s32 = x;
	uint64_t u64 = x;
	int64_t  s64 = x;
	char buf[4096];
	sprintf(buf,
		"x" // dummy for strtok
		" %" PRId32
		" %" PRIu32
		" %" PRIo32
		" %" PRIx32
		" %" PRIX32
		" %+" PRId32
		" %" PRIu32
		" %" PRIo32
		" %" PRIx32
		// " %010" PRId32 // printf includes sign bit in the 10 chars, we don't
		" %010" PRIu32
		" %011" PRIo32
		" %08" PRIx32
		" %+011" PRId32
		" %010" PRIu32
		" %011" PRIo32
		" %08" PRIx32
		" %" PRId64
		" %" PRIu64
		" %" PRIo64
		" %" PRIx64
		" %" PRIX64
		" %+" PRId64
		" %" PRIu64
		" %" PRIo64
		" %" PRIx64
		// " %019" PRId64 // same as above
		" %020" PRIu64
		" %022" PRIo64
		" %016" PRIx64
		" %+020" PRId64
		" %020" PRIu64
		" %022" PRIo64
		" %016" PRIx64,
		u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32,
		u64, u64, u64, u64, u64, u64, u64, u64, u64, u64, u64, u64, u64, u64, u64, u64);
	char *save_ptr = NULL;
	strtok_r(buf, " ", &save_ptr);

	const struct {
		bool is_64bit;
		bool is_signed;
		unsigned int flags;
	} tests[] = {
		{false, true, 0},
		{false, false, TO_CHARS_DECIMAL},
		{false, false, TO_CHARS_OCTAL},
		{false, false, TO_CHARS_HEXADECIMAL},
		{false, false, TO_CHARS_HEXADECIMAL | TO_CHARS_UPPERCASE},
		{false, true,  TO_CHARS_DECIMAL | TO_CHARS_PLUS_SIGN},
		{false, false, TO_CHARS_DECIMAL | TO_CHARS_PLUS_SIGN},
		{false, false, TO_CHARS_OCTAL | TO_CHARS_PLUS_SIGN},
		{false, false, TO_CHARS_HEXADECIMAL | TO_CHARS_PLUS_SIGN},
		// {false, true,  TO_CHARS_DECIMAL | TO_CHARS_LEADING_ZEROS},
		{false, false, TO_CHARS_DECIMAL | TO_CHARS_LEADING_ZEROS},
		{false, false, TO_CHARS_OCTAL | TO_CHARS_LEADING_ZEROS},
		{false, false, TO_CHARS_HEXADECIMAL | TO_CHARS_LEADING_ZEROS},
		{false, true,  TO_CHARS_DECIMAL | TO_CHARS_LEADING_ZEROS | TO_CHARS_PLUS_SIGN},
		{false, false, TO_CHARS_DECIMAL | TO_CHARS_LEADING_ZEROS | TO_CHARS_PLUS_SIGN},
		{false, false, TO_CHARS_OCTAL | TO_CHARS_LEADING_ZEROS | TO_CHARS_PLUS_SIGN},
		{false, false, TO_CHARS_HEXADECIMAL | TO_CHARS_LEADING_ZEROS | TO_CHARS_PLUS_SIGN},
		{true, true, 0},
		{true, false, TO_CHARS_DECIMAL},
		{true, false, TO_CHARS_OCTAL},
		{true, false, TO_CHARS_HEXADECIMAL},
		{true, false, TO_CHARS_HEXADECIMAL | TO_CHARS_UPPERCASE},
		{true, true,  TO_CHARS_DECIMAL | TO_CHARS_PLUS_SIGN},
		{true, false, TO_CHARS_DECIMAL | TO_CHARS_PLUS_SIGN},
		{true, false, TO_CHARS_OCTAL | TO_CHARS_PLUS_SIGN},
		{true, false, TO_CHARS_HEXADECIMAL | TO_CHARS_PLUS_SIGN},
		// {true, true,  TO_CHARS_DECIMAL | TO_CHARS_LEADING_ZEROS},
		{true, false, TO_CHARS_DECIMAL | TO_CHARS_LEADING_ZEROS},
		{true, false, TO_CHARS_OCTAL | TO_CHARS_LEADING_ZEROS},
		{true, false, TO_CHARS_HEXADECIMAL | TO_CHARS_LEADING_ZEROS},
		{true, true,  TO_CHARS_DECIMAL | TO_CHARS_LEADING_ZEROS | TO_CHARS_PLUS_SIGN},
		{true, false, TO_CHARS_DECIMAL | TO_CHARS_LEADING_ZEROS | TO_CHARS_PLUS_SIGN},
		{true, false, TO_CHARS_OCTAL | TO_CHARS_LEADING_ZEROS | TO_CHARS_PLUS_SIGN},
		{true, false, TO_CHARS_HEXADECIMAL | TO_CHARS_LEADING_ZEROS | TO_CHARS_PLUS_SIGN},
	};
	for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
		char str[64];
		size_t len;
		if (tests[i].is_64bit) {
			if (tests[i].is_signed) {
				len = to_chars(str, sizeof(str), s64, tests[i].flags);
			} else {
				len = to_chars(str, sizeof(str), u64, tests[i].flags);
			}
		} else {
			if (tests[i].is_signed) {
				len = to_chars(str, sizeof(str), s32, tests[i].flags);
			} else {
				len = to_chars(str, sizeof(str), u32, tests[i].flags);
			}
		}
		char *reference = strtok_r(NULL, " ", &save_ptr);
		assert(reference);
		// if (len != strlen(reference) || memcmp(str, reference, len) != 0) {
		// 	unsigned long long ull = tests[i].is_64bit ? u64 : u32;
		// 	long long ll = tests[i].is_64bit ? s64 : s32;
		// 	test_log("[%zu] %.*s != %s (%llu %lld %d %d %u)\n", i + 1, (int)len, str, reference,
		// 		 ull, ll, tests[i].is_64bit, tests[i].is_signed, tests[i].flags);
		// }
		CHECK(len == strlen(reference));
		CHECK(memcmp(str, reference, len) == 0);
	}
	assert(!strtok_r(NULL, " ", &save_ptr));
	return check_from_number_binary(x);
}

static bool check_binary(const char *str, size_t len, unsigned long long val, bool negative)
{
	// test_log("%.*s %zu %llu %d\n", (int)len, str, len, val, negative);
	if (negative) {
		CHECK(len > 1 && str[0] == '-');
		str++;
		len--;
		val = -val;
	}
	for (size_t i = 0; i < len; i++) {
		unsigned long long b1 = (val >> i) & 1;
		uint32_t b2 = str[len - i - 1] - '0';
		CHECK(b1 == b2);
	}
	return true;
}

SIMPLE_TEST(to_chars)
{
	char str[256];
	size_t len;
#define CHECK_TO_CHARS(expected) CHECK(len == strlen(expected) && memcmp(str, expected, len) == 0)

#define CHECK_TO_CHARS_DECIMAL_UNSIGNED(type)				\
	do {								\
		len = to_chars(str, sizeof(str), (type)0, TO_CHARS_DEFAULT); \
		CHECK_TO_CHARS("0");					\
		len = to_chars(str, sizeof(str), (type)1, TO_CHARS_DEFAULT); \
		CHECK_TO_CHARS("1");					\
		len = to_chars(str, sizeof(str), (type)10, TO_CHARS_DEFAULT); \
		CHECK_TO_CHARS("10");					\
		len = to_chars(str, sizeof(str), (type)100, TO_CHARS_DEFAULT); \
		CHECK_TO_CHARS("100");					\
		len = to_chars(str, sizeof(str), (type)123, TO_CHARS_DECIMAL | TO_CHARS_PLUS_SIGN); \
		CHECK_TO_CHARS("123");					\
		len = to_chars(str, sizeof(str), (type)-1, TO_CHARS_DEFAULT); \
		char max_val_str[32];					\
		sprintf(max_val_str, "%llu", (unsigned long long)(type)-1); \
		CHECK_TO_CHARS(max_val_str);				\
	} while (0)

#if CHAR_MIN == 0
	CHECK_TO_CHARS_DECIMAL_UNSIGNED(char);
#endif
	CHECK_TO_CHARS_DECIMAL_UNSIGNED(unsigned char);
	CHECK_TO_CHARS_DECIMAL_UNSIGNED(unsigned short);
	CHECK_TO_CHARS_DECIMAL_UNSIGNED(unsigned int);
	CHECK_TO_CHARS_DECIMAL_UNSIGNED(unsigned long);
	CHECK_TO_CHARS_DECIMAL_UNSIGNED(unsigned long long);

#define CHECK_TO_CHARS_DECIMAL_SIGNED(type)				\
	do {								\
		len = to_chars(str, sizeof(str), (type)0, TO_CHARS_DEFAULT); \
		CHECK_TO_CHARS("0");					\
		len = to_chars(str, sizeof(str), (type)1, TO_CHARS_DEFAULT); \
		CHECK_TO_CHARS("1");					\
		len = to_chars(str, sizeof(str), (type)10, TO_CHARS_DEFAULT); \
		CHECK_TO_CHARS("10");					\
		len = to_chars(str, sizeof(str), (type)100, TO_CHARS_DEFAULT); \
		CHECK_TO_CHARS("100");					\
		len = to_chars(str, sizeof(str), (type)123, TO_CHARS_DEFAULT); \
		CHECK_TO_CHARS("123");					\
		len = to_chars(str, sizeof(str), (type)0, TO_CHARS_DECIMAL | TO_CHARS_PLUS_SIGN); \
		CHECK_TO_CHARS("+0");					\
		len = to_chars(str, sizeof(str), (type)1, TO_CHARS_DECIMAL | TO_CHARS_PLUS_SIGN); \
		CHECK_TO_CHARS("+1");					\
		len = to_chars(str, sizeof(str), (type)10, TO_CHARS_DECIMAL | TO_CHARS_PLUS_SIGN); \
		CHECK_TO_CHARS("+10");					\
		len = to_chars(str, sizeof(str), (type)123, TO_CHARS_DECIMAL | TO_CHARS_PLUS_SIGN); \
		CHECK_TO_CHARS("+123");					\
		len = to_chars(str, sizeof(str), (type)-1, TO_CHARS_DEFAULT); \
		CHECK_TO_CHARS("-1");					\
		len = to_chars(str, sizeof(str), (type)-10, TO_CHARS_DEFAULT); \
		CHECK_TO_CHARS("-10");					\
		len = to_chars(str, sizeof(str), (type)-123, TO_CHARS_DEFAULT);	\
		CHECK_TO_CHARS("-123");					\
		long long min_val = (type)(1llu << (sizeof(type) * 8 - 1)); \
		long long max_val = ~min_val;				\
		len = to_chars(str, sizeof(str), (type)min_val, TO_CHARS_DEFAULT); \
		char min_val_str[32];					\
		sprintf(min_val_str, "%lld", min_val);			\
		CHECK_TO_CHARS(min_val_str);				\
		len = to_chars(str, sizeof(str), (type)max_val, TO_CHARS_DEFAULT); \
		char max_val_str[32];					\
		sprintf(max_val_str, "%lld", max_val);			\
		CHECK_TO_CHARS(max_val_str);				\
	} while (0)

#if CHAR_MIN < 0
	CHECK_TO_CHARS_DECIMAL_SIGNED(char);
#endif
	CHECK_TO_CHARS_DECIMAL_SIGNED(signed char);
	CHECK_TO_CHARS_DECIMAL_SIGNED(signed short);
	CHECK_TO_CHARS_DECIMAL_SIGNED(signed int);
	CHECK_TO_CHARS_DECIMAL_SIGNED(signed long);
	CHECK_TO_CHARS_DECIMAL_SIGNED(signed long long);

#define CHECK_TO_CHARS_BINARY(type)					\
	do {								\
		bool is_signed = type_is_signed(type);			\
		len = to_chars(str, sizeof(str), (type)0, TO_CHARS_BINARY); \
		CHECK_TO_CHARS("0");					\
		len = to_chars(str, sizeof(str), (type)1, TO_CHARS_BINARY); \
		CHECK_TO_CHARS("1");					\
		len = to_chars(str, sizeof(str), (type)2, TO_CHARS_BINARY); \
		CHECK_TO_CHARS("10");					\
		len = to_chars(str, sizeof(str), (type)4, TO_CHARS_BINARY); \
		CHECK_TO_CHARS("100");					\
		len = to_chars(str, sizeof(str), (type)8, TO_CHARS_BINARY); \
		CHECK_TO_CHARS("1000");					\
		len = to_chars(str, sizeof(str), (type)16, TO_CHARS_BINARY); \
		CHECK_TO_CHARS("10000");				\
		len = to_chars(str, sizeof(str), (type)32, TO_CHARS_BINARY); \
		CHECK_TO_CHARS("100000");				\
		len = to_chars(str, sizeof(str), (type)64, TO_CHARS_BINARY); \
		CHECK_TO_CHARS("1000000");				\
		len = to_chars(str, sizeof(str), (type)128, TO_CHARS_BINARY); \
		CHECK_TO_CHARS(((type)128 < 0) ? "-10000000" : "10000000"); \
		len = to_chars(str, sizeof(str), (type)10, TO_CHARS_BINARY); \
		CHECK_TO_CHARS("1010");					\
		len = to_chars(str, sizeof(str), (type)123, TO_CHARS_BINARY | TO_CHARS_PLUS_SIGN); \
		CHECK_TO_CHARS(is_signed ? "+1111011" : "1111011");	\
		long long signed_min_val = 1llu << (sizeof(type) * 8 - 1); \
		long long signed_max_val = (type)~signed_min_val;	\
		len = to_chars(str, sizeof(str), (type)signed_min_val, TO_CHARS_BINARY); \
		CHECK(check_binary(str, len, signed_min_val, is_signed)); \
		len = to_chars(str, sizeof(str), (type)signed_max_val, TO_CHARS_BINARY); \
		CHECK(check_binary(str, len, signed_max_val, false));	\
		len = to_chars(str, sizeof(str), (type)-1, TO_CHARS_BINARY); \
		unsigned long long minus_one = ~0llu >> ((sizeof(long long) - sizeof(type)) * 8); \
		CHECK(check_binary(str, len, minus_one, is_signed));	\
} while (0)

	CHECK_TO_CHARS_BINARY(char);
	CHECK_TO_CHARS_BINARY(unsigned char);
	CHECK_TO_CHARS_BINARY(signed char);
	CHECK_TO_CHARS_BINARY(short);
	CHECK_TO_CHARS_BINARY(unsigned short);
	CHECK_TO_CHARS_BINARY(int);
	CHECK_TO_CHARS_BINARY(unsigned int);
	CHECK_TO_CHARS_BINARY(long);
	CHECK_TO_CHARS_BINARY(unsigned long);
	CHECK_TO_CHARS_BINARY(long long);
	CHECK_TO_CHARS_BINARY(unsigned long long);

#define CHECK_TO_CHARS_OCTAL(type)					\
	do {								\
		bool is_signed = type_is_signed(type);			\
		len = to_chars(str, sizeof(str), (type)0, TO_CHARS_OCTAL); \
		CHECK_TO_CHARS("0");					\
		len = to_chars(str, sizeof(str), (type)1, TO_CHARS_OCTAL); \
		CHECK_TO_CHARS("1");					\
		len = to_chars(str, sizeof(str), (type)010, TO_CHARS_OCTAL); \
		CHECK_TO_CHARS("10");					\
		len = to_chars(str, sizeof(str), (type)0100, TO_CHARS_OCTAL); \
		CHECK_TO_CHARS("100");					\
		len = to_chars(str, sizeof(str), (type)10, TO_CHARS_OCTAL); \
		CHECK_TO_CHARS("12");					\
		len = to_chars(str, sizeof(str), (type)123, TO_CHARS_OCTAL | TO_CHARS_PLUS_SIGN); \
		CHECK_TO_CHARS(is_signed ? "+173" : "173");		\
		long long signed_min_val = 1llu << (sizeof(type) * 8 - 1); \
		long long signed_max_val = (type)~signed_min_val;	\
		len = to_chars(str, sizeof(str), (type)signed_min_val, TO_CHARS_OCTAL); \
		char signed_min_val_str[32];				\
		sprintf(signed_min_val_str, "%s%llo", is_signed ? "-" : "", signed_min_val); \
		CHECK_TO_CHARS(signed_min_val_str);			\
		len = to_chars(str, sizeof(str), (type)signed_max_val, TO_CHARS_OCTAL); \
		char signed_max_val_str[32];				\
		sprintf(signed_max_val_str, "%llo", signed_max_val);	\
		CHECK_TO_CHARS(signed_max_val_str);			\
		len = to_chars(str, sizeof(str), (type)-1, TO_CHARS_OCTAL); \
		char unsigned_max_val_str[32];				\
		unsigned long long unsigned_max_val = ~0llu >> ((sizeof(long long) - sizeof(type)) * 8); \
		sprintf(unsigned_max_val_str, "%llo", unsigned_max_val); \
		CHECK_TO_CHARS(is_signed ? "-1" : unsigned_max_val_str); \
	} while (0)

	CHECK_TO_CHARS_OCTAL(char);
	CHECK_TO_CHARS_OCTAL(unsigned char);
	CHECK_TO_CHARS_OCTAL(signed char);
	CHECK_TO_CHARS_OCTAL(short);
	CHECK_TO_CHARS_OCTAL(unsigned short);
	CHECK_TO_CHARS_OCTAL(int);
	CHECK_TO_CHARS_OCTAL(unsigned int);
	CHECK_TO_CHARS_OCTAL(long);
	CHECK_TO_CHARS_OCTAL(unsigned long);
	CHECK_TO_CHARS_OCTAL(long long);
	CHECK_TO_CHARS_OCTAL(unsigned long long);

#define CHECK_TO_CHARS_HEXADECIMAL(type)				\
	do {								\
		bool is_signed = type_is_signed(type);			\
		len = to_chars(str, sizeof(str), (type)0, TO_CHARS_HEXADECIMAL);	\
		CHECK_TO_CHARS("0");					\
		len = to_chars(str, sizeof(str), (type)1, TO_CHARS_HEXADECIMAL);	\
		CHECK_TO_CHARS("1");					\
		len = to_chars(str, sizeof(str), (type)0x10, TO_CHARS_HEXADECIMAL);	\
		CHECK_TO_CHARS("10");					\
		len = to_chars(str, sizeof(str), (type)10, TO_CHARS_HEXADECIMAL);	\
		CHECK_TO_CHARS("a");					\
		len = to_chars(str, sizeof(str), (type)123, TO_CHARS_HEXADECIMAL | TO_CHARS_PLUS_SIGN); \
		CHECK_TO_CHARS(is_signed ? "+7b" : "7b");		\
		len = to_chars(str, sizeof(str), (type)123, TO_CHARS_HEXADECIMAL | TO_CHARS_PLUS_SIGN | TO_CHARS_UPPERCASE); \
		CHECK_TO_CHARS(is_signed ? "+7B" : "7B");		\
		long long signed_min_val = 1llu << (sizeof(type) * 8 - 1); \
		long long signed_max_val = (type)~signed_min_val;	\
		len = to_chars(str, sizeof(str), (type)signed_min_val, TO_CHARS_HEXADECIMAL); \
		char signed_min_val_str[32];				\
		sprintf(signed_min_val_str, "%s%llx", is_signed ? "-" : "", signed_min_val); \
		CHECK_TO_CHARS(signed_min_val_str);			\
		len = to_chars(str, sizeof(str), (type)signed_max_val, TO_CHARS_HEXADECIMAL); \
		char signed_max_val_str[32];				\
		sprintf(signed_max_val_str, "%llx", signed_max_val);	\
		CHECK_TO_CHARS(signed_max_val_str);			\
		len = to_chars(str, sizeof(str), (type)-1, TO_CHARS_HEXADECIMAL);	\
		char unsigned_max_val_str[32];				\
		unsigned long long unsigned_max_val = ~0llu >> ((sizeof(long long) - sizeof(type)) * 8); \
		sprintf(unsigned_max_val_str, "%llx", unsigned_max_val); \
		CHECK_TO_CHARS(is_signed ? "-1" : unsigned_max_val_str); \
	} while (0)

	CHECK_TO_CHARS_HEXADECIMAL(char);
	CHECK_TO_CHARS_HEXADECIMAL(unsigned char);
	CHECK_TO_CHARS_HEXADECIMAL(signed char);
	CHECK_TO_CHARS_HEXADECIMAL(short);
	CHECK_TO_CHARS_HEXADECIMAL(unsigned short);
	CHECK_TO_CHARS_HEXADECIMAL(int);
	CHECK_TO_CHARS_HEXADECIMAL(unsigned int);
	CHECK_TO_CHARS_HEXADECIMAL(long);
	CHECK_TO_CHARS_HEXADECIMAL(unsigned long);
	CHECK_TO_CHARS_HEXADECIMAL(long long);
	CHECK_TO_CHARS_HEXADECIMAL(unsigned long long);

	len = to_chars(str, sizeof(str), 1234567890, TO_CHARS_DECIMAL);
	CHECK_TO_CHARS("1234567890");
	len = to_chars(str, sizeof(str), 2, TO_CHARS_BINARY);
	CHECK_TO_CHARS("10");
	len = to_chars(str, sizeof(str), 012345670, TO_CHARS_OCTAL);
	CHECK_TO_CHARS("12345670");
	len = to_chars(str, sizeof(str), 0x1234567890abcdef, TO_CHARS_HEXADECIMAL);
	CHECK_TO_CHARS("1234567890abcdef");
	len = to_chars(str, sizeof(str), 0x1234567890abcdef, TO_CHARS_HEXADECIMAL | TO_CHARS_UPPERCASE);
	CHECK_TO_CHARS("1234567890ABCDEF");

	CHECK(to_chars(NULL, 0, (uint8_t)0, TO_CHARS_BINARY | TO_CHARS_LEADING_ZEROS) == 8);
	CHECK(to_chars(NULL, 0, (uint16_t)0, TO_CHARS_BINARY | TO_CHARS_LEADING_ZEROS) == 16);
	CHECK(to_chars(NULL, 0, (uint32_t)0, TO_CHARS_BINARY | TO_CHARS_LEADING_ZEROS) == 32);
	CHECK(to_chars(NULL, 0, (uint64_t)0, TO_CHARS_BINARY | TO_CHARS_LEADING_ZEROS) == 64);
	CHECK(to_chars(NULL, 0, (int8_t)0, TO_CHARS_BINARY | TO_CHARS_LEADING_ZEROS) == 8);
	CHECK(to_chars(NULL, 0, (int16_t)0, TO_CHARS_BINARY | TO_CHARS_LEADING_ZEROS) == 16);
	CHECK(to_chars(NULL, 0, (int32_t)0, TO_CHARS_BINARY | TO_CHARS_LEADING_ZEROS) == 32);
	CHECK(to_chars(NULL, 0, (int64_t)0, TO_CHARS_BINARY | TO_CHARS_LEADING_ZEROS) == 64);

	CHECK(to_chars(NULL, 0, (uint8_t)0, TO_CHARS_OCTAL | TO_CHARS_LEADING_ZEROS) == 3);
	CHECK(to_chars(NULL, 0, (uint16_t)0, TO_CHARS_OCTAL | TO_CHARS_LEADING_ZEROS) == 6);
	CHECK(to_chars(NULL, 0, (uint32_t)0, TO_CHARS_OCTAL | TO_CHARS_LEADING_ZEROS) == 11);
	CHECK(to_chars(NULL, 0, (uint64_t)0, TO_CHARS_OCTAL | TO_CHARS_LEADING_ZEROS) == 22);
	CHECK(to_chars(NULL, 0, (int8_t)0, TO_CHARS_OCTAL | TO_CHARS_LEADING_ZEROS) == 3);
	CHECK(to_chars(NULL, 0, (int16_t)0, TO_CHARS_OCTAL | TO_CHARS_LEADING_ZEROS) == 6);
	CHECK(to_chars(NULL, 0, (int32_t)0, TO_CHARS_OCTAL | TO_CHARS_LEADING_ZEROS) == 11);
	CHECK(to_chars(NULL, 0, (int64_t)0, TO_CHARS_OCTAL | TO_CHARS_LEADING_ZEROS) == 22);

	CHECK(to_chars(NULL, 0, (uint8_t)0, TO_CHARS_HEXADECIMAL | TO_CHARS_LEADING_ZEROS) == 2);
	CHECK(to_chars(NULL, 0, (uint16_t)0, TO_CHARS_HEXADECIMAL | TO_CHARS_LEADING_ZEROS) == 4);
	CHECK(to_chars(NULL, 0, (uint32_t)0, TO_CHARS_HEXADECIMAL | TO_CHARS_LEADING_ZEROS) == 8);
	CHECK(to_chars(NULL, 0, (uint64_t)0, TO_CHARS_HEXADECIMAL | TO_CHARS_LEADING_ZEROS) == 16);
	CHECK(to_chars(NULL, 0, (int8_t)0, TO_CHARS_HEXADECIMAL | TO_CHARS_LEADING_ZEROS) == 2);
	CHECK(to_chars(NULL, 0, (int16_t)0, TO_CHARS_HEXADECIMAL | TO_CHARS_LEADING_ZEROS) == 4);
	CHECK(to_chars(NULL, 0, (int32_t)0, TO_CHARS_HEXADECIMAL | TO_CHARS_LEADING_ZEROS) == 8);
	CHECK(to_chars(NULL, 0, (int64_t)0, TO_CHARS_HEXADECIMAL | TO_CHARS_LEADING_ZEROS) == 16);

	CHECK(to_chars(NULL, 0, (uint8_t)0, TO_CHARS_DECIMAL | TO_CHARS_LEADING_ZEROS) == 3);
	CHECK(to_chars(NULL, 0, (uint16_t)0, TO_CHARS_DECIMAL | TO_CHARS_LEADING_ZEROS) == 5);
	CHECK(to_chars(NULL, 0, (uint32_t)0, TO_CHARS_DECIMAL | TO_CHARS_LEADING_ZEROS) == 10);
	CHECK(to_chars(NULL, 0, (uint64_t)0, TO_CHARS_DECIMAL | TO_CHARS_LEADING_ZEROS) == 20);
	CHECK(to_chars(NULL, 0, (int8_t)0, TO_CHARS_DECIMAL | TO_CHARS_LEADING_ZEROS) == 3);
	CHECK(to_chars(NULL, 0, (int16_t)0, TO_CHARS_DECIMAL | TO_CHARS_LEADING_ZEROS) == 5);
	CHECK(to_chars(NULL, 0, (int32_t)0, TO_CHARS_DECIMAL | TO_CHARS_LEADING_ZEROS) == 10);
	CHECK(to_chars(NULL, 0, (int64_t)0, TO_CHARS_DECIMAL | TO_CHARS_LEADING_ZEROS) == 19);

	CHECK(to_chars(NULL, 0, (int8_t)0, TO_CHARS_BINARY) == 1);
	CHECK(to_chars(NULL, 0, (int8_t)0, TO_CHARS_OCTAL) == 1);
	CHECK(to_chars(NULL, 0, (int8_t)0, TO_CHARS_HEXADECIMAL) == 1);
	CHECK(to_chars(NULL, 0, (int8_t)0, TO_CHARS_DECIMAL) == 1);
	CHECK(to_chars(NULL, 0, (int8_t)0, TO_CHARS_DECIMAL | TO_CHARS_PLUS_SIGN) == 2);

	CHECK(to_chars(NULL, 0, (int64_t)0, TO_CHARS_BINARY) == 1);
	CHECK(to_chars(NULL, 0, (int64_t)0, TO_CHARS_OCTAL) == 1);
	CHECK(to_chars(NULL, 0, (int64_t)0, TO_CHARS_HEXADECIMAL) == 1);
	CHECK(to_chars(NULL, 0, (int64_t)0, TO_CHARS_DECIMAL) == 1);
	CHECK(to_chars(NULL, 0, (int64_t)0, TO_CHARS_DECIMAL | TO_CHARS_PLUS_SIGN) == 2);

	len = to_chars(str, sizeof(str), (uint8_t)0, TO_CHARS_HEXADECIMAL | TO_CHARS_LEADING_ZEROS);
	CHECK_TO_CHARS("00");
	len = to_chars(str, sizeof(str), (int8_t)0, TO_CHARS_HEXADECIMAL | TO_CHARS_LEADING_ZEROS);
	CHECK_TO_CHARS("00");
	len = to_chars(str, sizeof(str), (uint16_t)0, TO_CHARS_HEXADECIMAL | TO_CHARS_LEADING_ZEROS);
	CHECK_TO_CHARS("0000");
	len = to_chars(str, sizeof(str), (int16_t)0, TO_CHARS_HEXADECIMAL | TO_CHARS_LEADING_ZEROS);
	CHECK_TO_CHARS("0000");
	len = to_chars(str, sizeof(str), (uint32_t)0, TO_CHARS_HEXADECIMAL | TO_CHARS_LEADING_ZEROS);
	CHECK_TO_CHARS("00000000");
	len = to_chars(str, sizeof(str), (int32_t)0, TO_CHARS_HEXADECIMAL | TO_CHARS_LEADING_ZEROS);
	CHECK_TO_CHARS("00000000");
	len = to_chars(str, sizeof(str), (uint64_t)0, TO_CHARS_HEXADECIMAL | TO_CHARS_LEADING_ZEROS);
	CHECK_TO_CHARS("0000000000000000");
	len = to_chars(str, sizeof(str), (int64_t)0, TO_CHARS_HEXADECIMAL | TO_CHARS_LEADING_ZEROS);
	CHECK_TO_CHARS("0000000000000000");

	len = to_chars(str, sizeof(str), (uint8_t)0, TO_CHARS_OCTAL | TO_CHARS_LEADING_ZEROS);
	CHECK_TO_CHARS("000");
	len = to_chars(str, sizeof(str), (int8_t)0, TO_CHARS_OCTAL | TO_CHARS_LEADING_ZEROS);
	CHECK_TO_CHARS("000");
	len = to_chars(str, sizeof(str), (uint16_t)0, TO_CHARS_OCTAL | TO_CHARS_LEADING_ZEROS);
	CHECK_TO_CHARS("000000");
	len = to_chars(str, sizeof(str), (int16_t)0, TO_CHARS_OCTAL | TO_CHARS_LEADING_ZEROS);
	CHECK_TO_CHARS("000000");
	len = to_chars(str, sizeof(str), (uint32_t)0, TO_CHARS_OCTAL | TO_CHARS_LEADING_ZEROS);
	CHECK_TO_CHARS("00000000000");
	len = to_chars(str, sizeof(str), (int32_t)0, TO_CHARS_OCTAL | TO_CHARS_LEADING_ZEROS);
	CHECK_TO_CHARS("00000000000");
	len = to_chars(str, sizeof(str), (uint64_t)0, TO_CHARS_OCTAL | TO_CHARS_LEADING_ZEROS);
	CHECK_TO_CHARS("0000000000000000000000");
	len = to_chars(str, sizeof(str), (int64_t)0, TO_CHARS_OCTAL | TO_CHARS_LEADING_ZEROS);
	CHECK_TO_CHARS("0000000000000000000000");

	len = to_chars(str, sizeof(str), (uint8_t)0, TO_CHARS_BINARY | TO_CHARS_LEADING_ZEROS);
	CHECK_TO_CHARS("00000000");
	len = to_chars(str, sizeof(str), (int8_t)0, TO_CHARS_BINARY | TO_CHARS_LEADING_ZEROS);
	CHECK_TO_CHARS("00000000");
	len = to_chars(str, sizeof(str), (uint16_t)0, TO_CHARS_BINARY | TO_CHARS_LEADING_ZEROS);
	CHECK_TO_CHARS("0000000000000000");
	len = to_chars(str, sizeof(str), (int16_t)0, TO_CHARS_BINARY | TO_CHARS_LEADING_ZEROS);
	CHECK_TO_CHARS("0000000000000000");
	len = to_chars(str, sizeof(str), (uint32_t)0, TO_CHARS_BINARY | TO_CHARS_LEADING_ZEROS);
	CHECK_TO_CHARS("00000000000000000000000000000000");
	len = to_chars(str, sizeof(str), (int32_t)0, TO_CHARS_BINARY | TO_CHARS_LEADING_ZEROS);
	CHECK_TO_CHARS("00000000000000000000000000000000");
	len = to_chars(str, sizeof(str), (uint64_t)0, TO_CHARS_BINARY | TO_CHARS_LEADING_ZEROS);
	CHECK_TO_CHARS("0000000000000000000000000000000000000000000000000000000000000000");
	len = to_chars(str, sizeof(str), (int64_t)0, TO_CHARS_BINARY | TO_CHARS_LEADING_ZEROS);
	CHECK_TO_CHARS("0000000000000000000000000000000000000000000000000000000000000000");

	len = to_chars(str, sizeof(str), (uint8_t)-1, TO_CHARS_HEXADECIMAL | TO_CHARS_LEADING_ZEROS);
	CHECK_TO_CHARS("ff");
	len = to_chars(str, sizeof(str), (int8_t)-1, TO_CHARS_HEXADECIMAL | TO_CHARS_LEADING_ZEROS);
	CHECK_TO_CHARS("-01");
	len = to_chars(str, sizeof(str), (uint16_t)-1, TO_CHARS_HEXADECIMAL | TO_CHARS_LEADING_ZEROS);
	CHECK_TO_CHARS("ffff");
	len = to_chars(str, sizeof(str), (int16_t)-1, TO_CHARS_HEXADECIMAL | TO_CHARS_LEADING_ZEROS);
	CHECK_TO_CHARS("-0001");
	len = to_chars(str, sizeof(str), (uint32_t)-1, TO_CHARS_HEXADECIMAL | TO_CHARS_LEADING_ZEROS);
	CHECK_TO_CHARS("ffffffff");
	len = to_chars(str, sizeof(str), (int32_t)-1, TO_CHARS_HEXADECIMAL | TO_CHARS_LEADING_ZEROS);
	CHECK_TO_CHARS("-00000001");
	len = to_chars(str, sizeof(str), (uint64_t)-1, TO_CHARS_HEXADECIMAL | TO_CHARS_LEADING_ZEROS);
	CHECK_TO_CHARS("ffffffffffffffff");
	len = to_chars(str, sizeof(str), (int64_t)-1, TO_CHARS_HEXADECIMAL | TO_CHARS_LEADING_ZEROS);
	CHECK_TO_CHARS("-0000000000000001");

	len = to_chars(str, sizeof(str), (uint8_t)-1, TO_CHARS_OCTAL | TO_CHARS_LEADING_ZEROS);
	CHECK_TO_CHARS("377");
	len = to_chars(str, sizeof(str), (int8_t)-1, TO_CHARS_OCTAL | TO_CHARS_LEADING_ZEROS);
	CHECK_TO_CHARS("-001");
	len = to_chars(str, sizeof(str), (uint16_t)-1, TO_CHARS_OCTAL | TO_CHARS_LEADING_ZEROS);
	CHECK_TO_CHARS("177777");
	len = to_chars(str, sizeof(str), (int16_t)-1, TO_CHARS_OCTAL | TO_CHARS_LEADING_ZEROS);
	CHECK_TO_CHARS("-000001");
	len = to_chars(str, sizeof(str), (uint32_t)-1, TO_CHARS_OCTAL | TO_CHARS_LEADING_ZEROS);
	CHECK_TO_CHARS("37777777777");
	len = to_chars(str, sizeof(str), (int32_t)-1, TO_CHARS_OCTAL | TO_CHARS_LEADING_ZEROS);
	CHECK_TO_CHARS("-00000000001");
	len = to_chars(str, sizeof(str), (uint64_t)-1, TO_CHARS_OCTAL | TO_CHARS_LEADING_ZEROS);
	CHECK_TO_CHARS("1777777777777777777777");
	len = to_chars(str, sizeof(str), (int64_t)-1, TO_CHARS_OCTAL | TO_CHARS_LEADING_ZEROS);
	CHECK_TO_CHARS("-0000000000000000000001");

	len = to_chars(str, sizeof(str), (uint8_t)-1, TO_CHARS_BINARY | TO_CHARS_LEADING_ZEROS);
	CHECK_TO_CHARS("11111111");
	len = to_chars(str, sizeof(str), (int8_t)-1, TO_CHARS_BINARY | TO_CHARS_LEADING_ZEROS);
	CHECK_TO_CHARS("-00000001");
	len = to_chars(str, sizeof(str), (uint16_t)-1, TO_CHARS_BINARY | TO_CHARS_LEADING_ZEROS);
	CHECK_TO_CHARS("1111111111111111");
	len = to_chars(str, sizeof(str), (int16_t)-1, TO_CHARS_BINARY | TO_CHARS_LEADING_ZEROS);
	CHECK_TO_CHARS("-0000000000000001");
	len = to_chars(str, sizeof(str), (uint32_t)-1, TO_CHARS_BINARY | TO_CHARS_LEADING_ZEROS);
	CHECK_TO_CHARS("11111111111111111111111111111111");
	len = to_chars(str, sizeof(str), (int32_t)-1, TO_CHARS_BINARY | TO_CHARS_LEADING_ZEROS);
	CHECK_TO_CHARS("-00000000000000000000000000000001");
	len = to_chars(str, sizeof(str), (uint64_t)-1, TO_CHARS_BINARY | TO_CHARS_LEADING_ZEROS);
	CHECK_TO_CHARS("1111111111111111111111111111111111111111111111111111111111111111");
	len = to_chars(str, sizeof(str), (int64_t)-1, TO_CHARS_BINARY | TO_CHARS_LEADING_ZEROS);
	CHECK_TO_CHARS("-0000000000000000000000000000000000000000000000000000000000000001");

	for (unsigned long long i = 2; i <= 36; i++) {
		len = to_chars(str, sizeof(str), 0, i);
		CHECK_TO_CHARS("0");
		len = to_chars(str, sizeof(str), 1, i);
		CHECK_TO_CHARS("1");
		len = to_chars(str, sizeof(str), i, i);
		CHECK_TO_CHARS("10");
		len = to_chars(str, sizeof(str), i * i, i);
		CHECK_TO_CHARS("100");
		len = to_chars(str, sizeof(str), i * i * i, i);
		CHECK_TO_CHARS("1000");
		len = to_chars(str, sizeof(str), i * i * i * i, i);
		CHECK_TO_CHARS("10000");
		len = to_chars(str, sizeof(str), i * i * i * i * i, i);
		CHECK_TO_CHARS("100000");
		len = to_chars(str, sizeof(str), i * i * i * i * i * i, i);
		CHECK_TO_CHARS("1000000");
		len = to_chars(str, sizeof(str), i * i * i * i * i * i * i, i);
		CHECK_TO_CHARS("10000000");
		len = to_chars(str, sizeof(str), i * i * i * i * i * i * i * i, i);
		CHECK_TO_CHARS("100000000");
	}

	memset(str, 'x', 10);
	CHECK(to_chars(str, 0, 1234567890, TO_CHARS_DECIMAL) == 10);
	CHECK(to_chars(str, 9, 1234567890, TO_CHARS_DECIMAL) == 10);
	CHECK(memcmp(str, "xxxxxxxxxx", 10) == 0);
	len = to_chars(str, 10, 1234567890, TO_CHARS_DECIMAL);
	CHECK_TO_CHARS("1234567890");

	// the unsigned max value in base 3 has one more char than the signed max value
	len = to_chars(str, sizeof(str), (int)1, 3 | TO_CHARS_LEADING_ZEROS);
	CHECK_TO_CHARS("00000000000000000001");
	len = to_chars(str, sizeof(str), (unsigned int)1, 3 | TO_CHARS_LEADING_ZEROS);
	CHECK_TO_CHARS("000000000000000000001");
	// same with base 6, test with + sign and negative value
	len = to_chars(str, sizeof(str), (int)1, 6 | TO_CHARS_LEADING_ZEROS | TO_CHARS_PLUS_SIGN);
	CHECK_TO_CHARS("+000000000001");
	len = to_chars(str, sizeof(str), (int)-1, 6 | TO_CHARS_LEADING_ZEROS | TO_CHARS_PLUS_SIGN);
	CHECK_TO_CHARS("-000000000001");
	len = to_chars(str, sizeof(str), (unsigned int)1, 6 | TO_CHARS_LEADING_ZEROS);
	CHECK_TO_CHARS("0000000000001");

	return true;
}

RANDOM_TEST(to_chars_random, 1u << 22)
{
	return check_from_number(random_seed);
}

RANGE_TEST(to_chars_bases, 2, 36)
{
	unsigned int base = value;
	const char *alphabet = "0123456789abcdefghijklmnopqrstuvwxyz";
	assert(base <= strlen(alphabet));
	unsigned char digits[64] = {0};
	char buf[64];
	memset(buf, alphabet[0], sizeof(buf));
	for (uint64_t i = 0; i < (1u << 20); i++) {
		char str[64];
		size_t len = to_chars(str, sizeof(str), i, base);
		CHECK(memcmp(str, &buf[sizeof(buf) - len], len) == 0);

		for (unsigned int j = sizeof(digits); j-- > 0;) {
			digits[j]++;
			if (digits[j] == base) {
				digits[j] = 0;
			}
			buf[j] = alphabet[digits[j]];
			if (digits[j] != 0) {
				break;
			}
		}
	}
	return true;
}

#if _FORTIFY_SOURCE >= 1 && (defined(HAVE_BUILTIN_DYNAMIC_OBJECT_SIZE) || defined(HAVE_BUILTIN_OBJECT_SIZE))

NEGATIVE_SIMPLE_TEST(to_chars_fortified)
{
	char str[5];
	to_chars(str, 6, 123, 0);
	return true;
}

#define TO_CHARS_FORTIFY_TEST(name, type)			\
	NEGATIVE_SIMPLE_TEST(to_chars_##name##_fortified)	\
	{							\
		char str[5];					\
		to_chars_##name(str, 6, 123, 0);		\
		return true;					\
	}							\

__CHARCONV_FOREACH_INTTYPE(TO_CHARS_FORTIFY_TEST)

#endif
