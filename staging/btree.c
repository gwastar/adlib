#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "compiler.h"
#include "hash.h"
#include "hashtable.h"
#include "random.h"

// TODO figure out the API
// TODO investigate the performance difference between gcc and clang
// TODO implement APIs with hints for optimized bulk operations
//      (figure out why the initial implementation did not improve performance...)
// TODO use the same tree search implementation for find, delete, and insert?
// TODO change height/depth semantics? (height == 0 means root is null OR leaf)

#define STRING_KEYS

#define MAX_ITEMS 127
#define MIN_ITEMS (MAX_ITEMS / 2)
#define MAX_CHILDREN (MAX_ITEMS + 1)

_Static_assert(MAX_ITEMS >= 2, "use an AVL or RB tree for 1 item per node");

#define LINEAR_SEARCH_THRESHOLD 0

#ifdef STRING_KEYS
typedef char *btree_key_t;
#else
typedef int64_t btree_key_t;
#endif

static int compare(const btree_key_t a, const btree_key_t b)
{
#ifdef STRING_KEYS
	return strcmp(a, b);
#else
	return (a < b) ? -1 : (a > b);
#endif
}

DEFINE_HASHTABLE(btable, btree_key_t, btree_key_t, 8, *key == *entry)

struct btree_node {
	// unsigned int _padding[3]; // why does this make find 10% faster with gcc?? (and insertion slightly slower)
	unsigned int num_keys;
	btree_key_t keys[MAX_ITEMS];
	struct btree_node *children[];
};

struct btree {
	struct btree_node *root;
	unsigned int height; // 0 means root is NULL, 1 means root is leaf
};

struct btree_iter {
	const struct btree *tree;
	unsigned int depth;
	struct btree_pos {
		struct btree_node *node;
		unsigned int idx;
	} path[32];
};

static struct btree_iter btree_iter_start(const struct btree *tree)
{
	struct btree_iter iter;
	iter.tree = tree;
	iter.depth = 0;

	if (tree->height == 0) {
		return iter;
	}

	struct btree_node *node = tree->root;
	iter.path[0].idx = 0;
	iter.path[0].node = node;

	for (iter.depth = 1; iter.depth < tree->height; iter.depth++) {
		node = node->children[0];
		iter.path[iter.depth].idx = 0;
		iter.path[iter.depth].node = node;
	}
	return iter;
}

static bool btree_iter_get_next(struct btree_iter *iter, btree_key_t *key)
{
	if (iter->depth == 0) {
		return false;
	}
	struct btree_pos *pos = &iter->path[iter->depth - 1];
	*key = pos->node->keys[pos->idx];
	pos->idx++;
	// descend into the leftmost child of the right child
	while (iter->depth < iter->tree->height) {
		struct btree_node *child = pos->node->children[pos->idx];
		pos = &iter->path[iter->depth++];
		pos->node = child;
		pos->idx = 0;
	}
	while (pos->idx >= pos->node->num_keys) {
		if (--iter->depth == 0) {
			return false;
		}
		pos = &iter->path[iter->depth - 1];
	}
	return true;
}

static struct btree_node *btree_new_node(bool leaf)
{
	struct btree_node *node = malloc(sizeof(*node) + (leaf ? 0 : MAX_CHILDREN * sizeof(node->children[0])));
	node->num_keys = 0;
	return node;
}

#define BTREE_EMPTY ((struct btree){.root = NULL, .height = 0})

static void btree_init(struct btree *tree)
{
	*tree = BTREE_EMPTY;
}

static void btree_destroy(struct btree *tree)
{
	if (tree->height == 0) {
		return;
	}
	struct btree_pos path[32];
	struct btree_node *node = tree->root;
	path[0].idx = 0;
	path[0].node = node;
	unsigned int depth;
	for (depth = 1; depth < tree->height; depth++) {
		node = node->children[0];
		path[depth].idx = 0;
		path[depth].node = node;
	}

	for (;;) {
		// start at leaf
		struct btree_pos *pos = &path[depth - 1];
		// free the leaf and go up, if we are at the end of the current node free it and keep going up
		do {
			free(pos->node);
			if (--depth == 0) {
				tree->root = NULL;
				tree->height = 0;
				return;
			}
			pos = &path[depth - 1];
		} while (pos->idx >= pos->node->num_keys);
		// descend into the leftmost child of the right child
		pos->idx++;
		do {
			struct btree_node *child = pos->node->children[pos->idx];
			pos = &path[depth++];
			pos->node = child;
			pos->idx = 0;
		} while (depth < tree->height);
	}
}

