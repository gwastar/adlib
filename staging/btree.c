#include <alloca.h>
#include <assert.h>
#include <inttypes.h>
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

// TODO cleanup api
// TODO get_lower/upper_bound, iter_start_lower/upper_bound, iter_start_at
// TODO rename min->leftmost, max->rightmost
// TODO assert that start + end in btree_node_search cannot overflow (make max_items an unsigned short?)
// TODO investigate the performance difference between gcc and clang
// TODO implement APIs with hints for optimized bulk operations
//      (figure out why the initial implementation did not improve performance...)

// #define STRING_MAP

#define LINEAR_SEARCH_THRESHOLD 0 // TODO put this in info and make it dependant on key type with _Generic

struct btree_node {
	unsigned int num_items;
	unsigned char data[]; // TODO alignment
	/* item_t items[MAX_ITEMS]; */
	/* struct btree_node *children[leaf ? 0 : MAX_ITEMS + 1]; */
};

struct _btree {
	struct btree_node *root;
	unsigned int height; // 0 means root is NULL, 1 means root is leaf
};

struct btree_iter {
	const struct _btree *tree;
	unsigned int depth;
	struct btree_pos {
		struct btree_node *node;
		unsigned int idx;
	} path[32];
};

struct btree_info {
	unsigned int max_items;
	unsigned int min_items;
	size_t item_size;
	// TODO alignment offset
	// TODO linear search threshold
	int (*cmp)(const void *a, const void *b);
	void (*destroy_item)(void *item);
};

static void *btree_node_item(struct btree_node *node, unsigned int idx, const struct btree_info *info)
{
	return node->data + idx * info->item_size;
}

static struct btree_node **btree_node_children(struct btree_node *node, const struct btree_info *info)
{
	return (struct btree_node **)btree_node_item(node, info->max_items, info);
}

static struct btree_node *btree_node_get_child(struct btree_node *node, unsigned int idx,
					       const struct btree_info *info)
{
	return btree_node_children(node, info)[idx];
}

static void btree_node_set_child(struct btree_node *node, unsigned int idx, struct btree_node *child,
				 const struct btree_info *info)
{
	btree_node_children(node, info)[idx] = child;
}

static void btree_node_copy_child(struct btree_node *dest_node, unsigned int dest_idx,
				  struct btree_node *src_node, unsigned int src_idx,
				  const struct btree_info *info)
{
	btree_node_set_child(dest_node, dest_idx, btree_node_get_child(src_node, src_idx, info), info);
}

static void btree_node_get_item(void *item, struct btree_node *node, unsigned int idx,
				const struct btree_info *info)
{
	memcpy(item, btree_node_item(node, idx, info), info->item_size);
}

static void btree_node_set_item(struct btree_node *node, unsigned int idx, const void *item,
				const struct btree_info *info)
{
	memcpy(btree_node_item(node, idx, info), item, info->item_size);
}

static void btree_node_copy_item(struct btree_node *dest_node, unsigned int dest_idx,
				 struct btree_node *src_node, unsigned int src_idx,
				 const struct btree_info *info)
{
	btree_node_set_item(dest_node, dest_idx, btree_node_item(src_node, src_idx, info),info);
}

// TODO void _btree_iter_start(struct btree_iter *iter, ...)?
static struct btree_iter _btree_iter_start(const struct _btree *tree, const struct btree_info *info)
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
		node = btree_node_get_child(node, 0, info);
		iter.path[iter.depth].idx = 0;
		iter.path[iter.depth].node = node;
	}
	return iter;
}

static bool _btree_iter_get_next(struct btree_iter *iter, void *item, const struct btree_info *info)
{
	if (iter->depth == 0) {
		return false;
	}
	struct btree_pos *pos = &iter->path[iter->depth - 1];
	btree_node_get_item(item, pos->node, pos->idx, info);
	pos->idx++;
	// descend into the leftmost child of the right child
	while (iter->depth < iter->tree->height) {
		struct btree_node *child = btree_node_get_child(pos->node, pos->idx, info);
		pos = &iter->path[iter->depth++];
		pos->node = child;
		pos->idx = 0;
	}
	while (pos->idx >= pos->node->num_items) {
		if (--iter->depth == 0) {
			break;
		}
		pos = &iter->path[iter->depth - 1];
	}
	return true;
}

static struct btree_node *btree_new_node(bool leaf, const struct btree_info *info)
{
	size_t items_size = info->max_items * info->item_size;
	size_t children_size = leaf ? 0 : (info->max_items + 1) * sizeof(struct btree_node *);
	struct btree_node *node = malloc(sizeof(*node) + items_size + children_size);
	node->num_items = 0;
	return node;
}

#define BTREE_EMPTY {.root = NULL, .height = 0}

static void _btree_init(struct _btree *tree)
{
	*tree = (struct _btree)BTREE_EMPTY;
}

static void _btree_destroy(struct _btree *tree, const struct btree_info *info)
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
		node = btree_node_get_child(node, 0, info);
		path[depth].idx = 0;
		path[depth].node = node;
	}

	for (;;) {
		// start at leaf
		struct btree_pos *pos = &path[depth - 1];
		// free the leaf and go up, if we are at the end of the current node free it and keep going up
		do {
			if (info->destroy_item) {
				for (unsigned int i = 0; i < pos->node->num_items; i++) {
					info->destroy_item(btree_node_item(pos->node, i, info));
				}
			}
			free(pos->node);
			if (--depth == 0) {
				tree->root = NULL;
				tree->height = 0;
				return;
			}
			pos = &path[depth - 1];
		} while (pos->idx >= pos->node->num_items);
		// descend into the leftmost child of the right child
		pos->idx++;
		do {
			struct btree_node *child = btree_node_get_child(pos->node, pos->idx, info);
			pos = &path[depth++];
			pos->node = child;
			pos->idx = 0;
		} while (depth < tree->height);
	}
}

