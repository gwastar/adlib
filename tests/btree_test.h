#pragma once

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "btree.h"
#include "hash.h"
#include "hashtable.h"
#include "random.h"
#include "testing.h"

// #define STRING_MAP

#ifdef STRING_MAP
DEFINE_BTREE_MAP(btree, char *, uint32_t, NULL, NULL, 128, strcmp(a, b))
#else
DEFINE_BTREE_SET(btree, int64_t, NULL, 127, (a < b) ? -1 : (a > b))
#endif
DEFINE_HASHTABLE(btable, btree_key_t, btree_key_t, 8, *key == *entry)

static bool btree_check_node(struct _btree_node *node, unsigned int height, const struct btree_info *info);

static bool btree_check_children(struct _btree_node *node, unsigned int height, const struct btree_info *info)
{
	CHECK(height != 0);
	if (height == 1) {
		return true;
	}
	for (unsigned int i = 0; i < node->num_items + 1u; i++) {
		if (!btree_check_node(_btree_debug_node_get_child(node, i, info), height - 1, info)) {
			return false;
		}
	}
	return true;
}

static bool btree_check_node_sorted(struct _btree_node *node, const struct btree_info *info)
{
	for (unsigned int i = 1; i < node->num_items; i++) {
		CHECK(info->cmp(_btree_debug_node_item(node, i - 1, info),
				_btree_debug_node_item(node, i, info)) < 0);
	}
	return true;
}

static bool btree_check_node(struct _btree_node *node, unsigned int height, const struct btree_info *info)
{
	CHECK(node->num_items >= info->min_items && node->num_items <= info->max_items);
	return btree_check_node_sorted(node, info) && btree_check_children(node, height, info);
}

static bool btree_check(const struct btree *btree, const struct btree_info *info)
{
	struct _btree_node *root = btree->_impl.root;
	if (btree->_impl.height == 0) {
		CHECK(!root);
		return true;
	}
	CHECK(root->num_items >= 1 && root->num_items <= info->max_items);
	return btree_check_children(root, btree->_impl.height, info);
}

#ifdef STRING_MAP
#define __KEY_SIZE 15
#define KEY_SIZE (__KEY_SIZE + 1)
#define KEY_FORMAT __KEY_FORMAT(__KEY_SIZE)
#define __KEY_FORMAT(S) ___KEY_FORMAT(S)
#define ___KEY_FORMAT(S) "%0" #S "zu"

static btree_key_t *create_keys(size_t num_keys)
{
	btree_key_t *keys = malloc(num_keys * sizeof(keys[0]));
	for (size_t i = 0; i < num_keys; i++) {
		keys[i] = malloc(KEY_SIZE);
		snprintf(keys[i], KEY_SIZE, KEY_FORMAT, i);
	}
	return keys;
}

static void destroy_keys(btree_key_t *keys, size_t num_keys)
{
	for (size_t i = 0; i < num_keys; i++) {
		free(keys[i]);
	}
	free(keys);
}

static _attr_pure btable_hash_t get_hash(const char *key)
{
	return murmurhash3_x64_64(key, KEY_SIZE, 0).u64;
}

static btree_key_t get_key(btree_key_t *keys, size_t x)
{
	return keys[x];
}
#else
static btree_key_t *create_keys(size_t num_keys)
{
	(void)num_keys;
	return NULL;
}
static void destroy_keys(btree_key_t *keys, size_t num_keys)
{
	(void)keys;
	(void)num_keys;
}
static btable_hash_t get_hash(btree_key_t key)
{
	return (btable_hash_t)key;
}
static btree_key_t get_key(btree_key_t *keys, size_t x)
{
	(void)keys;
	return (btree_key_t)x;
}
#endif

// TODO split this up into multiple test functions?
// (would probably have to make keys global with pthread_once)

