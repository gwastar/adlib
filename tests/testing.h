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

#include <inttypes.h>
#include <stdbool.h>
#include <string.h>
#include "compiler.h"

_Static_assert(HAVE_ATTR_CONSTRUCTOR, "");

#define CHECK(cond)							\
	do {								\
		__extension__ __auto_type c = (cond);			\
		if (unlikely(!_Generic(c,				\
				       struct check *:			\
				       _check_cond(_Generic(c,		\
							    struct check *: c, \
							    default: 0), \
						   __func__, __FILE__, __LINE__), \
				       default:				\
				       _check_bool(_Generic(c,	\
							    struct check *: 0, \
							    default: c), \
						   __func__, __FILE__, __LINE__, #cond))))  { \
			return false;					\
		}							\
	} while (0)

#define __TEST_FOREACH_TYPE_WITH_ARGS(f, ...)		\
	f(bool, bool, __VA_ARGS__)			\
	f(char, char, __VA_ARGS__)			\
	f(schar, signed char, __VA_ARGS__)		\
	f(uchar, unsigned char, __VA_ARGS__)		\
	f(short, short, __VA_ARGS__)			\
	f(ushort, unsigned short, __VA_ARGS__)		\
	f(int, int, __VA_ARGS__)			\
	f(uint, unsigned int, __VA_ARGS__)		\
	f(long, long, __VA_ARGS__)			\
	f(ulong, unsigned long, __VA_ARGS__)		\
	f(llong, long long, __VA_ARGS__)		\
	f(ullong, unsigned long long, __VA_ARGS__)	\
	f(pointer, const void *, __VA_ARGS__)		\
	f(string, const char *, __VA_ARGS__)		\

#define __TEST_FOREACH_TYPE(f) __TEST_FOREACH_TYPE_WITH_ARGS(f, dummy)

struct primitive {
	enum data_type {
#define E(name, ...) DATA_TYPE_##name,
		__TEST_FOREACH_TYPE(E)
#undef E
	} type;
	const char *code;
	union {
#define F(name, type, ...) type _##name;
		__TEST_FOREACH_TYPE(F)
#undef F
	};
};

#define __TEST_MAKE_CONST(val)						\
	_Generic((val),							\
		 char *: (const char *)(_Generic((val), char *: (val), default: 0)), \
		 void *: (const void *)(_Generic((val), void *: (val), default: 0)), \
		 default: (val))

#define __TEST_PRIMITIVE_GENERIC_CASE(name_, type_, val)		\
	type_: (&(struct primitive){ .type = DATA_TYPE_##name_, .code = #val, ._##name_ = (type_)_Generic(val, type_: (val), default: 0)}),

struct _test_dummy_type { int i; };

#define PRIMITIVE(val)							\
	_Generic((val),							\
		 __TEST_FOREACH_TYPE_WITH_ARGS(__TEST_PRIMITIVE_GENERIC_CASE, val) \
		 __TEST_PRIMITIVE_GENERIC_CASE(string, char *, val)	\
		 __TEST_PRIMITIVE_GENERIC_CASE(pointer, void *, val)	\
		 struct _test_dummy_type: 0)

struct check {
	enum check_type {
		CHECK_TYPE_AND,
		CHECK_TYPE_OR,
		CHECK_TYPE_BOOL,
		CHECK_TYPE_COMPARE_EQ,
		CHECK_TYPE_COMPARE_LT,
		CHECK_TYPE_DEBUG,
	} check_type;
	bool invert;
	bool checked;
	bool passed;
	union {
		struct check *check_args[2];
		struct primitive *primitive_args[2];
	};
};

static inline struct check *_invert_check(struct check *check)
{
	check->invert = !check->invert;
	return check;
}

#if 0
#define __TEST_CHECK_COMPARISON(cmp, a, b, invert_)			\
	_Generic(__TEST_MAKE_CONST(a),					\
		 typeof(__TEST_MAKE_CONST(b)): (&(struct check) { .check_type = CHECK_TYPE_COMPARE_##cmp, \
				 .invert = (invert_), .primitive_args = { PRIMITIVE(a), PRIMITIVE(b) } }))
#define __TEST_BOOL(a, invert_)						\
	(&(struct check) { .check_type = CHECK_TYPE_BOOL, .invert = (invert_), \
		 .primitive_args = { PRIMITIVE(a) }})
#define AND(a, b)							\
	(&(struct check) { .check_type = CHECK_TYPE_AND, .check_args = {(a), (b)} })
#define OR(a, b)							\
	(&(struct check) { .check_type = CHECK_TYPE_OR, .check_args = {(a), (b)} })
#define NOT(a)								\
	_invert_check(a)
#define EQ(a, b) __TEST_CHECK_COMPARISON(EQ, a, b, false)
#define NE(a, b) __TEST_CHECK_COMPARISON(EQ, a, b, true)
#define LT(a, b) __TEST_CHECK_COMPARISON(LT, a, b, false)
#define GE(a, b) __TEST_CHECK_COMPARISON(LT, a, b, true)
#define GT(a, b) __TEST_CHECK_COMPARISON(LT, b, a, false)
#define LE(a, b) __TEST_CHECK_COMPARISON(LT, b, a, true)
#define TRUE(a) __TEST_BOOL(a, false)
#define FALSE(a) __TEST_BOOL(a, true)
#define DEBUG(a) (&(struct check) { .check_type = CHECK_TYPE_DEBUG, .checked = true, .primitive_args = {PRIMITIVE(a)} })
#else
#define ___TEST_CMP(cmp, a, b)						\
	_Generic((a), typeof(b): _Generic((a), const char *: strcmp(_Generic((a), const char *: (a), default: ""), _Generic((b), const char *: (b), default: "")) cmp 0, default: _Generic((a), const char *: 0, default: (a)) cmp (_Generic((b), const char *: 0, default: (b)))))
#define __TEST_CMP(cmp, a, b) ___TEST_CMP(cmp, __TEST_MAKE_CONST(a), __TEST_MAKE_CONST(b))
#define AND(a, b) ((a) && (b))
#define OR(a, b) ((a) || (b))
#define NOT(a) (!(a))
#define EQ(a, b) __TEST_CMP(==, a, b)
#define NE(a, b) __TEST_CMP(!=, a, b)
#define LT(a, b) __TEST_CMP(<, a, b)
#define GE(a, b) __TEST_CMP(>=, a, b)
#define GT(a, b) __TEST_CMP(>, a, b)
#define LE(a, b) __TEST_CMP(<=, a, b)
#define TRUE(a) ((bool)(a))
#define FALSE(a) ((bool)!(a))
#define DEBUG(a) true
#endif

#define TRY(function_call)				\
	do {						\
		if (!(function_call)) return false;	\
	} while (0)

bool _evaluate_check(struct check *check);
void _print_check_failure(struct check *check, const char *func, const char *file, unsigned int line);

#define SIMPLE_TEST(name)			\
	_SIMPLE_TEST(name, true)
#define RANGE_TEST(name, start_, end_)		\
	_RANGE_TEST(name, start_, end_, true)
#define RANDOM_TEST(name, num_values)		\
	_RANDOM_TEST(name, num_values, true)

#define NEGATIVE_SIMPLE_TEST(name)		\
	_SIMPLE_TEST(name, false)

#define _SIMPLE_TEST(name, should_succeed)				\
	static bool test_##name(void);					\
	static _attr_constructor void register_simple_test_##name(void)	\
	{								\
		register_simple_test(__FILE__, #name, test_##name, should_succeed); \
	}								\
	static bool test_##name(void)

#define _RANGE_TEST(name, start_, end_, should_succeed)			\
	static bool test_##name(uint64_t);				\
	static bool _test_##name##_helper(uint64_t _start, uint64_t _end) \
	{								\
		for (uint64_t _x = _start; _x <= _end && _x >= _start; _x++) { \
			if (unlikely(!test_##name(_x))) {		\
				test_log("test failed with input: %" PRIu64 "\n", _x); \
				return false;				\
			}						\
		}							\
		return true;						\
	}								\
	static _attr_constructor void register_simple_test_##name(void)	\
	{								\
		register_range_test(__FILE__, #name, start_, end_, _test_##name##_helper, should_succeed); \
	}								\
	static bool test_##name(uint64_t value)

#define _RANDOM_TEST(name, num_values, should_succeed)			\
	static bool test_##name(uint64_t);				\
	static bool _test_##name##_helper(uint64_t _num_values, uint64_t _seed) \
	{								\
		for (uint64_t _i = 0; _i < _num_values; _i++) {		\
			/* splitmix64 */				\
			uint64_t _z = (_seed += 0x9e3779b97f4a7c15);	\
			_z = (_z ^ (_z >> 30)) * 0xbf58476d1ce4e5b9;	\
			_z = (_z ^ (_z >> 27)) * 0x94d049bb133111eb;	\
			_z = _z ^ (_z >> 31);				\
			if (unlikely(!test_##name(_z))) {		\
				test_log("test failed with input: %" PRIu64 "\n", _z); \
				return false;				\
			}						\
		}							\
		return true;						\
	}								\
	static _attr_constructor void register_simple_test_##name(void)	\
	{								\
		register_random_test(__FILE__, #name, num_values, _test_##name##_helper, should_succeed); \
	}								\
	static bool test_##name(uint64_t random_seed)


void register_simple_test(const char *file, const char *name,
			  bool (*f)(void), bool should_succeed);
void register_range_test(const char *file, const char *name, uint64_t start, uint64_t end,
			 bool (*f)(uint64_t start, uint64_t end), bool should_succeed);
void register_random_test(const char *file, const char *name, uint64_t num_values,
			  bool (*f)(uint64_t num_values, uint64_t seed), bool should_succeed);

void check_failed(const char *func, const char *file, unsigned int line, const char *cond);

void test_log(const char *fmt, ...) _attr_format_printf(1, 2);

static inline bool _check_bool(bool cond, const char *func, const char *file, unsigned int line,
			      const char *cond_string)
{
	if (cond)  {
		return true;
	}
	check_failed(func, file, line, cond_string);
	return false;
}

static inline bool _check_cond(struct check *check, const char *func, const char *file, unsigned int line)
{
	if (_evaluate_check(check))  {
		return true;
	}
	_print_check_failure(check, func, file, line);
	return false;
}