static bool btree_node_search(struct btree_node *node, const void *key, unsigned int *ret_idx,
			      const struct btree_info *info)
{
	unsigned int start = 0;
	unsigned int end = node->num_items;
	compiler_assume(end <= info->max_items);
	bool found = false;
	unsigned int idx;
	while (start + LINEAR_SEARCH_THRESHOLD < end) {
		unsigned int mid = (start + end) / 2; // start + end should never overflow
		int cmp = info->cmp(key, btree_node_item(node, mid, info));
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
		int cmp = info->cmp(key, btree_node_item(node, start, info));
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
		cmp = info->cmp(key, btree_node_item(node, end, info));
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

static void *_btree_find(const struct _btree *tree, const void *key, const struct btree_info *info)
{
	if (tree->height == 0) {
		return NULL;
	}
	struct btree_node *node = tree->root;
	unsigned int depth = 1;
	for (;;) {
		unsigned int idx;
		if (btree_node_search(node, key, &idx, info)) {
			return btree_node_item(node, idx, info);
		}
		if (depth++ == tree->height) {
			break;
		}
		node = btree_node_get_child(node, idx, info);
	}
	return NULL;
}

static void *btree_get_minmax_internal(const struct _btree *tree, bool min, const struct btree_info *info)
{
	if (tree->height == 0) {
		return NULL;
	}
	struct btree_node *node = tree->root;
	unsigned int depth = 0;
	for (;;) {
		if (++depth == tree->height) {
			break;
		}
		unsigned int idx = min ? 0 : node->num_items;
		node = btree_node_get_child(node, idx, info);
	}
	unsigned int idx = min ? 0 : node->num_items - 1;
	return btree_node_item(node, idx, info);
}

static void btree_node_shift_items_right(struct btree_node *node, unsigned int idx,
					 const struct btree_info *info)
{
	memmove(btree_node_item(node, idx + 1, info),
		btree_node_item(node, idx, info),
		(node->num_items - idx) * info->item_size);
}

static void btree_node_shift_children_right(struct btree_node *node, unsigned int idx,
					    const struct btree_info *info)
{
	memmove(btree_node_children(node, info) + idx + 1,
		btree_node_children(node, info) + idx,
		(node->num_items + 1 - idx) * sizeof(struct btree_node *));
}

static void btree_node_shift_items_left(struct btree_node *node, unsigned int idx,
					const struct btree_info *info)
{
	memmove(btree_node_item(node, idx, info),
		btree_node_item(node, idx + 1, info),
		(node->num_items - idx - 1) * info->item_size);
}

static void btree_node_shift_children_left(struct btree_node *node, unsigned int idx,
					   const struct btree_info *info)
{
	memmove(btree_node_children(node, info) + idx,
		btree_node_children(node, info) + idx + 1,
		(node->num_items - idx) * sizeof(struct btree_node *));
}

enum btree_deletion_mode {
	DELETE_MIN,
	DELETE_MAX,
	DELETE_KEY,
};

static bool btree_delete_internal(struct _btree *tree, enum btree_deletion_mode mode, const void *key,
				  void *ret_item, const struct btree_info *info)
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
			if (btree_node_search(node, key, &idx, info)) {
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
			idx = leaf ? node->num_items - 1 : node->num_items;
			break;
		}
		if (leaf) {
			break;
		}
		if (found) {
			// we found the key in an internal node
			// now return it and then replace it with the max value of the left subtree
			if (ret_item) {
				btree_node_get_item(ret_item, node, idx, info);
			}
			ret_item = btree_node_item(node, idx, info);
			mode = DELETE_MAX;
		}
		path[depth - 1].idx = idx;
		path[depth - 1].node = node;
		depth++;
		node = btree_node_get_child(node, idx, info);
	}

	// we are at the leaf and have found the item to delete
	// return the item and rebalance
	if (ret_item) {
		btree_node_get_item(ret_item, node, idx, info);
	}
	btree_node_shift_items_left(node, idx, info);
	assert(node->num_items != 0);
	node->num_items--;

	while (--depth > 0) {
		if (node->num_items >= info->min_items) {
			return true;
		}

		node = path[depth - 1].node;
		idx = path[depth - 1].idx;

		// eagerly merging with the left child slightly improves performance for delete-only benchmarks
		if (idx == node->num_items ||
		    (idx != 0 &&
		     btree_node_get_child(node, idx - 1, info)->num_items +
		     btree_node_get_child(node, idx, info)->num_items < info->max_items)) {
			idx--;
		}
		struct btree_node *left = btree_node_get_child(node, idx, info);
		struct btree_node *right = btree_node_get_child(node, idx + 1, info);

		if (left->num_items + right->num_items < info->max_items) {
			btree_node_copy_item(left, left->num_items, node, idx, info);
			btree_node_shift_items_left(node, idx, info);
			btree_node_shift_children_left(node, idx + 1, info);
			node->num_items--;
			left->num_items++;
			memcpy(btree_node_item(left, left->num_items, info),
			       btree_node_item(right, 0, info),
			       right->num_items * info->item_size);
			if (!leaf) {
				memcpy(btree_node_children(left, info) + left->num_items,
				       btree_node_children(right, info),
				       (right->num_items + 1) * sizeof(struct btree_node *));
			}
			left->num_items += right->num_items;
			free(right);
		} else if (left->num_items > right->num_items) {
			btree_node_shift_items_right(right, 0, info);
			btree_node_copy_item(right, 0, node, idx, info);
			btree_node_copy_item(node, idx, left, left->num_items - 1, info);
			if (!leaf) {
				btree_node_shift_children_right(right, 0, info);
				btree_node_copy_child(right, 0, left, left->num_items, info);
			}
			left->num_items--;
			right->num_items++;
		} else {
			btree_node_copy_item(left, left->num_items, node, idx, info);
			btree_node_copy_item(node, idx, right, 0, info);
			btree_node_shift_items_left(right, 0, info);
			if (!leaf) {
				btree_node_copy_child(left, left->num_items + 1, right, 0, info);
				btree_node_shift_children_left(right, 0, info);
			}
			right->num_items--;
			left->num_items++;
		}

		leaf = false;
	}

	// assert(node == tree->root);

	if (node->num_items == 0) {
		tree->root = NULL;
		if (tree->height > 1) {
			tree->root = btree_node_get_child(node, 0, info);
		}
		tree->height--;
		free(node);
	}

	return true;
}