static bool btree_node_search(const struct btree_node *node, const btree_key_t key, unsigned int *ret_idx)
{
	unsigned int start = 0;
	unsigned int end = node->num_keys;
	compiler_assume(end <= MAX_ITEMS);
	bool found = false;
	unsigned int idx;
	while (start + LINEAR_SEARCH_THRESHOLD < end) {
		unsigned int mid = (start + end) / 2; // start + end should never overflow
		int cmp = compare(key, node->keys[mid]);
		if (cmp == 0) {
			idx = mid;
			found = true;
			goto done;
		} else if (cmp > 0) {
			start = mid + 1;
		} else {
			end = mid;
		}
	}

	if (LINEAR_SEARCH_THRESHOLD == 0) {
		idx = start;
		goto done;
	}

	while (start < end) {
		int cmp = compare(key, node->keys[start]);
		if (cmp == 0) {
			idx = start;
			found = true;
			goto done;
		}
		if (cmp < 0) {
			break;
		}
		start++;
		end--;
		cmp = compare(key, node->keys[end]);
		if (cmp == 0) {
			idx = end;
			found = true;
			goto done;
		}
		if (cmp > 0) {
			start = end + 1;
			break;
		}
	}
	idx = start;
done:
	*ret_idx = idx;
	return found;
}

static bool btree_find(const struct btree *tree, btree_key_t key)
{
	if (tree->height == 0) {
		return false;
	}
	const struct btree_node *node = tree->root;
	unsigned int depth = 1;
	for (;;) {
		unsigned int idx;
		if (btree_node_search(node, key, &idx)) {
			return true;
		}
		if (depth++ == tree->height) {
			break;
		}
		node = node->children[idx];
	}
	return false;
}

static bool btree_get_minmax_internal(const struct btree *tree, btree_key_t *key, bool min)
{
	if (tree->height == 0) {
		return false;
	}
	const struct btree_node *node = tree->root;
	unsigned int depth = 0;
	for (;;) {
		if (++depth == tree->height) {
			break;
		}
		unsigned int idx = min ? 0 : node->num_keys;
		node = node->children[idx];
	}
	unsigned int idx = min ? 0 : node->num_keys - 1;
	*key = node->keys[idx];
	return true;
}

static bool btree_get_min(const struct btree *tree, btree_key_t *key)
{
	return btree_get_minmax_internal(tree, key, true);
}

static bool btree_get_max(const struct btree *tree, btree_key_t *key)
{
	return btree_get_minmax_internal(tree, key, false);
}

static void btree_node_shift_keys_right(struct btree_node *node, unsigned int idx)
{
	memmove(node->keys + idx + 1, node->keys + idx, (node->num_keys - idx) * sizeof(node->keys[0]));
}

static void btree_node_shift_children_right(struct btree_node *node, unsigned int idx)
{
	memmove(node->children + idx + 1,
		node->children + idx,
		(node->num_keys + 1 - idx) * sizeof(node->children[0]));
}

static void btree_node_shift_keys_left(struct btree_node *node, unsigned int idx)
{
	memmove(node->keys + idx, node->keys + idx + 1, (node->num_keys - idx - 1) * sizeof(node->keys[0]));
}

static void btree_node_shift_children_left(struct btree_node *node, unsigned int idx)
{
	memmove(node->children + idx,
		node->children + idx + 1,
		(node->num_keys - idx) * sizeof(node->children[0]));
}

enum btree_deletion_mode {
	DELETE_MIN,
	DELETE_MAX,
	DELETE_KEY,
};

static bool btree_delete_internal(struct btree *tree, enum btree_deletion_mode mode, btree_key_t key,
				  btree_key_t *ret_key)
{
	if (tree->height == 0) {
		return false;
	}
	struct btree_node *node = tree->root;
	unsigned int depth = 1;
	struct btree_pos path[32];
	unsigned int idx;
	bool leaf = false;
	for (;;) {
		leaf = depth == tree->height;
		bool found = false;
		switch (mode) {
		case DELETE_KEY:
			if (btree_node_search(node, key, &idx)) {
				found = true;
			} else if (leaf) {
				// at leaf and not found
				return false;
			}
			break;
		case DELETE_MIN:
			idx = 0;
			break;
		case DELETE_MAX:
			idx = leaf ? node->num_keys - 1 : node->num_keys;
			break;
		}
		if (leaf) {
			break;
		}
		if (found) {
			// we found the key in an internal node
			// now return it and then replace it with the max value of the left subtree
			*ret_key = node->keys[idx];
			ret_key = &node->keys[idx];
			mode = DELETE_MAX;
		}
		path[depth - 1].idx = idx;
		path[depth - 1].node = node;
		depth++;
		node = node->children[idx];
	}

	// we are at the leaf and have found the item to delete
	// return the item and rebalance
	*ret_key = node->keys[idx];
	btree_node_shift_keys_left(node, idx);
	assert(node->num_keys != 0);
	node->num_keys--;

	while (--depth > 0) {
		if (node->num_keys >= MIN_ITEMS) {
			return true;
		}

		node = path[depth - 1].node;
		idx = path[depth - 1].idx;

		// eagerly merging with the left child slightly improves performance for delete-only benchmarks
		if (idx == node->num_keys ||
		    (idx != 0 && node->children[idx - 1]->num_keys + node->children[idx]->num_keys < MAX_ITEMS)) {
			idx--;
		}
		struct btree_node *left = node->children[idx];
		struct btree_node *right = node->children[idx + 1];

		if (left->num_keys + right->num_keys < MAX_ITEMS) {
			left->keys[left->num_keys] = node->keys[idx];
			btree_node_shift_keys_left(node, idx);
			btree_node_shift_children_left(node, idx + 1);
			node->num_keys--;
			left->num_keys++;
			memcpy(left->keys + left->num_keys, right->keys, right->num_keys * sizeof(right->keys[0]));
			if (!leaf) {
				memcpy(left->children + left->num_keys,
				       right->children,
				       (right->num_keys + 1) * sizeof(right->children[0]));
			}
			left->num_keys += right->num_keys;
			free(right);
		} else if (left->num_keys > right->num_keys) {
			btree_node_shift_keys_right(right, 0);
			right->keys[0] = node->keys[idx];
			node->keys[idx] = left->keys[left->num_keys - 1];
			if (!leaf) {
				btree_node_shift_children_right(right, 0);
				right->children[0] = left->children[left->num_keys];
			}
			left->num_keys--;
			right->num_keys++;
		} else {
			left->keys[left->num_keys] = node->keys[idx];
			node->keys[idx] = right->keys[0];
			btree_node_shift_keys_left(right, 0);
			if (!leaf) {
				left->children[left->num_keys + 1] = right->children[0];
				btree_node_shift_children_left(right, 0);
			}
			right->num_keys--;
			left->num_keys++;
		}

		leaf = false;
	}

	// assert(node == tree->root);

	if (node->num_keys == 0) {
		tree->root = NULL;
		if (tree->height > 1) {
			tree->root = node->children[0];
		}
		tree->height--;
		free(node);
	}

	return true;
}

