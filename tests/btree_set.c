#include "btree_test.h"

RANDOM_TEST(btree_set, 2, 0, UINT64_MAX)
{
	return btree_set_test(random);
}