/* item will be inserted and then set to the median */
static struct btree_node *btree_node_split_and_insert(struct btree_node *node, unsigned int idx,
						      void *item, struct btree_node *right,
						      const struct btree_info *info)
{
	assert(node->num_items == info->max_items);
	struct btree_node *new_node = btree_new_node(!right, info);
	node->num_items = info->min_items;
	if (idx < info->min_items) {
		memcpy(btree_node_item(new_node, 0, info),
		       btree_node_item(node, info->min_items, info),
		       info->min_items * info->item_size);
		btree_node_shift_items_right(node, idx, info);
		btree_node_set_item(node, idx, item, info);

		// the median got shifted right by one
		btree_node_get_item(item, node, info->min_items, info); // return median

		if (right) {
			memcpy(btree_node_children(new_node, info),
			       btree_node_children(node, info) + info->min_items,
			       (info->min_items + 1) * sizeof(struct btree_node *));
			btree_node_shift_children_right(node, idx + 1, info);
			btree_node_set_child(node, idx + 1, right, info);
		}
	} else if (idx == info->min_items) {
		// item is median
		memcpy(btree_node_item(new_node, 0, info),
		       btree_node_item(node, info->min_items, info),
		       info->min_items * info->item_size);

		if (right) {
			memcpy(btree_node_children(new_node, info) + 1,
			       btree_node_children(node, info) + info->min_items + 1,
			       info->min_items * sizeof(struct btree_node *));
			btree_node_set_child(new_node, 0, right, info);
		}
	} else {
		idx -= info->min_items + 1;
		new_node->num_items = info->min_items - 1;
		// it is not worth splitting the memcpys here in two to avoid the shifts
		memcpy(btree_node_item(new_node, 0, info),
		       btree_node_item(node, info->min_items + 1, info),
		       (info->min_items - 1) * info->item_size);
		btree_node_shift_items_right(new_node, idx, info);
		btree_node_set_item(new_node, idx, item, info);

		btree_node_get_item(item, node, info->min_items, info); // return median

		if (right) {
			memcpy(btree_node_children(new_node, info),
			       btree_node_children(node, info) + info->min_items + 1,
			       info->min_items * sizeof(struct btree_node *));
			btree_node_shift_children_right(new_node, idx + 1, info);
			btree_node_set_child(new_node, idx + 1, right, info);
		}
	}
	new_node->num_items = info->min_items;

	return new_node;
}

static void _btree_insert_and_rebalance_even(struct _btree *tree, void *item, unsigned int idx,
					     struct btree_node *node, struct btree_pos path[static 32],
					     unsigned int depth, unsigned int last_nonfull_node_depth,
					     const struct btree_info *info)
{
	(void)last_nonfull_node_depth;
	struct btree_node *right = NULL;
	for (;;) {
		if (node->num_items < info->max_items) {
			btree_node_shift_items_right(node, idx, info);
			btree_node_set_item(node, idx, item, info);
			if (right) {
				btree_node_shift_children_right(node, idx + 1, info);
				btree_node_set_child(node, idx + 1, right, info);
			}
			node->num_items++;
			return;
		}

		right = btree_node_split_and_insert(node, idx, item, right, info);

		if (--depth == 0) {
			break;
		}
		idx = path[depth - 1].idx;
		node = path[depth - 1].node;
	}
	struct btree_node *new_root = btree_new_node(false, info);
	btree_node_set_item(new_root, 0, item, info);
	new_root->num_items = 1;
	btree_node_set_child(new_root, 0, node, info);
	btree_node_set_child(new_root, 1, right, info);
	tree->root = new_root;
	tree->height++;
}

static struct btree_node *btree_node_split(struct btree_node *node, void *median, bool leaf,
					   const struct btree_info *info)
{
	assert(node->num_items == info->max_items);
	struct btree_node *new_node = btree_new_node(leaf, info);
	node->num_items = info->min_items;
	btree_node_get_item(median, node, info->min_items, info);
	memcpy(btree_node_item(new_node, 0, info),
	       btree_node_item(node, info->min_items + 1, info),
	       info->min_items * info->item_size);
	if (!leaf) {
		memcpy(btree_node_children(new_node, info),
		       btree_node_children(node, info)+ info->min_items + 1,
		       (info->min_items + 1) * sizeof(struct btree_node *));
	}
	new_node->num_items = info->min_items;
	return new_node;
}

static void _btree_insert_and_rebalance_odd(struct _btree *tree, void *item, unsigned int idx,
					    struct btree_node *node, struct btree_pos path[static 32],
					    unsigned int depth, unsigned int last_nonfull_node_depth,
					    const struct btree_info *info)
{
	if (last_nonfull_node_depth != depth) {
		unsigned int d = last_nonfull_node_depth;
		if (d == 0) {
			struct btree_node *new_root = btree_new_node(false, info);
			btree_node_set_child(new_root, 0, tree->root, info);
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
			void *median = alloca(info->item_size);
			struct btree_node *right = btree_node_split(btree_node_get_child(node, idx, info),
								    median, d == depth, info);
			btree_node_shift_items_right(node, idx, info);
			btree_node_set_item(node, idx, median, info);
			btree_node_shift_children_right(node, idx + 1, info);
			btree_node_set_child(node, idx + 1, right, info);
			node->num_items++;
			if (info->cmp(item, median) < 0) {
				node = btree_node_get_child(node, idx, info);
				idx = path[d - 1].idx;
			} else {
				node = btree_node_get_child(node, idx + 1, info);
				idx = path[d - 1].idx - info->min_items - 1;
			}
		} while (d != depth);
	}

	btree_node_shift_items_right(node, idx, info);
	btree_node_set_item(node, idx, item, info);
	node->num_items++;
}