static bool btree_delete(struct btree *tree, btree_key_t key, btree_key_t *ret_key)
{
	return btree_delete_internal(tree, DELETE_KEY, key, ret_key);
}

static bool btree_delete_min(struct btree *tree, btree_key_t *ret_key)
{
	btree_key_t dummy = 0;
	return btree_delete_internal(tree, DELETE_MIN, dummy, ret_key);
}

static bool btree_delete_max(struct btree *tree, btree_key_t *ret_key)
{
	btree_key_t dummy = 0;
	return btree_delete_internal(tree, DELETE_MAX, dummy, ret_key);
}

#if MAX_ITEMS % 2 == 0

static struct btree_node *btree_node_split_and_insert(struct btree_node *node, unsigned int idx, btree_key_t key,
						      struct btree_node *right, btree_key_t *median)
{
	assert(node->num_keys == MAX_ITEMS);
	struct btree_node *new_node = btree_new_node(!right);
	node->num_keys = MIN_ITEMS;
	if (idx < MIN_ITEMS) {
		*median = node->keys[MIN_ITEMS - 1];
		memcpy(new_node->keys, node->keys + MIN_ITEMS, MIN_ITEMS * sizeof(node->keys[0]));
		btree_node_shift_keys_right(node, idx);
		node->keys[idx] = key;
		if (right) {
			memcpy(new_node->children,
			       node->children + MIN_ITEMS,
			       (MIN_ITEMS + 1) * sizeof(node->children[0]));
			btree_node_shift_children_right(node, idx + 1);
			node->children[idx + 1] = right;
		}
	} else if (idx == MIN_ITEMS) {
		*median = key; // TODO could avoid this copy... but is it even worth it?
		memcpy(new_node->keys, node->keys + MIN_ITEMS, MIN_ITEMS * sizeof(node->keys[0]));
		if (right) {
			memcpy(new_node->children + 1,
			       node->children + MIN_ITEMS + 1,
			       MIN_ITEMS * sizeof(node->children[0]));
			new_node->children[0] = right;
		}
	} else {
		idx -= MIN_ITEMS + 1;
		*median = node->keys[MIN_ITEMS];
		new_node->num_keys = MIN_ITEMS - 1;
		// it is not worth splitting the memcpys here in two to avoid the shifts
		memcpy(new_node->keys, node->keys + MIN_ITEMS + 1, (MIN_ITEMS - 1) * sizeof(node->keys[0]));
		btree_node_shift_keys_right(new_node, idx);
		new_node->keys[idx] = key;
		if (right) {
			memcpy(new_node->children,
			       node->children + MIN_ITEMS + 1,
			       MIN_ITEMS * sizeof(node->children[0]));
			btree_node_shift_children_right(new_node, idx + 1);
			new_node->children[idx + 1] = right;
		}
	}
	new_node->num_keys = MIN_ITEMS;

	return new_node;
}

