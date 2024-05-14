#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

#define error_on(expr, ...) (!(expr) ? (void)0 : error(__VA_ARGS__))
#define error(...) _error(__FILE__, __LINE__, __func__, __VA_ARGS__)

static void __attribute__ ((format (printf, 4, 5)))
_error(const char *file, const unsigned int line, const char *func, const char *fmt, ...)
{
	fputs("Error: ", stderr);

	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);

	if (errno != 0) {
		fprintf(stderr, ": %s", strerror(errno));
	}

	fprintf(stderr, " (%s:%u '%s')\n", file, line, func);

	exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
	error_on(argc < 2, "No filename specified");
	FILE *f = fopen(argv[1], "r");
	error_on(!f, "Unable to open file '%s'", argv[1]);
}