static void _btree_insert_and_rebalance(struct _btree *tree, void *item, unsigned int idx,
					struct btree_node *node, struct btree_pos path[static 32],
					unsigned int depth, unsigned int last_nonfull_node_depth,
					const struct btree_info *info)
{
	((info->max_items & 1) ? _btree_insert_and_rebalance_odd : _btree_insert_and_rebalance_even)
		(tree, item, idx, node, path, depth, last_nonfull_node_depth, info);
}

static bool _btree_insert(struct _btree *tree, void *item, bool update, const struct btree_info *info)
{
	if (tree->height == 0) {
		tree->root = btree_new_node(true, info);
		tree->height = 1;
	}
	struct btree_node *node = tree->root;
	struct btree_pos path[32];
	unsigned int depth = 1;
	unsigned int idx;
	unsigned int last_nonfull_node_depth = 0;
	for (;;) {
		if (btree_node_search(node, item, &idx, info)) {
			if (update) {
				if (info->destroy_item) {
					info->destroy_item(btree_node_item(node, idx, info));
				}
				btree_node_set_item(node, idx, item, info);
			}
			return false;
		}
		if (node->num_items < info->max_items) {
			last_nonfull_node_depth = depth;
		}
		path[depth - 1].idx = idx;
		path[depth - 1].node = node;
		if (depth == tree->height) {
			break;
		}
		depth++;
		node = btree_node_get_child(node, idx, info);
	}
	_btree_insert_and_rebalance(tree, item, idx, node, path, depth, last_nonfull_node_depth, info);
	return true;
}

static bool _btree_insert_sequential(struct _btree *tree, void *item, const struct btree_info *info)
{
	if (tree->height == 0) {
		return _btree_insert(tree, item, false, info);
	}
	struct btree_node *node = tree->root;
	struct btree_pos path[32];
	unsigned int depth = 1;
	unsigned int idx;
	unsigned int last_nonfull_node_depth = 0;
	for (;;) {
		idx = node->num_items;
		if (unlikely(info->cmp(item, btree_node_item(node, idx - 1, info)) <= 0)) {
			return _btree_insert(tree, item, false, info);
		}
		if (node->num_items < info->max_items) {
			last_nonfull_node_depth = depth;
		}
		path[depth - 1].idx = idx;
		path[depth - 1].node = node;
		if (depth == tree->height) {
			break;
		}
		depth++;
		node = btree_node_get_child(node, idx, info);
	}
	_btree_insert_and_rebalance(tree, item, idx, node, path, depth, last_nonfull_node_depth, info);
	return true;
}

