#include <signal.h>
#include <stdio.h>
#include "testing.h"

NEGATIVE_SIMPLE_TEST(selftest1)
{
	raise(SIGABRT);
	return true;
}

NEGATIVE_SIMPLE_TEST(selftest2)
{
	return false;
}

RANGE_TEST(selftest3, UINT64_MAX - 1, UINT64_MAX)
{
	CHECK(UINT64_MAX - 1 <= value && value <= UINT64_MAX);
	return true;
}

static bool invert(bool b)
{
	return !b;
}

RANDOM_TEST(selftest4, 2)
{
	(void)random_seed;
#if 0
	if (random_seed % 2 == 0) {
		fputs("Hello world", stdout);
		return false;
	}
#endif
	return true;
}

RANGE_TEST(selftest5, 3, 7)
{
	int a = 2;
	char str[] = "abc";
	CHECK(AND(EQ(str, "abd"), DEBUG(a)));
	CHECK(AND(EQ(str, "abc"), NE(str, "abcd")));
	CHECK(AND(FALSE(a == 1), TRUE(a == 2)));
	CHECK(OR(NOT(EQ(a, 3)), EQ(value, value + 1)));
	CHECK(AND(NOT(EQ(a, 1 + 2)), EQ(value, value)));
	TRY(true);
	TRY(invert(false));
	return true;
}

NEGATIVE_SIMPLE_TEST(selftest6)
{
	TRY(false);
	return true;
}
