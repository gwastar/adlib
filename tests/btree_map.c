#define STRING_MAP
#include "btree_test.h"

RANDOM_TEST(btree_map, 2, 0, UINT64_MAX)
{
	return btree_map_test(random);
}
