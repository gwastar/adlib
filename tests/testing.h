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

#include <stdbool.h>
#include <stdint.h>
#include "compiler.h"

_Static_assert(HAVE_ATTR_CONSTRUCTOR, "");

#define CHECK(cond)							\
	do {								\
		if (unlikely(!(cond)))  {				\
			check_failed(__func__, __FILE__, __LINE__, #cond); \
			return false;					\
		}							\
	} while (0)

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
	static bool test_##name(uint64_t start, uint64_t end);		\
	static _attr_constructor void register_simple_test_##name(void)	\
	{								\
		register_range_test(__FILE__, #name, start_, end_, test_##name, should_succeed); \
	}								\
	static bool test_##name(uint64_t start, uint64_t end)

#define _RANDOM_TEST(name, num_values, should_succeed)			\
	static bool test_##name(uint64_t);				\
	static _attr_constructor void register_simple_test_##name(void)	\
	{								\
		register_random_test(__FILE__, #name, num_values, test_##name, should_succeed); \
	}								\
	static bool test_##name(uint64_t random_seed)


void register_simple_test(const char *file, const char *name,
			  bool (*f)(void), bool should_succeed);
void register_range_test(const char *file, const char *name, uint64_t start, uint64_t end,
			 bool (*f)(uint64_t start, uint64_t end), bool should_succeed);
void register_random_test(const char *file, const char *name, uint64_t num_values,
			  bool (*f)(uint64_t), bool should_succeed);

void check_failed(const char *func, const char *file, unsigned int line, const char *cond);
