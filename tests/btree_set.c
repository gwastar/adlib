#include "btree_test.h"

RANDOM_TEST(btree_set, random_seed, 2)
{
	return btree_set_test(random_seed);
}