static bool _btree_insert_and_rebalance(struct btree *tree, btree_key_t key, unsigned int idx,
					struct btree_node *node, struct btree_pos path[static 32],
					unsigned int depth, unsigned int last_nonfull_node_depth)
{
	(void)last_nonfull_node_depth;
	struct btree_node *right = NULL;
	for (;;) {
		if (node->num_keys < MAX_ITEMS) {
			btree_node_shift_keys_right(node, idx);
			node->keys[idx] = key;
			if (right) {
				btree_node_shift_children_right(node, idx + 1);
				node->children[idx + 1] = right;
			}
			node->num_keys++;
			return true;
		}

		right = btree_node_split_and_insert(node, idx, key, right, &key);

		if (--depth == 0) {
			break;
		}
		idx = path[depth - 1].idx;
		node = path[depth - 1].node;
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

#else

static struct btree_node *btree_node_split(struct btree_node *node, btree_key_t *median, bool leaf)
{
	assert(node->num_keys == MAX_ITEMS);
	struct btree_node *new_node = btree_new_node(leaf);
	node->num_keys = MIN_ITEMS;
	*median = node->keys[MIN_ITEMS];
	memcpy(new_node->keys, node->keys + MIN_ITEMS + 1, (MIN_ITEMS) * sizeof(node->keys[0]));
	if (!leaf) {
		memcpy(new_node->children,
		       node->children + MIN_ITEMS + 1,
		       (MIN_ITEMS + 1) * sizeof(node->children[0]));
	}
	new_node->num_keys = MIN_ITEMS;
	return new_node;
}

static bool _btree_insert_and_rebalance(struct btree *tree, btree_key_t key, unsigned int idx,
					struct btree_node *node, struct btree_pos path[static 32],
					unsigned int depth, unsigned int last_nonfull_node_depth)
{
	if (last_nonfull_node_depth != depth) {
		unsigned int d = last_nonfull_node_depth;
		if (d == 0) {
			struct btree_node *new_root = btree_new_node(false);
			new_root->children[0] = tree->root;
			tree->root = new_root;
			tree->height++;
			idx = 0;
			node = new_root;
		} else {
			node = path[d - 1].node;
			idx = path[d - 1].idx;
		}

		do {
			d++;
			btree_key_t median;
			struct btree_node *right = btree_node_split(node->children[idx], &median, d == depth);
			btree_node_shift_keys_right(node, idx);
			node->keys[idx] = median;
			btree_node_shift_children_right(node, idx + 1);
			node->children[idx + 1] = right;
			node->num_keys++;
			if (compare(key, median) < 0) {
				node = node->children[idx];
				idx = path[d - 1].idx;
			} else {
				node = node->children[idx + 1];
				idx = path[d - 1].idx - MIN_ITEMS - 1;
			}
		} while (d != depth);
	}

	btree_node_shift_keys_right(node, idx);
	node->keys[idx] = key;
	node->num_keys++;
	return true;
}

#endif

static bool btree_insert(struct btree *tree, btree_key_t key)
{
	if (tree->height == 0) {
		tree->root = btree_new_node(true);
		tree->height = 1;
	}
	struct btree_node *node = tree->root;
	struct btree_pos path[32];
	unsigned int depth = 1;
	unsigned int idx;
	unsigned int last_nonfull_node_depth = 0;
	for (;;) {
		if (btree_node_search(node, key, &idx)) {
			return false;
		}
		if (node->num_keys < MAX_ITEMS) {
			last_nonfull_node_depth = depth;
		}
		path[depth - 1].idx = idx;
		path[depth - 1].node = node;
		if (depth == tree->height) {
			break;
		}
		depth++;
		node = node->children[idx];
	}
	return _btree_insert_and_rebalance(tree, key, idx, node, path, depth, last_nonfull_node_depth);
}

static bool btree_insert_sequential(struct btree *tree, btree_key_t key)
{
	if (tree->height == 0) {
		return btree_insert(tree, key);
	}
	struct btree_node *node = tree->root;
	struct btree_pos path[32];
	unsigned int depth = 1;
	unsigned int idx;
	unsigned int last_nonfull_node_depth = 0;
	for (;;) {
		idx = node->num_keys;
		if (unlikely(compare(key, node->keys[idx - 1]) <= 0)) {
			return btree_insert(tree, key);
		}
		if (node->num_keys < MAX_ITEMS) {
			last_nonfull_node_depth = depth;
		}
		path[depth - 1].idx = idx;
		path[depth - 1].node = node;
		if (depth == tree->height) {
			break;
		}
		depth++;
		node = node->children[idx];
	}
	return _btree_insert_and_rebalance(tree, key, idx, node, path, depth, last_nonfull_node_depth);
}

#define CHECK(cond)							\
	do {								\
		if (!(cond)) {						\
			fputs("\"" #cond "\" failed\n", stderr);	\
			return false;					\
		}							\
	} while (0)

static bool btree_check_node(const struct btree_node *node, unsigned int height);

static bool btree_check_children(const struct btree_node *node, unsigned int height)
{
	CHECK(height != 0);
	if (height == 1) {
		return true;
	}
	for (unsigned int i = 0; i < node->num_keys + 1; i++) {
		if (!btree_check_node(node->children[i], height - 1)) {
			return false;
		}
	}
	return true;
}

static bool btree_check_node_sorted(const struct btree_node *node)
{
	for (unsigned int i = 1; i < node->num_keys; i++) {
		CHECK(compare(node->keys[i - 1], node->keys[i]) < 0);
	}
	return true;
}

static bool btree_check_node(const struct btree_node *node, unsigned int height)
{
	CHECK(node->num_keys >= MIN_ITEMS && node->num_keys <= MAX_ITEMS);
	return btree_check_node_sorted(node) && btree_check_children(node, height);
}

static bool btree_check(const struct btree *btree)
{
	struct btree_node *root = btree->root;
	if (btree->height == 0) {
		CHECK(!root);
		return true;
	}
	CHECK(root->num_keys >= 1 && root->num_keys <= MAX_ITEMS);
	return btree_check_children(root, btree->height);
}

#ifdef STRING_KEYS
static char **keys;
static size_t num_keys;
#define __KEY_SIZE 15
#define KEY_SIZE (__KEY_SIZE + 1)
#define KEY_FORMAT __KEY_FORMAT(__KEY_SIZE)
#define __KEY_FORMAT(S) ___KEY_FORMAT(S)
#define ___KEY_FORMAT(S) "%0" #S "zu"

static void init_keys(size_t N)
{
	num_keys = N;
	keys = malloc(N * sizeof(keys[0]));
	for (size_t i = 0; i < N; i++) {
		keys[i] = malloc(KEY_SIZE);
		snprintf(keys[i], KEY_SIZE, KEY_FORMAT, i);
	}
}

static void destroy_keys(void)
{
	for (size_t i = 0; i < num_keys; i++) {
		free(keys[i]);
	}
	free(keys);
	num_keys = 0;
	keys = NULL;
}

static _attr_pure btable_hash_t get_hash(const char *key)
{
	return murmurhash3_x64_64(key, KEY_SIZE, 0).u64;
}

static char *get_key(size_t x)
{
	assert(x < num_keys);
	return keys[x];
}

static char **random_keys;
static size_t num_random_keys;

static void init_random_keys(uint32_t *random_numbers, size_t N)
{
	num_random_keys = N;
	random_keys = malloc(N * sizeof(random_keys[0]));
	for (size_t i = 0; i < N; i++) {
		random_keys[i] = malloc(KEY_SIZE);
		snprintf(random_keys[i], KEY_SIZE, KEY_FORMAT, (size_t)random_numbers[i]);
	}
}

static void destroy_random_keys(void)
{
	for (size_t i = 0; i < num_random_keys; i++) {
		free(random_keys[i]);
	}
	free(random_keys);
	num_random_keys = 0;
	random_keys = NULL;
}

static char *get_random_key(size_t i)
{
	assert(i < num_random_keys);
	return random_keys[i];
}
#else
static uint32_t *random_keys;
static void init_keys(size_t N) {}
static void destroy_keys(void) {}
static btable_hash_t get_hash(btree_key_t key)
{
	return (btable_hash_t)key;
}
static btree_key_t get_key(size_t x)
{
	return (btree_key_t)x;
}
static void init_random_keys(uint32_t *random_numbers, size_t N)
{
	random_keys = random_numbers;
}
static void destroy_random_keys(void)
{
	random_keys = NULL;
}
static btree_key_t get_random_key(size_t i)
{
	return (btree_key_t)random_keys[i];
}
#endif

static void test(void)
{
	// srand(1234);

	const size_t N = 1 << 14;
	const size_t LIMIT = 1 << 16;

	init_keys(2 * (N > LIMIT ? N : LIMIT));

	struct btree btree;
	btree_init(&btree);

	struct btable btable;
	btable_init(&btable, N);

	for (size_t i = 0; i < N; i++) {
		btree_key_t key = get_key(rand() % LIMIT);
		bool exists = btable_lookup(&btable, key, get_hash(key));
		if (!exists) {
			*btable_insert(&btable, key, get_hash(key)) = key;
		}
		bool inserted = btree_insert(&btree, key);
		assert(exists ? !inserted : inserted);
		assert(btree_check(&btree));
		assert(btree_find(&btree, key));
	}
	for (btable_iter_t iter = btable_iter_start(&btable);
	     !btable_iter_finished(&iter);
	     btable_iter_advance(&iter)) {
		assert(btree_find(&btree, *iter.entry));
	}
	{
		btree_key_t prev_key, key;
		struct btree_iter iter = btree_iter_start(&btree);
		btree_iter_get_next(&iter, &key);
		assert(btable_lookup(&btable, key, get_hash(key)));
		prev_key = key;
		while (btree_iter_get_next(&iter, &key)) {
			assert(btable_lookup(&btable, key, get_hash(key)));
			assert(compare(prev_key, key) < 0);
			prev_key = key;
		}
	}
	for (size_t i = LIMIT; i < LIMIT + 1000; i++) {
		assert(!btree_find(&btree, get_key(i)));
	}
	for (size_t i = 0; i < N; i++) {
		btree_key_t key = get_key(rand() % LIMIT);
		bool exists = btable_remove(&btable, key, get_hash(key), NULL);
		btree_key_t k;
		bool removed = btree_delete(&btree, key, &k);
		assert(exists == removed);
		assert(!removed || (k == key));
		assert(btree_check(&btree));
		assert(!btree_find(&btree, key));
	}

	for (btable_iter_t iter = btable_iter_start(&btable);
	     !btable_iter_finished(&iter);
	     btable_iter_advance(&iter)) {
		btree_key_t key;
		assert(btree_delete(&btree, *iter.entry, &key));
		assert(key == *iter.entry);
		assert(btree_check(&btree));
	}
	btable_clear(&btable);

	assert(!btree.root && btree.height == 0);

	for (size_t i = 0; i < N; i++) {
		btree_insert_sequential(&btree, get_key(i));
		assert(btree_check(&btree));
	}

	for (size_t i = 0; i < N; i++) {
		btree_key_t key = get_key(rand() % LIMIT);
		btree_insert_sequential(&btree, key);
		assert(btree_check(&btree));
	}

	btree_destroy(&btree);

	for (size_t i = 0; i < N; i++) {
		btree_key_t key = get_key(rand() % LIMIT);
		bool exists = btable_lookup(&btable, key, get_hash(key));
		if (!exists) {
			*btable_insert(&btable, key, get_hash(key)) = key;
		}
		bool inserted = btree_insert(&btree, key);
		assert(exists ? !inserted : inserted);
		assert(btree_check(&btree));
		assert(btree_find(&btree, key));
	}

	btree_destroy(&btree);
	assert(btree.height == 0);
	assert(btree_check(&btree));

	btable_destroy(&btable);

	for (size_t i = 0; i < N; i++) {
		btree_key_t key = get_key(i);
		bool inserted = btree_insert(&btree, key);
		assert(inserted);
	}

	assert(btree_check(&btree));

	for (size_t i = 0; i < N; i++) {
		btree_key_t min;
		bool found = btree_get_min(&btree, &min);
		assert(found);
		assert(compare(min, get_key(i)) == 0);
		bool removed = btree_delete_min(&btree, &min);
		assert(removed);
		assert(compare(min, get_key(i)) == 0);
	}

	for (size_t i = 0; i < N; i++) {
		btree_key_t key = get_key(i);
		bool inserted = btree_insert(&btree, key);
		assert(inserted);
	}

	assert(btree_check(&btree));

	for (size_t i = 0; i < N; i++) {
		btree_key_t max;
		bool found = btree_get_max(&btree, &max);
		assert(found);
		assert(compare(max, get_key(N - 1 - i)) == 0);
		bool removed = btree_delete_max(&btree, &max);
		assert(removed);
		assert(compare(max, get_key(N - 1 - i)) == 0);
	}

	assert(btree.height == 0);

	destroy_keys();
}

static double ns_elapsed(struct timespec start, struct timespec end)
{
	double s = end.tv_sec - start.tv_sec;
	double ns = end.tv_nsec - start.tv_nsec;
	return ns + 1000000000 * s;
}

static int compare_doubles(const void *_a, const void *_b)
{
	double a = *(const double *)_a;
	double b = *(const double *)_b;
	return a < b ? -1 : (a == b ? 0 : 1);
}

static double get_median(double *values, size_t n)
{
	qsort(values, n, sizeof(values[0]), compare_doubles);
	double x = 0.5 * (n - 1);
	size_t idx0 = (size_t)x;
	size_t idx1 = idx0 + (n != 1);
	double fract = x - idx0;
	return (1 - fract) * values[idx0] + fract * values[idx1];
}

static struct btree_node *btree_node_copy(const struct btree_node *node, unsigned int depth)
{
	struct btree_node *copy = btree_new_node(depth == 0);
	memcpy(copy->keys, node->keys, node->num_keys * sizeof(node->keys[0]));
	copy->num_keys = node->num_keys;
	if (depth == 0) {
		return copy;
	}
	for (unsigned int i = 0; i < node->num_keys + 1; i++) {
		copy->children[i] = btree_node_copy(node->children[i], depth - 1);
	}
	return copy;
}

static struct btree btree_copy(struct btree *btree)
{
	struct btree copy;
	copy.height = btree->height;
	copy.root = btree->height == 0 ? NULL : btree_node_copy(btree->root, btree->height - 1);
	return copy;
}

static size_t next_pow2(size_t x)
{
	x--;
	x |= x >> 1;
	x |= x >> 2;
	x |= x >> 4;
	x |= x >> 8;
	x |= x >> 16;
	x |= x >> (sizeof(x) * 4);
	x++;
	return x;
}

static void benchmark(void)
{
#ifdef STRING_KEYS
	const size_t N = 1 << 16;
#else
	const size_t N = 1 << 18;
#endif

#define ITERATIONS 15
	double inorder_fast_insert[ITERATIONS];
	double inorder_insert[ITERATIONS];
	double revorder_insert[ITERATIONS];
	double random_insert[ITERATIONS];
	double inorder_find[ITERATIONS];
	double revorder_find[ITERATIONS];
	double randorder_find[ITERATIONS];
	double random_find[ITERATIONS];
	double iteration[ITERATIONS];
	double inorder_deletion[ITERATIONS];
	double revorder_deletion[ITERATIONS];
	double randorder_deletion[ITERATIONS];
	double destruction[ITERATIONS];
	double inorder_mixed[ITERATIONS];
	double revorder_mixed[ITERATIONS];
	double random_mixed[ITERATIONS];

	uint32_t *random_numbers = malloc(2 * N * sizeof(random_numbers[0]));
	uint32_t *random_numbers2 = random_numbers + N;
	{
		struct random_state rng;
		random_state_init(&rng, 0xdeadbeef);
		for (size_t i = 0; i < 2 * N; i++) {
			random_numbers[i] = random_next_u32(&rng);
		}
	}

	init_keys(2 * N);
	init_random_keys(random_numbers, 2 * N);

	for (size_t k = 0; k < ITERATIONS; k++) {
		struct btree btree;
		btree_init(&btree);

		struct timespec start_tp, end_tp;

		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp);
		for (size_t i = 0; i < N; i++) {
			btree_key_t key = get_key(i);
			btree_insert(&btree, key);
		}
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);
		inorder_insert[k] = ns_elapsed(start_tp, end_tp);

		assert(btree_check(&btree));
		btree_destroy(&btree);

		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp);
		for (size_t i = 0; i < N; i++) {
			btree_key_t key = get_key(i);
			btree_insert_sequential(&btree, key);
		}
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);
		inorder_fast_insert[k] = ns_elapsed(start_tp, end_tp);

		assert(btree_check(&btree));
		btree_destroy(&btree);

		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp);
		for (size_t i = 0; i < N; i++) {
			btree_key_t key = get_key(N - 1 - i);
			btree_insert(&btree, key);
		}
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);
		revorder_insert[k] = ns_elapsed(start_tp, end_tp);

		assert(btree_check(&btree));
		btree_destroy(&btree);

		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp);
		for (size_t i = 0; i < N; i++) {
			btree_key_t key = get_random_key(i);
			btree_insert(&btree, key);
		}
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);
		random_insert[k] = ns_elapsed(start_tp, end_tp);

		assert(btree_check(&btree));

		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp);
		for (size_t i = 0; i < N; i++) {
			btree_key_t key = get_random_key(i);
			bool found = btree_find(&btree, key);
			asm volatile("" :: "g" (found) : "memory");
		}
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);
		inorder_find[k] = ns_elapsed(start_tp, end_tp);

		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp);
		for (size_t i = 0; i < N; i++) {
			btree_key_t key = get_random_key(N - 1 - i);
			bool found = btree_find(&btree, key);
			asm volatile("" :: "g" (found) : "memory");
		}
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);
		revorder_find[k] = ns_elapsed(start_tp, end_tp);

		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp);
		for (size_t i = 0; i < N; i++) {
			btree_key_t key = get_random_key(random_numbers2[i] % N);
			bool found = btree_find(&btree, key);
			asm volatile("" :: "g" (found) : "memory");
		}
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);
		randorder_find[k] = ns_elapsed(start_tp, end_tp);

		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp);
		for (size_t i = 0; i < N; i++) {
			btree_key_t key = get_random_key(N + i);
			bool found = btree_find(&btree, key);
			asm volatile("" :: "g" (found) : "memory");
		}
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);
		random_find[k] = ns_elapsed(start_tp, end_tp);

		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp);
		{
			btree_key_t prev_key, key;
			struct btree_iter iter = btree_iter_start(&btree);
			btree_iter_get_next(&iter, &prev_key);
			while (btree_iter_get_next(&iter, &key)) {
				assert(compare(prev_key, key) < 0);
				prev_key = key;
			}
		}
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);
		iteration[k] = ns_elapsed(start_tp, end_tp);

		struct btree copy = btree_copy(&btree);

		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp);
		for (size_t i = 0; i < N; i++) {
			btree_key_t key = get_random_key(i);
			bool found = btree_delete(&btree, key, &key);
			asm volatile("" :: "g" (found) : "memory");
		}
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);
		inorder_deletion[k] = ns_elapsed(start_tp, end_tp);

		btree = copy;
		copy = btree_copy(&btree);

		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp);
		for (size_t i = 0; i < N; i++) {
			btree_key_t key = get_random_key(N - 1 - i);
			bool found = btree_delete(&btree, key, &key);
			asm volatile("" :: "g" (found) : "memory");
		}
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);
		revorder_deletion[k] = ns_elapsed(start_tp, end_tp);

		btree = copy;
		copy = btree_copy(&btree);

		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp);
		for (size_t i = 0; i < N; i++) {
			btree_key_t key = get_random_key(random_numbers2[i] % N);
			bool found = btree_delete(&btree, key, &key);
			asm volatile("" :: "g" (found) : "memory");
		}
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);
		randorder_deletion[k] = ns_elapsed(start_tp, end_tp);

		btree_destroy(&btree);
		btree = copy;

		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp);
		btree_destroy(&btree);
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);
		destruction[k] = ns_elapsed(start_tp, end_tp);

		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp);
		for (size_t i = 0; i < N / 4; i++) {
			const size_t INSERTIONS = 5;
			const size_t FINDS = 10;
			const size_t DELETIONS = 2;
			for (size_t j = 0; j < INSERTIONS; j++) {
				btree_key_t key = get_key((i * INSERTIONS + j) % (2 * N));
				btree_insert(&btree, key);
			}
			size_t n = next_pow2((i + 1) * INSERTIONS);
			if (n > 2 * N) {
				n = 2 * N;
			}
			for (size_t j = 0; j < FINDS; j++) {
				btree_key_t key = get_key((i * FINDS + j) & (n - 1));
				bool found = btree_find(&btree, key);
				asm volatile("" :: "g" (found) : "memory");
			}
			for (size_t j = 0; j < DELETIONS; j++) {
				btree_key_t key = get_key((i * DELETIONS + 2 * j) & (n - 1));
				btree_delete(&btree, key, &key);
			}
		}
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);
		inorder_mixed[k] = ns_elapsed(start_tp, end_tp);

		btree_destroy(&btree);

		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp);
		for (size_t i = 0; i < N / 4; i++) {
			const size_t INSERTIONS = 5;
			const size_t FINDS = 10;
			const size_t DELETIONS = 2;
			for (size_t j = 0; j < INSERTIONS; j++) {
				btree_key_t key = get_key(2 * N - 1 - ((i * INSERTIONS + j) % (2 * N)));
				btree_insert(&btree, key);
			}
			size_t n = next_pow2((i + 1) * INSERTIONS);
			if (n > 2 * N) {
				n = 2 * N;
			}
			for (size_t j = 0; j < FINDS; j++) {
				btree_key_t key = get_key(2 * N - 1 - ((i * FINDS + j) & (n - 1)));
				bool found = btree_find(&btree, key);
				asm volatile("" :: "g" (found) : "memory");
			}
			for (size_t j = 0; j < DELETIONS; j++) {
				btree_key_t key = get_key(2 * N - 1 - ((i * DELETIONS + 2 * j) & (n - 1)));
				btree_delete(&btree, key, &key);
			}
		}
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);
		revorder_mixed[k] = ns_elapsed(start_tp, end_tp);

		btree_destroy(&btree);

		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp);
		for (size_t i = 0; i < N / 4; i++) {
			const size_t INSERTIONS = 5;
			const size_t FINDS = 10;
			const size_t DELETIONS = 2;
			for (size_t j = 0; j < INSERTIONS; j++) {
				btree_key_t key = get_random_key((i * INSERTIONS + j) % (2 * N));
				btree_insert(&btree, key);
			}
			size_t n = next_pow2((i + 1) * INSERTIONS);
			if (n > 2 * N) {
				n = 2 * N;
			}
			for (size_t j = 0; j < FINDS; j++) {
				btree_key_t key = get_random_key((i * FINDS + j) & (n - 1));
				bool found = btree_find(&btree, key);
				asm volatile("" :: "g" (found) : "memory");
			}
			for (size_t j = 0; j < DELETIONS; j++) {
				btree_key_t key = get_random_key((i * DELETIONS + 2 * j) & (n - 1));
				btree_delete(&btree, key, &key);
			}
		}
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);
		random_mixed[k] = ns_elapsed(start_tp, end_tp);

		btree_destroy(&btree);
	}
	// TODO print more statistics
	double t;
	t = get_median(inorder_insert, ITERATIONS);
	printf("%-32s %8.1f ns\n", "in-order insertion", t / N);
	t = get_median(inorder_fast_insert, ITERATIONS);
	printf("%-32s %8.1f ns\n", "in-order insertion (fastpath)", t / N);
	t = get_median(revorder_insert, ITERATIONS);
	printf("%-32s %8.1f ns\n", "reverse-order insertion", t / N);
	t = get_median(random_insert, ITERATIONS);
	printf("%-32s %8.1f ns\n", "random insertion", t / N);
	t = get_median(inorder_find, ITERATIONS);
	printf("%-32s %8.1f ns\n", "in-order find", t / N);
	t = get_median(revorder_find, ITERATIONS);
	printf("%-32s %8.1f ns\n", "reverse-order find", t / N);
	t = get_median(randorder_find, ITERATIONS);
	printf("%-32s %8.1f ns\n", "random-order find", t / N);
	t = get_median(random_find, ITERATIONS);
	printf("%-32s %8.1f ns\n", "random find", t / N);
	t = get_median(iteration, ITERATIONS);
	printf("%-32s %8.1f ns\n", "iteration", t / N);
	t = get_median(inorder_deletion, ITERATIONS);
	printf("%-32s %8.1f ns\n", "in-order deletion", t / N);
	t = get_median(revorder_deletion, ITERATIONS);
	printf("%-32s %8.1f ns\n", "reverse-order deletion", t / N);
	t = get_median(randorder_deletion, ITERATIONS);
	printf("%-32s %8.1f ns\n", "random-order deletion", t / N);
	t = get_median(destruction, ITERATIONS);
	printf("%-32s %8.1f ns\n", "destruction", t / N);
	t = get_median(inorder_mixed, ITERATIONS);
	printf("%-32s %8.1f ns\n", "in-order mixed", t / (N / 4));
	t = get_median(revorder_mixed, ITERATIONS);
	printf("%-32s %8.1f ns\n", "reverse-order mixed", t / (N / 4));
	t = get_median(random_mixed, ITERATIONS);
	printf("%-32s %8.1f ns\n", "random-order mixed", t / (N / 4));

	destroy_keys();
	destroy_random_keys();
	free(random_numbers);
}

int main(int argc, char **argv)
{
	// test();
	benchmark();
}
