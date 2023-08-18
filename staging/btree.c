#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hashtable.h"

#define BTREE_K 7
#define BTREE_2K (2 * BTREE_K)

typedef long btree_key_t;

DEFINE_HASHTABLE(btable, btree_key_t, btree_key_t, 8, *key == *entry)

struct btree_node {
	// TODO store the parent pointer and index in parent and benchmark
	unsigned int num_keys;
	btree_key_t keys[BTREE_2K];
	struct btree_node *children[BTREE_2K + 1]; // TODO most nodes are leafs and don't habe children
};

struct btree {
	struct btree_node *root;
	unsigned int height; // 0 means root is leaf (or NULL)
};

struct btree_iter {
	struct btree *tree;
	unsigned int cur_height;
	struct btree_pos {
		struct btree_node *node;
		unsigned int idx;
	} path[32];
};

static struct btree_iter btree_iter_start(struct btree *tree)
{
	struct btree_iter iter;
	iter.tree = tree;
	iter.cur_height = 0;

	struct btree_node *node = tree->root;
	for (;;) {
		iter.path[iter.cur_height].idx = 0;
		iter.path[iter.cur_height].node = node;
		if (iter.cur_height == tree->height) {
			break;
		}
		iter.cur_height++;
		node = node->children[0];
	}
	return iter;
}

static bool btree_iter_get_next(struct btree_iter *iter, btree_key_t *key)
{
	struct btree_pos *pos = &iter->path[iter->cur_height];
	if (!pos->node) {
		return false;
	}
	*key = pos->node->keys[pos->idx];
	pos->idx++;
	// descend into the leftmost child of the right child
	while (iter->cur_height < iter->tree->height) {
		struct btree_node *child = pos->node->children[pos->idx];
		pos = &iter->path[++iter->cur_height];
		pos->node = child;
		pos->idx = 0;
	}
	while (pos->idx >= pos->node->num_keys) {
		if (iter->cur_height == 0) {
			pos->node = NULL;
			return false;
		}
		pos = &iter->path[--iter->cur_height];
	}
	return true;
}

static int compare(const btree_key_t *key1, const btree_key_t *key2)
{
	return *key1 - *key2;
}

static struct btree_node *btree_new_node(bool leaf)
{
	struct btree_node *node = calloc(1, sizeof(*node));
	return node;
}

static void btree_init(struct btree *btree)
{
	btree->root = NULL;
	btree->height = 0;
}

static bool btree_bsearch(struct btree_node *node, const btree_key_t *key, size_t *ret_idx)
{
	// TODO optimize
	size_t start = 0;
	size_t end = node->num_keys;
	while (end > start) {
		size_t idx = start + (end - start) / 2;
		int cmp = compare(key, &node->keys[idx]);
		if (cmp < 0) {
			end = idx;
		} else if (cmp > 0) {
			start = idx + 1;
		} else {
			*ret_idx = idx;
			return true;
		}
	}
	*ret_idx = start;
	return false;
}

static bool btree_find(struct btree *tree, btree_key_t key)
{
	struct btree_node *node = tree->root;
	if (!node) {
		return false;
	}
	unsigned int cur_height = 0;
	for (;;) {
		size_t idx;
		if (btree_bsearch(node, &key, &idx)) {
			return true;
		}
		if (cur_height++ == tree->height) {
			break;
		}
		node = node->children[idx];
	}
	return false;
}

static void btree_swap(btree_key_t *key1, btree_key_t *key2)
{
	btree_key_t tmp;
	memcpy(&tmp, key1, sizeof(*key1));
	memcpy(key1, key2, sizeof(*key1));
	memcpy(key2, &tmp, sizeof(*key1));
}

static void btree_node_shift_keys_right(struct btree_node *node, size_t idx)
{
	assert(node->num_keys < BTREE_2K);
	assert(idx <= node->num_keys);
	memmove(node->keys + idx + 1, node->keys + idx, (node->num_keys - idx) * sizeof(node->keys[0]));
}

static void btree_node_shift_children_right(struct btree_node *node, size_t idx)
{
	assert(node->num_keys <= BTREE_2K);
	assert(idx <= node->num_keys + 1);
	memmove(node->children + idx + 1,
		node->children + idx,
		(node->num_keys + 1 - idx) * sizeof(node->children[0]));
}