#ifdef STRING_MAP
static bool btree_map_test(uint64_t random)
#else
static bool btree_set_test(uint64_t random)
#endif
{
	struct random_state rng;
	random_state_init(&rng, random);

	const size_t N = 1 << 14;
	const size_t LIMIT = 1 << 16;

	size_t num_keys = 2 * (N > LIMIT ? N : LIMIT);
	btree_key_t *keys = create_keys(num_keys);

	struct btree btree = BTREE_EMPTY;
	btree_init(&btree);

	struct btable btable;
	btable_init(&btable, N);

	for (size_t i = 0; i < N; i++) {
		size_t x = random_next_u64(&rng) % LIMIT;
		btree_key_t key = get_key(keys, x);
		bool exists = btable_lookup(&btable, key, get_hash(key));
		if (!exists) {
			*btable_insert(&btable, key, get_hash(key)) = key;
		}
#ifdef STRING_MAP
		bool inserted = btree_insert(&btree, key, x);
#else
		bool inserted = btree_insert(&btree, key);
#endif
		CHECK(exists ? !inserted : inserted);
		CHECK(btree_check(&btree, &btree_info));
		CHECK(btree_find(&btree, key));
	}
	for (btable_iter_t iter = btable_iter_start(&btable);
	     !btable_iter_finished(&iter);
	     btable_iter_advance(&iter)) {
		CHECK(btree_find(&btree, *iter.entry));
	}
	{
		btree_iter_t iter;
#ifdef STRING_MAP
		btree_key_t prev_key, key;
		btree_value_t *value = btree_iter_start_leftmost(&iter, &btree, &key);
		CHECK(value && strtoumax(key, NULL, 10) == *value);
		CHECK(btable_lookup(&btable, key, get_hash(key)));
#else
		const btree_key_t *prev_key, *key;
		key = btree_iter_start_leftmost(&iter, &btree);
		CHECK(key);
		CHECK(btable_lookup(&btable, *key, get_hash(*key)));
#endif
		prev_key = key;
#ifdef STRING_MAP
		while ((value = btree_iter_next(&iter, &key))) {
			CHECK(strtoumax(key, NULL, 10) == *value);
			CHECK(btable_lookup(&btable, key, get_hash(key)));
			CHECK(btree_info.cmp(&prev_key, &key) < 0);
			prev_key = key;
		}
#else
		while ((key = btree_iter_next(&iter))) {
			CHECK(btable_lookup(&btable, *key, get_hash(*key)));
			CHECK(btree_info.cmp(prev_key, key) < 0);
			prev_key = key;
		}
#endif
	}
	for (size_t i = LIMIT; i < LIMIT + 1000; i++) {
		CHECK(!btree_find(&btree, get_key(keys, i)));
	}
	for (size_t i = 0; i < N; i++) {
		btree_key_t key = get_key(keys, random_next_u64(&rng) % LIMIT);
		bool exists = btable_remove(&btable, key, get_hash(key), NULL);
#ifdef STRING_MAP
		btree_key_t k;
		btree_value_t v;
		bool removed = btree_delete(&btree, key, &k, &v);
		CHECK(!removed || strtoumax(k, NULL, 10) == v);
#else
		btree_key_t k;
		bool removed = btree_delete(&btree, key, &k);
#endif
		CHECK(exists == removed);
		CHECK(!removed || (k == key));
		CHECK(btree_check(&btree, &btree_info));
		CHECK(!btree_find(&btree, key));
	}

	for (btable_iter_t iter = btable_iter_start(&btable);
	     !btable_iter_finished(&iter);
	     btable_iter_advance(&iter)) {
#ifdef STRING_MAP
		btree_key_t key;
		btree_value_t value;
		bool removed = btree_delete(&btree, *iter.entry, &key, &value);
		CHECK(removed && strtoumax(key, NULL, 10) == value);
#else
		btree_key_t key;
		bool removed = btree_delete(&btree, *iter.entry, &key);
		CHECK(removed);
#endif
		CHECK(key == *iter.entry);
		CHECK(btree_check(&btree, &btree_info));
	}
	btable_clear(&btable);

	CHECK(btree._impl.height == 0); // TODO expose height?
	CHECK(btree_check(&btree, &btree_info));

	for (size_t i = 0; i < N; i++) {
		btree_key_t key = get_key(keys, i);
#ifdef STRING_MAP
		btree_insert_sequential(&btree, key, i);
#else
		btree_insert_sequential(&btree, key);
#endif
		CHECK(btree_check(&btree, &btree_info));
		CHECK(btree_find(&btree, key));
	}

	for (size_t i = 0; i < N; i++) {
		btree_key_t key = get_key(keys, i);
#ifdef STRING_MAP
		bool inserted = btree_set(&btree, key, i + 1);
#else
		bool inserted = btree_set(&btree, key);
#endif
		CHECK(!inserted);
		CHECK(btree_check(&btree, &btree_info));
		CHECK(btree_find(&btree, key));
	}

	{
		btree_iter_t iter;
#ifdef STRING_MAP
		btree_key_t key;
		btree_value_t *value = btree_iter_start_leftmost(&iter, &btree, &key);
		for (size_t i = 0; i < N; i++) {
			CHECK(value);
			CHECK(*value == i + 1);
			*value = i + 2;
			CHECK(key == get_key(keys, i));
			value = btree_iter_next(&iter, &key);
		}
		CHECK(!value);
		CHECK(!btree_iter_next(&iter, &key));
#else
		const btree_key_t *key = btree_iter_start_leftmost(&iter, &btree);
		for (size_t i = 0; i < N; i++) {
			CHECK(key);
			CHECK(*key == get_key(keys, i));
			key = btree_iter_next(&iter);
		}
		CHECK(!key);
		CHECK(!btree_iter_next(&iter));
#endif
#ifdef STRING_MAP
		value = btree_iter_start_rightmost(&iter, &btree, &key);
		for (size_t i = N; i-- > 0;) {
			CHECK(value);
			CHECK(*value == i + 2);
			CHECK(key == get_key(keys, i));
			value = btree_iter_prev(&iter, &key);
		}
		CHECK(!value);
		CHECK(!btree_iter_prev(&iter, &key));
#else
		key = btree_iter_start_rightmost(&iter, &btree);
		for (size_t i = N; i-- > 0;) {
			CHECK(key);
			CHECK(*key == get_key(keys, i));
			key = btree_iter_prev(&iter);
		}
		CHECK(!key);
		CHECK(!btree_iter_prev(&iter));
#endif
		size_t i = 0;
#ifdef STRING_MAP
		for (btree_value_t *value = btree_iter_start_leftmost(&iter, &btree, &key);
		     value;
		     value = btree_iter_next(&iter, &key)) {
			CHECK(i < N);
			CHECK(*value == i + 2);
			CHECK(key == get_key(keys, i));
			i++;
		}
#else
		for (const btree_key_t *key = btree_iter_start_leftmost(&iter, &btree);
		     key;
		     key = btree_iter_next(&iter)) {
			CHECK(i < N);
			CHECK(*key == get_key(keys, i));
			i++;
		}
#endif
		CHECK(i == N);

#ifdef STRING_MAP
		for (size_t i = 0; i < N; i++) {
			value = btree_iter_start_at(&iter, &btree, get_key(keys, i), &key, BTREE_ITER_FIND_KEY);
			CHECK(key == get_key(keys, i));
			CHECK(*value == i + 2);
			value = btree_iter_next(&iter, &key);
			if (i == N - 1) {
				CHECK(!value);
			} else {
				CHECK(key == get_key(keys, i + 1));
				CHECK(*value == i + 3);
			}
		}
#else
		for (size_t i = 0; i < N; i++) {
			key = btree_iter_start_at(&iter, &btree, get_key(keys, i), BTREE_ITER_FIND_KEY);
			CHECK(*key == get_key(keys, i));
			key = btree_iter_next(&iter);
			if (i == N - 1) {
				CHECK(!key);
			} else {
				CHECK(*key == get_key(keys, i + 1));
			}
		}
#endif

#ifdef STRING_MAP
		value = btree_iter_start_at(&iter, &btree, get_key(keys, N / 2), &key, BTREE_ITER_FIND_KEY);
		CHECK(key == get_key(keys, N / 2));
		value = btree_iter_next(&iter, &key);
		CHECK(key == get_key(keys, N / 2 + 1));
		value = btree_iter_prev(&iter, &key);
		CHECK(key == get_key(keys, N / 2));
		value = btree_iter_prev(&iter, &key);
		CHECK(key == get_key(keys, N / 2 - 1));
		value = btree_iter_next(&iter, &key);
		CHECK(key == get_key(keys, N / 2));
#else
		key = btree_iter_start_at(&iter, &btree, get_key(keys, N / 2), BTREE_ITER_FIND_KEY);
		CHECK(*key == get_key(keys, N / 2));
		key = btree_iter_next(&iter);
		CHECK(*key == get_key(keys, N / 2 + 1));
		key = btree_iter_prev(&iter);
		CHECK(*key == get_key(keys, N / 2));
		key = btree_iter_prev(&iter);
		CHECK(*key == get_key(keys, N / 2 - 1));
		key = btree_iter_next(&iter);
		CHECK(*key == get_key(keys, N / 2));
#endif

#ifdef STRING_MAP
		for (size_t i = 0; i < N; i++) {
			value = btree_iter_start_at(&iter, &btree, get_key(keys, i), &key, BTREE_ITER_LOWER_BOUND_INCLUSIVE);
			CHECK(value && key == get_key(keys, i) && *value == i + 2);
			value = btree_iter_start_at(&iter, &btree, get_key(keys, i), &key, BTREE_ITER_LOWER_BOUND_EXCLUSIVE);
			CHECK(i == N - 1 ? !value : value && key == get_key(keys, i + 1) && *value == i + 3);
			bool deleted = btree_delete(&btree, get_key(keys, i), NULL, NULL);
			CHECK(deleted);
			CHECK(!btree_iter_start_at(&iter, &btree, get_key(keys, i), NULL, BTREE_ITER_FIND_KEY));
			value = btree_iter_start_at(&iter, &btree, get_key(keys, i), &key, BTREE_ITER_LOWER_BOUND_INCLUSIVE);
			CHECK(i == N - 1 ? !value : value && key == get_key(keys, i + 1) && *value == i + 3);
			value = btree_iter_start_at(&iter, &btree, get_key(keys, i), &key, BTREE_ITER_LOWER_BOUND_EXCLUSIVE);
			CHECK(i == N - 1 ? !value : value && key == get_key(keys, i + 1) && *value == i + 3);
			btree_insert(&btree, get_key(keys, i), i + 2);
		}

		for (size_t i = 0; i < N; i++) {
			value = btree_iter_start_at(&iter, &btree, get_key(keys, i), &key, BTREE_ITER_UPPER_BOUND_INCLUSIVE);
			CHECK(value && key == get_key(keys, i) && *value == i + 2);
			value = btree_iter_start_at(&iter, &btree, get_key(keys, i), &key, BTREE_ITER_UPPER_BOUND_EXCLUSIVE);
			CHECK(i == 0 ? !value : value && key == get_key(keys, i - 1) && *value == i + 1);
			bool deleted = btree_delete(&btree, get_key(keys, i), NULL, NULL);
			CHECK(deleted);
			CHECK(!btree_iter_start_at(&iter, &btree, get_key(keys, i), NULL, BTREE_ITER_FIND_KEY));
			value = btree_iter_start_at(&iter, &btree, get_key(keys, i), &key, BTREE_ITER_UPPER_BOUND_INCLUSIVE);
			CHECK(i == 0 ? !value : value && key == get_key(keys, i - 1) && *value == i + 1);
			value = btree_iter_start_at(&iter, &btree, get_key(keys, i), &key, BTREE_ITER_UPPER_BOUND_EXCLUSIVE);
			CHECK(i == 0 ? !value : value && key == get_key(keys, i - 1) && *value == i + 1);
			btree_insert(&btree, get_key(keys, i), i + 2);
		}
#else
		for (size_t i = 0; i < N; i++) {
			key = btree_iter_start_at(&iter, &btree, get_key(keys, i), BTREE_ITER_LOWER_BOUND_INCLUSIVE);
			CHECK(*key == get_key(keys, i));
			key = btree_iter_start_at(&iter, &btree, get_key(keys, i), BTREE_ITER_LOWER_BOUND_EXCLUSIVE);
			CHECK(i == N - 1 ? !key : *key == get_key(keys, i + 1));
			bool deleted = btree_delete(&btree, get_key(keys, i), NULL);
			CHECK(deleted);
			CHECK(!btree_iter_start_at(&iter, &btree, get_key(keys, i), BTREE_ITER_FIND_KEY));
			key = btree_iter_start_at(&iter, &btree, get_key(keys, i), BTREE_ITER_LOWER_BOUND_INCLUSIVE);
			CHECK(i == N - 1 ? !key : *key == get_key(keys, i + 1));
			key = btree_iter_start_at(&iter, &btree, get_key(keys, i), BTREE_ITER_LOWER_BOUND_EXCLUSIVE);
			CHECK(i == N - 1 ? !key : *key == get_key(keys, i + 1));
			btree_insert(&btree, get_key(keys, i));
		}

		for (size_t i = 0; i < N; i++) {
			key = btree_iter_start_at(&iter, &btree, get_key(keys, i), BTREE_ITER_UPPER_BOUND_INCLUSIVE);
			CHECK(*key == get_key(keys, i));
			key = btree_iter_start_at(&iter, &btree, get_key(keys, i), BTREE_ITER_UPPER_BOUND_EXCLUSIVE);
			CHECK(i == 0 ? !key : *key == get_key(keys, i - 1));
			bool deleted = btree_delete(&btree, get_key(keys, i), NULL);
			CHECK(deleted);
			CHECK(!btree_iter_start_at(&iter, &btree, get_key(keys, i), BTREE_ITER_FIND_KEY));
			key = btree_iter_start_at(&iter, &btree, get_key(keys, i), BTREE_ITER_UPPER_BOUND_INCLUSIVE);
			CHECK(i == 0 ? !key : *key == get_key(keys, i - 1));
			key = btree_iter_start_at(&iter, &btree, get_key(keys, i), BTREE_ITER_UPPER_BOUND_EXCLUSIVE);
			CHECK(i == 0 ? !key : *key == get_key(keys, i - 1));
			btree_insert(&btree, get_key(keys, i));
		}
#endif

		btree_check(&btree, &btree_info);
	}

	for (size_t i = 0; i < N; i++) {
		size_t x = random_next_u64(&rng) % LIMIT;
		btree_key_t key = get_key(keys, x);
#ifdef STRING_MAP
		btree_insert_sequential(&btree, key, x);
#else
		btree_insert_sequential(&btree, key);
#endif
		CHECK(btree_check(&btree, &btree_info));
	}

	btree_destroy(&btree);

	for (size_t i = 0; i < N; i++) {
		size_t x = random_next_u64(&rng) % LIMIT;
		btree_key_t key = get_key(keys, x);
		bool exists = btable_lookup(&btable, key, get_hash(key));
		if (!exists) {
			*btable_insert(&btable, key, get_hash(key)) = key;
		}
#ifdef STRING_MAP
		bool inserted = btree_insert(&btree, key, x);
#else
		bool inserted = btree_insert(&btree, key);
#endif
		CHECK(exists ? !inserted : inserted);
		CHECK(btree_check(&btree, &btree_info));
		CHECK(btree_find(&btree, key));
	}

	btree_destroy(&btree);
	CHECK(btree._impl.height == 0);
	CHECK(btree_check(&btree, &btree_info));

	btable_destroy(&btable);

	for (size_t i = 0; i < N; i++) {
		btree_key_t key = get_key(keys, i);
#ifdef STRING_MAP
		bool inserted = btree_insert(&btree, key, i);
#else
		bool inserted = btree_insert(&btree, key);
#endif
		CHECK(inserted);
	}

	CHECK(btree_check(&btree, &btree_info));

	for (size_t i = 0; i < N; i++) {
		btree_key_t key = get_key(keys, i);
#ifdef STRING_MAP
		btree_key_t min;
		btree_value_t *value = btree_get_leftmost(&btree, &min);
		CHECK(value && strtoumax(min, NULL, 10) == *value);
		CHECK(btree_info.cmp(&min, &key) == 0);
		btree_value_t val;
		bool removed = btree_delete_min(&btree, &min, &val);
		CHECK(removed && strtoumax(min, NULL, 10) == val);
		CHECK(btree_info.cmp(&min, &key) == 0);
#else
		const btree_key_t *min = btree_get_leftmost(&btree);
		CHECK(min);
		CHECK(btree_info.cmp(min, &key) == 0);
		btree_key_t min_val;
		bool removed = btree_delete_min(&btree, &min_val);
		CHECK(removed);
		CHECK(btree_info.cmp(&min_val, &key) == 0);
#endif
	}

	for (size_t i = 0; i < N; i++) {
		btree_key_t key = get_key(keys, i);
#ifdef STRING_MAP
		bool inserted = btree_insert(&btree, key, i);
#else
		bool inserted = btree_insert(&btree, key);
#endif
		CHECK(inserted);
	}

	CHECK(btree_check(&btree, &btree_info));

	for (size_t i = 0; i < N; i++) {
		btree_key_t key = get_key(keys, N - 1 - i);
#ifdef STRING_MAP
		btree_key_t max;
		btree_value_t *value = btree_get_rightmost(&btree, &max);
		CHECK(value && strtoumax(max, NULL, 10) == *value);
		CHECK(btree_info.cmp(&max, &key) == 0);
		btree_value_t val;
		bool removed = btree_delete_max(&btree, &max, &val);
		CHECK(removed && strtoumax(max, NULL, 10) == val);
		CHECK(btree_info.cmp(&max, &key) == 0);
#else
		const btree_key_t *max = btree_get_rightmost(&btree);
		CHECK(max);
		CHECK(btree_info.cmp(max, &key) == 0);
		btree_key_t max_val;
		bool removed = btree_delete_max(&btree, &max_val);
		CHECK(removed);
		CHECK(btree_info.cmp(&max_val, &key) == 0);
#endif
	}

	CHECK(btree._impl.height == 0);

	destroy_keys(keys, num_keys);

	return true;
}