#define DEFINE_BTREE_SET(name, key_type, key_destructor, max_items_per_node, ...) \
	typedef key_type name##_key_t;					\
	typedef void (*name##_key_destructor)(name##_key_t key);	\
									\
	struct name {							\
		struct _btree _impl;					\
	};								\
									\
	static int _##name##_compare(const void *_a, const void *_b)	\
	{								\
		const name##_key_t a = *(const name##_key_t *)_a;	\
		const name##_key_t b = *(const name##_key_t *)_b;	\
		return (__VA_ARGS__);					\
	}								\
									\
	static void _##name##_destroy_item(void *item)			\
	{								\
		name##_key_destructor destructor = (key_destructor);	\
		if (destructor) {					\
			destructor(*(name##_key_t *)item);		\
		}							\
	}								\
									\
	_Static_assert((max_items_per_node) >= 2, "use an AVL or RB tree for 1 item per node");	\
									\
	static _Alignas(64) const struct btree_info name##_info = {	\
		.max_items = (max_items_per_node),			\
		.min_items = (max_items_per_node) / 2,			\
		.item_size = sizeof(name##_key_t),			\
		.cmp = _##name##_compare,				\
		.destroy_item = _##name##_destroy_item,			\
	};								\
									\
	typedef struct btree_iter name##_iter_t;			\
									\
	static struct btree_iter name##_iter_start(const struct name *tree) \
	{								\
		return _btree_iter_start(&tree->_impl, &name##_info);	\
	}								\
									\
	static bool name##_iter_get_next(name##_iter_t *iter, name##_key_t *key) \
	{								\
		return _btree_iter_get_next(iter, key, &name##_info);	\
	}								\
									\
	static void name##_init(struct name *tree)			\
	{								\
		_btree_init(&tree->_impl);				\
	}								\
									\
	static void name##_destroy(struct name *tree)			\
	{								\
		_btree_destroy(&tree->_impl, &name##_info);		\
	}								\
									\
	static const name##_key_t *name##_find(const struct name *tree, name##_key_t key) \
	{								\
		return (const name##_key_t *)_btree_find(&tree->_impl, &key, &name##_info); \
	}								\
									\
	static const name##_key_t *name##_get_min(const struct name *tree) \
	{								\
		return btree_get_minmax_internal(&tree->_impl, true, &name##_info); \
	}								\
									\
	static const name##_key_t *name##_get_max(const struct name *tree) \
	{								\
		return btree_get_minmax_internal(&tree->_impl, false, &name##_info); \
	}								\
									\
	static bool name##_delete(struct name *tree, name##_key_t key, name##_key_t *ret_key) \
	{								\
		return btree_delete_internal(&tree->_impl, DELETE_KEY, &key, (void *)ret_key, &name##_info); \
	}								\
									\
	static bool name##_delete_min(struct name *tree, name##_key_t *ret_key) \
	{								\
		return btree_delete_internal(&tree->_impl, DELETE_MIN, NULL, (void *)ret_key, &name##_info); \
	}								\
									\
	static bool name##_delete_max(struct name *tree, name##_key_t *ret_key) \
	{								\
		return btree_delete_internal(&tree->_impl, DELETE_MAX, NULL, (void *)ret_key, &name##_info); \
	}								\
									\
	static bool name##_insert(struct name *tree, name##_key_t key)	\
	{								\
		return _btree_insert(&tree->_impl, &key, false, &name##_info); \
	}								\
									\
	/* TODO does this even make sense for a btree set? */		\
	/* I guess you can use this to destruct and replace a key... */	\
	static bool name##_set(struct name *tree, name##_key_t key)	\
	{								\
		return _btree_insert(&tree->_impl, &key, true, &name##_info); \
	}								\
									\
	static bool name##_insert_sequential(struct name *tree, name##_key_t key) \
	{								\
		return _btree_insert_sequential(&tree->_impl, &key, &name##_info); \
	}


#define DEFINE_BTREE_MAP(name, key_type, value_type, key_destructor, value_destructor, max_items_per_node, ...) \
	typedef key_type name##_key_t;					\
	typedef value_type name##_value_t;				\
	typedef void (*name##_key_destructor)(name##_key_t key);	\
	typedef void (*name##_value_destructor)(name##_value_t *value);	\
	typedef struct { name##_key_t key; name##_value_t value; } _##name##_item_t; \
									\
	struct name {							\
		struct _btree _impl;					\
	};								\
									\
	static int _##name##_compare(const void *_a, const void *_b)	\
	{								\
		const name##_key_t a = *(const name##_key_t *)_a;	\
		const name##_key_t b = *(const name##_key_t *)_b;	\
		return (__VA_ARGS__);					\
	}								\
									\
	static void _##name##_destroy_item(void *_item)			\
	{								\
		name##_key_destructor destroy_key = (key_destructor);	\
		name##_value_destructor destroy_value = (value_destructor); \
		_##name##_item_t *item = _item;				\
		if (destroy_key) {					\
			destroy_key(item->key);				\
		}							\
		if (destroy_value) {					\
			destroy_value(&item->value);			\
		}							\
	}								\
									\
	_Static_assert((max_items_per_node) >= 2, "use an AVL or RB tree for 1 item per node");	\
									\
	static _Alignas(64) const struct btree_info name##_info = {	\
		.max_items = (max_items_per_node),			\
		.min_items = (max_items_per_node) / 2,			\
		.item_size = sizeof(_##name##_item_t),			\
		.cmp = _##name##_compare,				\
		.destroy_item = _##name##_destroy_item,			\
	};								\
									\
	typedef struct btree_iter name##_iter_t;			\
									\
	static struct btree_iter name##_iter_start(const struct name *tree) \
	{								\
		return _btree_iter_start(&tree->_impl, &name##_info);	\
	}								\
									\
	/* TODO this should return name##_value_t * */			\
	static bool name##_iter_get_next(name##_iter_t *iter, name##_key_t *key, name##_value_t *value) \
	{								\
		_##name##_item_t item;					\
		bool have_next = _btree_iter_get_next(iter, &item, &name##_info); \
		if (have_next) {					\
			if (key) {					\
				*key = item.key;			\
			}						\
			if (value) {					\
				*value = item.value;			\
			}						\
		}							\
		return have_next;					\
	}								\
									\
	static void name##_init(struct name *tree)			\
	{								\
		_btree_init(&tree->_impl);				\
	}								\
									\
	static void name##_destroy(struct name *tree)			\
	{								\
		_btree_destroy(&tree->_impl, &name##_info);		\
	}								\
									\
	static name##_value_t *name##_find(const struct name *tree, name##_key_t key) \
	{								\
		_##name##_item_t *item = _btree_find(&tree->_impl, &key, &name##_info); \
		return item ? &item->value : NULL;			\
	}								\
									\
	static name##_value_t *name##_get_min(const struct name *tree, name##_key_t *ret_key) \
	{								\
		_##name##_item_t *item = btree_get_minmax_internal(&tree->_impl, true, &name##_info); \
		if (!item) {						\
			return NULL;					\
		}							\
		if (ret_key) {						\
			*ret_key = item->key;				\
		}							\
		return &item->value;					\
	}								\
									\
	static name##_value_t *name##_get_max(const struct name *tree, name##_key_t *ret_key) \
	{								\
		_##name##_item_t *item = btree_get_minmax_internal(&tree->_impl, false, &name##_info); \
		if (!item) {						\
			return NULL;					\
		}							\
		if (ret_key) {						\
			*ret_key = item->key;				\
		}							\
		return &item->value;					\
	}								\
									\
	static bool name##_delete(struct name *tree, name##_key_t key, name##_key_t *ret_key, \
				  name##_value_t *ret_value)		\
	{								\
		_##name##_item_t item;					\
		bool found = btree_delete_internal(&tree->_impl, DELETE_KEY, &key, \
						   (ret_key || ret_value) ? &item : NULL, &name##_info); \
		if (found) {						\
			if (ret_key) {					\
				*ret_key = item.key;			\
			}						\
			if (ret_value) {				\
				*ret_value = item.value;		\
			}						\
		}							\
		return found;						\
	}								\
									\
	static bool name##_delete_min(struct name *tree, name##_key_t *ret_key, name##_value_t *ret_value) \
	{								\
		_##name##_item_t item;					\
		bool found = btree_delete_internal(&tree->_impl, DELETE_MIN, NULL, \
						   (ret_key || ret_value) ? &item : NULL, &name##_info); \
		if (found) {						\
			if (ret_key) {					\
				*ret_key = item.key;			\
			}						\
			if (ret_value) {				\
				*ret_value = item.value;		\
			}						\
		}							\
		return found;						\
	}								\
									\
	static bool name##_delete_max(struct name *tree, name##_key_t *ret_key, name##_value_t *ret_value) \
	{								\
		_##name##_item_t item;					\
		bool found = btree_delete_internal(&tree->_impl, DELETE_MAX, NULL, \
						   (ret_key || ret_value) ? &item : NULL, &name##_info); \
		if (found) {						\
			if (ret_key) {					\
				*ret_key = item.key;			\
			}						\
			if (ret_value) {				\
				*ret_value = item.value;		\
			}						\
		}							\
		return found;						\
	}								\
									\
	static bool name##_insert(struct name *tree, name##_key_t key, name##_value_t value) \
	{								\
		return _btree_insert(&tree->_impl, &(_##name##_item_t){.key = key, .value = value}, false, \
				     &name##_info);			\
	}								\
									\
	static bool name##_set(struct name *tree, name##_key_t key, name##_value_t value) \
	{								\
		return _btree_insert(&tree->_impl, &(_##name##_item_t){.key = key, .value = value}, true, \
				     &name##_info);			\
	}								\
									\
	static bool name##_insert_sequential(struct name *tree, name##_key_t key, name##_value_t value) \
	{								\
		return _btree_insert_sequential(&tree->_impl, &(_##name##_item_t){.key = key, .value = value}, \
						&name##_info);		\
	}

#ifdef STRING_MAP
DEFINE_BTREE_MAP(btree, char *, uint32_t, NULL, NULL, 128, strcmp(a, b))
#else
DEFINE_BTREE_SET(btree, int64_t, NULL, 127, (a < b) ? -1 : (a > b))
#endif
DEFINE_HASHTABLE(btable, btree_key_t, btree_key_t, 8, *key == *entry)

#define CHECK(cond)							\
	do {								\
		if (!(cond)) {						\
			fputs("\"" #cond "\" failed\n", stderr);	\
			return false;					\
		}							\
	} while (0)

static bool btree_check_node(struct btree_node *node, unsigned int height, const struct btree_info *info);

static bool btree_check_children(struct btree_node *node, unsigned int height, const struct btree_info *info)
{
	CHECK(height != 0);
	if (height == 1) {
		return true;
	}
	for (unsigned int i = 0; i < node->num_items + 1; i++) {
		if (!btree_check_node(btree_node_get_child(node, i, info), height - 1, info)) {
			return false;
		}
	}
	return true;
}

static bool btree_check_node_sorted(struct btree_node *node, const struct btree_info *info)
{
	for (unsigned int i = 1; i < node->num_items; i++) {
		CHECK(info->cmp(btree_node_item(node, i - 1, info), btree_node_item(node, i, info)) < 0);
	}
	return true;
}

static bool btree_check_node(struct btree_node *node, unsigned int height, const struct btree_info *info)
{
	CHECK(node->num_items >= info->min_items && node->num_items <= info->max_items);
	return btree_check_node_sorted(node, info) && btree_check_children(node, height, info);
}

static bool btree_check(const struct btree *btree, const struct btree_info *info)
{
	struct btree_node *root = btree->_impl.root;
	if (btree->_impl.height == 0) {
		CHECK(!root);
		return true;
	}
	CHECK(root->num_items >= 1 && root->num_items <= info->max_items);
	return btree_check_children(root, btree->_impl.height, info);
}

#ifdef STRING_MAP
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
static void init_keys(size_t N)
{
	(void)N;
}
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
	(void)N;
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
		size_t x = rand() % LIMIT;
		btree_key_t key = get_key(x);
		bool exists = btable_lookup(&btable, key, get_hash(key));
		if (!exists) {
			*btable_insert(&btable, key, get_hash(key)) = key;
		}
#ifdef STRING_MAP
		bool inserted = btree_insert(&btree, key, x);
#else
		bool inserted = btree_insert(&btree, key);
#endif
		assert(exists ? !inserted : inserted);
		assert(btree_check(&btree, &btree_info));
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
#ifdef STRING_MAP
		btree_value_t value;
		btree_iter_get_next(&iter, &key, &value);
		assert(strtoumax(key, NULL, 10) == value);
#else
		btree_iter_get_next(&iter, &key);
#endif
		assert(btable_lookup(&btable, key, get_hash(key)));
		prev_key = key;
#ifdef STRING_MAP
		while (btree_iter_get_next(&iter, &key, &value)) {
			assert(strtoumax(key, NULL, 10) == value);

#else
			while (btree_iter_get_next(&iter, &key)) {
#endif
				assert(btable_lookup(&btable, key, get_hash(key)));
				assert(btree_info.cmp(&prev_key, &key) < 0);
				prev_key = key;
			}
		}
		for (size_t i = LIMIT; i < LIMIT + 1000; i++) {
			assert(!btree_find(&btree, get_key(i)));
		}
		for (size_t i = 0; i < N; i++) {
			btree_key_t key = get_key(rand() % LIMIT);
			bool exists = btable_remove(&btable, key, get_hash(key), NULL);
#ifdef STRING_MAP
			btree_key_t k;
			btree_value_t v;
			bool removed = btree_delete(&btree, key, &k, &v);
			assert(!removed || strtoumax(k, NULL, 10) == v);
#else
			btree_key_t k;
			bool removed = btree_delete(&btree, key, &k);
#endif
			assert(exists == removed);
			assert(!removed || (k == key));
			assert(btree_check(&btree, &btree_info));
			assert(!btree_find(&btree, key));
		}

		for (btable_iter_t iter = btable_iter_start(&btable);
		     !btable_iter_finished(&iter);
		     btable_iter_advance(&iter)) {
#ifdef STRING_MAP
			btree_key_t key;
			btree_value_t value;
			bool removed = btree_delete(&btree, *iter.entry, &key, &value);
			assert(removed && strtoumax(key, NULL, 10) == value);
#else
			btree_key_t key;
			bool removed = btree_delete(&btree, *iter.entry, &key);
			assert(removed);
#endif
			assert(key == *iter.entry);
			assert(btree_check(&btree, &btree_info));
		}
		btable_clear(&btable);

		assert(btree._impl.height == 0); // TODO expose height?
		assert(btree_check(&btree, &btree_info));

		for (size_t i = 0; i < N; i++) {
			btree_key_t key = get_key(i);
#ifdef STRING_MAP
			btree_insert_sequential(&btree, key, i);
#else
			btree_insert_sequential(&btree, key);
#endif
			assert(btree_check(&btree, &btree_info));
			assert(btree_find(&btree, key));
		}

		for (size_t i = 0; i < N; i++) {
			btree_key_t key = get_key(i);
#ifdef STRING_MAP
			bool inserted = btree_set(&btree, key, i + 1);
#else
			bool inserted = btree_set(&btree, key);
#endif
			assert(!inserted);
			assert(btree_check(&btree, &btree_info));
			assert(btree_find(&btree, key));
		}

		{
			struct btree_iter iter = btree_iter_start(&btree);
			for (size_t i = 0; i < N; i++) {
				btree_key_t key;
#ifdef STRING_MAP
				btree_value_t value;
				bool have_next = btree_iter_get_next(&iter, &key, &value);
				assert(have_next);
				assert(value == i + 1);
#else
				bool have_next = btree_iter_get_next(&iter, &key);
				assert(have_next);
#endif
				assert(key == get_key(i));
			}
		}

		for (size_t i = 0; i < N; i++) {
			size_t x = rand() % LIMIT;
			btree_key_t key = get_key(x);
#ifdef STRING_MAP
			btree_insert_sequential(&btree, key, x);
#else
			btree_insert_sequential(&btree, key);
#endif
			assert(btree_check(&btree, &btree_info));
		}

		btree_destroy(&btree);

		for (size_t i = 0; i < N; i++) {
			size_t x = rand() % LIMIT;
			btree_key_t key = get_key(x);
			bool exists = btable_lookup(&btable, key, get_hash(key));
			if (!exists) {
				*btable_insert(&btable, key, get_hash(key)) = key;
			}
#ifdef STRING_MAP
			bool inserted = btree_insert(&btree, key, x);
#else
			bool inserted = btree_insert(&btree, key);
#endif
			assert(exists ? !inserted : inserted);
			assert(btree_check(&btree, &btree_info));
			assert(btree_find(&btree, key));
		}

		btree_destroy(&btree);
		assert(btree._impl.height == 0);
		assert(btree_check(&btree, &btree_info));

		btable_destroy(&btable);

		for (size_t i = 0; i < N; i++) {
			btree_key_t key = get_key(i);
#ifdef STRING_MAP
			bool inserted = btree_insert(&btree, key, i);
#else
			bool inserted = btree_insert(&btree, key);
#endif
			assert(inserted);
		}

		assert(btree_check(&btree, &btree_info));

		for (size_t i = 0; i < N; i++) {
			btree_key_t key = get_key(i);
#ifdef STRING_MAP
			btree_key_t min;
			btree_value_t *value = btree_get_min(&btree, &min);
			assert(value && strtoumax(min, NULL, 10) == *value);
			assert(btree_info.cmp(&min, &key) == 0);
			btree_value_t val;
			bool removed = btree_delete_min(&btree, &min, &val);
			assert(removed && strtoumax(min, NULL, 10) == val);
			assert(btree_info.cmp(&min, &key) == 0);
#else
			const btree_key_t *min = btree_get_min(&btree);
			assert(min);
			assert(btree_info.cmp(min, &key) == 0);
			btree_key_t min_val;
			bool removed = btree_delete_min(&btree, &min_val);
			assert(removed);
			assert(btree_info.cmp(&min_val, &key) == 0);
#endif
		}

		for (size_t i = 0; i < N; i++) {
			btree_key_t key = get_key(i);
#ifdef STRING_MAP
			bool inserted = btree_insert(&btree, key, i);
#else
			bool inserted = btree_insert(&btree, key);
#endif
			assert(inserted);
		}

		assert(btree_check(&btree, &btree_info));

		for (size_t i = 0; i < N; i++) {
			btree_key_t key = get_key(N - 1 - i);
#ifdef STRING_MAP
			btree_key_t max;
			btree_value_t *value = btree_get_max(&btree, &max);
			assert(value && strtoumax(max, NULL, 10) == *value);
			assert(btree_info.cmp(&max, &key) == 0);
			btree_value_t val;
			bool removed = btree_delete_max(&btree, &max, &val);
			assert(removed && strtoumax(max, NULL, 10) == val);
			assert(btree_info.cmp(&max, &key) == 0);
#else
			const btree_key_t *max = btree_get_max(&btree);
			assert(max);
			assert(btree_info.cmp(max, &key) == 0);
			btree_key_t max_val;
			bool removed = btree_delete_max(&btree, &max_val);
			assert(removed);
			assert(btree_info.cmp(&max_val, &key) == 0);
#endif
		}

		assert(btree._impl.height == 0);

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

	static struct btree_node *btree_node_copy(struct btree_node *node, unsigned int depth,
						  const struct btree_info *info)
	{
		struct btree_node *copy = btree_new_node(depth == 0, info);
		memcpy(btree_node_item(copy, 0, info),
		       btree_node_item(node, 0, info),
		       node->num_items * info->item_size);
		copy->num_items = node->num_items;
		if (depth == 0) {
			return copy;
		}
		for (unsigned int i = 0; i < node->num_items + 1; i++) {
			struct btree_node *child = btree_node_get_child(node, i, info);
			struct btree_node *child_copy = btree_node_copy(child, depth - 1, info);
			btree_node_set_child(copy, i, child_copy, info);
		}
		return copy;
	}

	static struct btree btree_copy(const struct btree *btree, const struct btree_info *info)
	{
		unsigned int height = btree->_impl.height;
		struct btree copy;
		copy._impl.height = height;
		copy._impl.root = height == 0 ? NULL : btree_node_copy(btree->_impl.root, height - 1, info);
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
#ifdef STRING_MAP
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
#ifdef STRING_MAP
				btree_insert(&btree, key, i);
#else
				btree_insert(&btree, key);
#endif
			}
			clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);
			inorder_insert[k] = ns_elapsed(start_tp, end_tp);

			assert(btree_check(&btree, &btree_info));
			btree_destroy(&btree);

			clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp);
			for (size_t i = 0; i < N; i++) {
				btree_key_t key = get_key(i);
#ifdef STRING_MAP
				btree_insert_sequential(&btree, key, i);
#else
				btree_insert_sequential(&btree, key);
#endif
			}
			clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);
			inorder_fast_insert[k] = ns_elapsed(start_tp, end_tp);

			assert(btree_check(&btree, &btree_info));
			btree_destroy(&btree);

			clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp);
			for (size_t i = 0; i < N; i++) {
				size_t x = N - 1 - i;
				btree_key_t key = get_key(x);
#ifdef STRING_MAP
				btree_insert(&btree, key, x);
#else
				btree_insert(&btree, key);
#endif
			}
			clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);
			revorder_insert[k] = ns_elapsed(start_tp, end_tp);

			assert(btree_check(&btree, &btree_info));
			btree_destroy(&btree);

			clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp);
			for (size_t i = 0; i < N; i++) {
				btree_key_t key = get_random_key(i);
#ifdef STRING_MAP
				btree_insert(&btree, key, random_numbers[i]);
#else
				btree_insert(&btree, key);
#endif
			}
			clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);
			random_insert[k] = ns_elapsed(start_tp, end_tp);

			assert(btree_check(&btree, &btree_info));

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
#ifdef STRING_MAP
				btree_value_t value;
				btree_iter_get_next(&iter, &prev_key, &value);
				while (btree_iter_get_next(&iter, &key, &value)) {
					assert(btree_info.cmp(&prev_key, &key) < 0);
					prev_key = key;
				}
#else
				btree_iter_get_next(&iter, &prev_key);
				while (btree_iter_get_next(&iter, &key)) {
					assert(btree_info.cmp(&prev_key, &key) < 0);
					prev_key = key;
				}
#endif
			}
			clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);
			iteration[k] = ns_elapsed(start_tp, end_tp);

			struct btree copy = btree_copy(&btree, &btree_info);

			clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp);
			for (size_t i = 0; i < N; i++) {
				btree_key_t key = get_random_key(i);
#ifdef STRING_MAP
				bool found = btree_delete(&btree, key, &key, NULL);
#else
				bool found = btree_delete(&btree, key, &key);
#endif
				asm volatile("" :: "g" (found) : "memory");
			}
			clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);
			inorder_deletion[k] = ns_elapsed(start_tp, end_tp);

			btree = copy;
			copy = btree_copy(&btree, &btree_info);

			clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp);
			for (size_t i = 0; i < N; i++) {
				btree_key_t key = get_random_key(N - 1 - i);
#ifdef STRING_MAP
				bool found = btree_delete(&btree, key, &key, NULL);
#else
				bool found = btree_delete(&btree, key, &key);
#endif
				asm volatile("" :: "g" (found) : "memory");
			}
			clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);
			revorder_deletion[k] = ns_elapsed(start_tp, end_tp);

			btree = copy;
			copy = btree_copy(&btree, &btree_info);

			clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp);
			for (size_t i = 0; i < N; i++) {
				btree_key_t key = get_random_key(random_numbers2[i] % N);
#ifdef STRING_MAP
				bool found = btree_delete(&btree, key, &key, NULL);
#else
				bool found = btree_delete(&btree, key, &key);
#endif
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
					size_t z = (i * INSERTIONS + j) % (2 * N);
					btree_key_t key = get_key(z);
#ifdef STRING_MAP
					btree_insert(&btree, key, z);
#else
					btree_insert(&btree, key);
#endif
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
#ifdef STRING_MAP
					btree_delete(&btree, key, &key, NULL);
#else
					btree_delete(&btree, key, &key);
#endif
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
					size_t z = 2 * N - 1 - ((i * INSERTIONS + j) % (2 * N));
					btree_key_t key = get_key(z);
#ifdef STRING_MAP
					btree_insert(&btree, key, z);
#else
					btree_insert(&btree, key);
#endif
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
#ifdef STRING_MAP
					btree_delete(&btree, key, &key, NULL);
#else
					btree_delete(&btree, key, &key);
#endif
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
					size_t z = (i * INSERTIONS + j) % (2 * N);
					btree_key_t key = get_random_key(z);
#ifdef STRING_MAP
					btree_insert(&btree, key, random_numbers[z]);
#else
					btree_insert(&btree, key);
#endif
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
#ifdef STRING_MAP
					btree_delete(&btree, key, &key, NULL);
#else
					btree_delete(&btree, key, &key);
#endif
				}
			}
			clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);
			random_mixed[k] = ns_elapsed(start_tp, end_tp);

			btree_destroy(&btree);
		}
		// TODO print more statistics or maybe just print minimum instead
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

	int main(void)
	{
		test();
		// benchmark();
	}
