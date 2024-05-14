#ifndef __string_include__
#define __string_include__

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>

static char * __attribute__ ((format (printf, 1, 2)))
mprintf(const char *fmt, ...)
{
	char buf[256];

	va_list args;
	va_start(args, fmt);
	size_t n = vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	char *str = malloc(n + 1);
	if (!str) {
		return NULL;
	}

	if (n < sizeof(buf)) {
		memcpy(str, buf, n + 1);
		return str;
	}

	va_start(args, fmt);
	vsnprintf(str, n + 1, fmt, args);
	va_end(args);

	return str;
}

#endif
