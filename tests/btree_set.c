#include "btree_test.h"

RANDOM_TEST(btree_set, 2)
{
	return btree_set_test(random_seed);
}