static struct btree_node *btree_node_split_and_insert(struct btree_node *node, size_t idx, btree_key_t key,
						      struct btree_node *right, btree_key_t *median)
{
	assert(node->num_keys == BTREE_2K);
	struct btree_node *new_node = btree_new_node(!right);
	node->num_keys = BTREE_K;
	new_node->num_keys = BTREE_K;
	if (idx < BTREE_K) {
		*median = node->keys[BTREE_K - 1];
		memcpy(new_node->keys, node->keys + BTREE_K, BTREE_K * sizeof(node->keys[0]));
		btree_node_shift_keys_right(node, idx);
		node->keys[idx] = key;
		if (right) {
			memcpy(new_node->children,
			       node->children + BTREE_K,
			       (BTREE_K + 1) * sizeof(node->children[0]));
			btree_node_shift_children_right(node, idx + 1);
			node->children[idx + 1] = right;
		}
	} else if (idx == BTREE_K) {
		*median = key; // TODO could avoid this copy... but is it even worth it?
		memcpy(new_node->keys, node->keys + BTREE_K, BTREE_K * sizeof(node->keys[0]));
		if (right) {
			memcpy(new_node->children + 1,
			       node->children + BTREE_K + 1,
			       BTREE_K * sizeof(node->children[0]));
			new_node->children[0] = right;
		}
	} else {
		idx -= BTREE_K + 1;
		*median = node->keys[BTREE_K];
		// TODO could avoid these shifts by splitting the memcpys in two
		memcpy(new_node->keys, node->keys + BTREE_K + 1, (BTREE_K - 1) * sizeof(node->keys[0]));
		// TODO new_node->num_keys is wrong here since the new key was not inserted yet
		btree_node_shift_keys_right(new_node, idx);
		new_node->keys[idx] = key;
		if (right) {
			memcpy(new_node->children,
			       node->children + BTREE_K + 1,
			       BTREE_K * sizeof(node->children[0]));
			btree_node_shift_children_right(new_node, idx + 1);
			new_node->children[idx + 1] = right;
		}
	}

	return new_node;
}

static bool btree_insert(struct btree *tree, btree_key_t key)
{
	if (!tree->root) {
		tree->root = btree_new_node(true);
	}
	struct btree_node *node = tree->root;
	size_t cur_height = 0;
	size_t idx;
	struct {
		struct btree_node *node;
		unsigned int idx;
	} path[32];
	for (;;) {
		if (btree_bsearch(node, &key, &idx)) {
			return false;
		}
		if (cur_height == tree->height) {
			break;
		}
		path[cur_height].idx = idx;
		path[cur_height].node = node;
		cur_height++;
		node = node->children[idx];
	}
	struct btree_node *right = NULL;
	for (;;) {
		if (node->num_keys < BTREE_2K) {
			btree_node_shift_keys_right(node, idx);
			node->keys[idx] = key;
			node->num_keys++;
			if (right) {
				btree_node_shift_children_right(node, idx + 1);
				node->children[idx + 1] = right;
			}
			return true;
		}

		right = btree_node_split_and_insert(node, idx, key, right, &key);

		if (cur_height-- == 0) {
			break;
		}
		idx = path[cur_height].idx;
		node = path[cur_height].node;
	}
	struct btree_node *new_root = btree_new_node(false);
	new_root->keys[0] = key;
	new_root->num_keys = 1;
	new_root->children[0] = node;
	new_root->children[1] = right;
	tree->root = new_root;
	tree->height++;
	return true;
}

#define CHECK(cond)							\
	do {								\
		if (!(cond)) {						\
			fputs("\"" #cond " \"failed\n", stderr);	\
			return false;					\
		}							\
	} while (0)

static bool btree_check_node(struct btree_node *node, unsigned int height);

static bool btree_check_children(struct btree_node *node, unsigned int height)
{
	if (height == 0) {
		return true;
	}
	for (size_t i = 0; i < node->num_keys + 1; i++) {
		if (!btree_check_node(node->children[i], height - 1)) {
			return false;
		}
	}
	return true;
}

static bool btree_check_node_sorted(struct btree_node *node)
{
	for (size_t i = 1; i < node->num_keys; i++) {
		CHECK(compare(&node->keys[i - 1], &node->keys[i]) < 0);
	}
	return true;
}

static bool btree_check_node(struct btree_node *node, unsigned int height)
{
	CHECK(node->num_keys >= BTREE_K && node->num_keys <= BTREE_2K);
	return btree_check_node_sorted(node) && btree_check_children(node, height);
}

static bool btree_check(struct btree *btree)
{
	struct btree_node *root = btree->root;
	if (btree->height == 0 && !root) {
		return true;
	}
	CHECK(root->num_keys >= 1 && root->num_keys <= BTREE_2K);
	return btree_check_children(root, btree->height);
}

int main(int argc, char **argv)
{
	srand(1234);

	const size_t N = 100000;
	const size_t LIMIT = 1 << 20;

	struct btree btree;
	btree_init(&btree);

	struct btable btable;
	btable_init(&btable, N);

	for (size_t i = 0; i < N; i++) {
		btree_key_t key = rand() % LIMIT;
		bool exists = btable_lookup(&btable, key, (btable_hash_t)key);
		if (!exists) {
			*btable_insert(&btable, key, (btable_hash_t)key) = key;
		}
		bool inserted = btree_insert(&btree, key);
		assert(exists ? !inserted : inserted);
		assert(btree_find(&btree, key));
		assert(btree_check(&btree));
	}
	for (btable_iter_t iter = btable_iter_start(&btable);
	     !btable_iter_finished(&iter);
	     btable_iter_advance(&iter)) {
		assert(btree_find(&btree, *iter.entry));
	}
	btree_key_t key;
	for (struct btree_iter iter = btree_iter_start(&btree); btree_iter_get_next(&iter, &key);) {
		assert(btable_lookup(&btable, key, (btable_hash_t)key));
	}
	for (size_t i = LIMIT; i < LIMIT + 1000; i++) {
		assert(!btree_find(&btree, i));
	}

	btable_destroy(&btable);
}
