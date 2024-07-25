#define STRING_MAP
#include "btree_test.h"

RANDOM_TEST(btree_map, random_seed, 2)
{
	return btree_map_test(random_seed);
}
