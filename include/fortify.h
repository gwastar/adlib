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

// TODO compile-time check (figure out a way that doesn't need __attribute__((error(msg)))?)

#if _FORTIFY_SOURCE >= 1
# define __FORTIFY_ENABLED 1
# if _FORTIFY_SOURCE >= 3
#  define _fortify_bos(ptr) _bdos(ptr, 1) // accesses in range _fortify_bos() <= byte_idx are out of bounds
#  define _fortify_bos_safe(ptr) _bdos(ptr, 3) // accesses in range 0 <= byte_idx < _fortify_bos_safe() are safe
# elif _FORTIFY_SOURCE == 2
#  define _fortify_bos(ptr) _bos(ptr, 1)
#  define _fortify_bos_safe(ptr) _bos(ptr, 3)
# elif _FORTIFY_SOURCE == 1
#  define _fortify_bos(ptr) _bos(ptr, 0)
#  define _fortify_bos_safe(ptr) _bos(ptr, 2)
#endif
# define _fortify_check(cond)						\
	((void)(likely(cond) || (_fortify_check_failed(#cond, __FILE__, __LINE__), 0)))
#else
# define _fortify_bos(ptr) ((size_t)-1)
# define _fortify_check(cond) ((void)0)
#endif

// TODO there are currently some false positives with the array functions
#if 0 && defined(__OPTIMIZE__) && defined(HAVE_BUILTIN_CONSTANT_P) && defined(_attr_error)
void _fortify_compiletime_error(void) _attr_error("fortify check failed");
# define _fortify_check_failed(cond, file, line) (__builtin_constant_p(cond) ? _fortify_compiletime_error() : _fortify_runtime_check_failed(#cond, file, line))
#else
# define _fortify_check_failed(cond, file, line) _fortify_runtime_check_failed(#cond, file, line)
#endif

void _fortify_runtime_check_failed(const char *cond, const char *file, unsigned int line) _attr_noreturn;
