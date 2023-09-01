#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "mprintf.h"

int main(void)
{
#if 1
	for (unsigned int i = 0; i < 1000000; i++) {
		char *str = mprintf("%s %i %.3f\n", "abc", 123456, 123.456);
		free(str);
		str = mprintf("%s %s %i %i %.3f\n", "abcdef", "123 456", 123, 456, 123.456);
		free(str);
		str = mprintf("%s %s %i %i %i %.3f\n", "abcdefghi", "123 456 789", 123, 456, 789, 123456.789);
		free(str);
		str = mprintf("%s %s %i %i %i %.3f %f %u %x\n", "abcdefghi", "123 456 789", 123, 456, 789, 123456.789, 3.1456273960, -1, 0xdeadbeef);
		free(str);
		str = mprintf("%s/%s", "/home", "user");
		free(str);
	}

#else

	char *str = mprintf("%s %i %.3f\n", "abc", 123456, 123.456);
	fputs(str, stdout);
	free(str);

	str = mprintf("%s %s %i %i %.3f\n", "abcdef", "123 456", 123, 456, 123.456);
	fputs(str, stdout);
	free(str);

	str = mprintf("%s %s %i %i %i %.3f\n", "abcdefghi", "123 456 789", 123, 456, 789, 123456.789);
	fputs(str, stdout);
	free(str);

	str = mprintf("%s %s %i %i %i %.3f %f %u %x\n", "abcdefghi", "123 456 789", 123, 456, 789, 123456.789, 3.1456273960, -1, 0xdeadbeef);
	fputs(str, stdout);
	free(str);

	str = mprintf("/%s/%s/%s/%s/%s", "home", "user", "something", "foo", "bar");
	fputs(str, stdout);
	free(str);

	putchar('\n');
#endif
}
