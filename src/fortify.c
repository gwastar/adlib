#include <stdio.h>
#include <stdlib.h>
#include "fortify.h"

void _fortify_check_failed(const char *cond, const char *file, unsigned int line)
{
	fprintf(stderr, "Error: fortify check failed: %s (%s:%u)\n", cond, file, line);
	abort();
}
