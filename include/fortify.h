#ifndef __FORTIFY_INCLUDE__
#define __FORTIFY_INCLUDE__

#include "compiler.h"

// TODO compile-time check (figure out a way that doesn't need __attribute__((error(msg)))?)

#if _FORTIFY_SOURCE >= 1
# define __FORTIFY_ENABLED 1
# if _FORTIFY_SOURCE >= 3
#  define _fortify_bos(ptr) _bdos(ptr, 1)
# elif _FORTIFY_SOURCE == 2
#  define _fortify_bos(ptr) _bos(ptr, 1)
# elif _FORTIFY_SOURCE == 1
#  define _fortify_bos(ptr) _bos(ptr, 0)
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

#endif
