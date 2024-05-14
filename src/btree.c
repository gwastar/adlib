/*
 * Copyright (C) 2024 Fabian Hügel
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdlib.h>
#include <string.h>

#include "btree.h"
#include "compiler.h"

// TODO fix bad performance when compiling with gcc
//      (gcc specializes the API functions for the info parameter but even with __attribute__((flatten)) on all
//       API functions or __attribute__((always_inline)) on the compare function, gcc will never inline the
//       compare function (info->cmp), which really hurts performance for btree_set_benchmark since the keys
//       are simple integers and the compare function is trivial) (PGO fixes this issue)
// TODO is get_lower/upper_bound useful or are the iterator variants good enough?
// TODO implement APIs with hints for optimized bulk operations?
//      (figure out why the initial implementation did not improve performance...)

static void *btree_node_item(struct _btree_node *node, unsigned int idx, const struct btree_info *info)
{
	return node->data + info->alignment_offset + idx * info->item_size;
}

void *_btree_debug_node_item(struct _btree_node *node, unsigned int idx, const struct btree_info *info)
{
	return btree_node_item(node, idx, info);
}

static struct _btree_node **btree_node_children(struct _btree_node *node, const struct btree_info *info)
{
	return (struct _btree_node **)btree_node_item(node, info->max_items, info);
}

static struct _btree_node *btree_node_get_child(struct _btree_node *node, unsigned int idx,
					       const struct btree_info *info)
{
	return btree_node_children(node, info)[idx];
}

struct _btree_node *_btree_debug_node_get_child(struct _btree_node *node, unsigned int idx,
						const struct btree_info *info)
{
	return btree_node_get_child(node, idx, info);
}

static void btree_node_set_child(struct _btree_node *node, unsigned int idx, struct _btree_node *child,
				 const struct btree_info *info)
{
	btree_node_children(node, info)[idx] = child;
}

static void btree_node_copy_child(struct _btree_node *dest_node, unsigned int dest_idx,
				  struct _btree_node *src_node, unsigned int src_idx,
				  const struct btree_info *info)
{
	btree_node_set_child(dest_node, dest_idx, btree_node_get_child(src_node, src_idx, info), info);
}

static void btree_node_get_item(void *item, struct _btree_node *node, unsigned int idx,
				const struct btree_info *info)
{
	memcpy(item, btree_node_item(node, idx, info), info->item_size);
}

static void btree_node_set_item(struct _btree_node *node, unsigned int idx, const void *item,
				const struct btree_info *info)
{
	memcpy(btree_node_item(node, idx, info), item, info->item_size);
}

static void btree_node_copy_item(struct _btree_node *dest_node, unsigned int dest_idx,
				 struct _btree_node *src_node, unsigned int src_idx,
				 const struct btree_info *info)
{
	btree_node_set_item(dest_node, dest_idx, btree_node_item(src_node, src_idx, info),info);
}

static bool btree_node_search(struct _btree_node *node, const void *key, unsigned int *ret_idx,
			      const struct btree_info *info)
{
	unsigned int start = 0;
	unsigned int end = node->num_items;
	bool found = false;
	unsigned int idx;
	while (start + info->linear_search_threshold < end) {
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

	if (info->linear_search_threshold == 0) {
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

void *_btree_iter_start(struct btree_iter *iter, const struct _btree *tree, bool rightmost,
			const struct btree_info *info)
{
	iter->tree = tree;
	iter->depth = 0;

	if (tree->height == 0) {
		return NULL;
	}

	struct _btree_node *node = tree->root;
	iter->path[0].idx = rightmost ? node->num_items : 0;
	iter->path[0].node = node;

	struct _btree_pos *pos = &iter->path[0];
	for (iter->depth = 1; iter->depth < tree->height; iter->depth++) {
		node = btree_node_get_child(node, pos->idx, info);
		pos = &iter->path[iter->depth];
		pos->idx = rightmost ? node->num_items : 0;
		pos->node = node;
	}
	if (rightmost) {
		pos->idx--;
	}
	return btree_node_item(pos->node, pos->idx, info);
}

void *_btree_iter_next(struct btree_iter *iter, const struct btree_info *info)
{
	if (iter->depth == 0) {
		return NULL;
	}
	struct _btree_pos *pos = &iter->path[iter->depth - 1];
	pos->idx++;
	// descend into the leftmost child of the right child
	while (iter->depth < iter->tree->height) {
		struct _btree_node *child = btree_node_get_child(pos->node, pos->idx, info);
		pos = &iter->path[iter->depth++];
		pos->node = child;
		pos->idx = 0;
	}
	while (pos->idx >= pos->node->num_items) {
		if (--iter->depth == 0) {
			return NULL;
		}
		pos = &iter->path[iter->depth - 1];
	}
	return btree_node_item(pos->node, pos->idx, info);
}

void *_btree_iter_prev(struct btree_iter *iter, const struct btree_info *info)
{
	if (iter->depth == 0) {
		return NULL;
	}
	struct _btree_pos *pos = &iter->path[iter->depth - 1];
	// descend into the rightmost child of the left child
	while (iter->depth < iter->tree->height) {
		struct _btree_node *child = btree_node_get_child(pos->node, pos->idx, info);
		pos = &iter->path[iter->depth++];
		pos->node = child;
		pos->idx = child->num_items;
	}
	while (pos->idx == 0) {
		if (--iter->depth == 0) {
			return NULL;
		}
		pos = &iter->path[iter->depth - 1];
	}
	pos->idx--;
	return btree_node_item(pos->node, pos->idx, info);
}

// TODO could unify this with _btree_iter_start?
void *_btree_iter_start_at(struct btree_iter *iter, const struct _btree *tree, void *key,
			   enum btree_iter_start_at_mode mode, const struct btree_info *info)
{
	iter->tree = tree;
	iter->depth = 0;

	if (tree->height == 0) {
		return NULL;
	}

	struct _btree_node *node = tree->root;
	struct _btree_pos *pos;
	for (;;) {
		pos = &iter->path[iter->depth++];
		pos->node = node;
		if (btree_node_search(node, key, &pos->idx, info)) {
			if (mode == BTREE_ITER_LOWER_BOUND_EXCLUSIVE) {
				return _btree_iter_next(iter, info);
			} else if (mode == BTREE_ITER_UPPER_BOUND_EXCLUSIVE) {
				return _btree_iter_prev(iter, info);
			}
			break;
		}
		if (iter->depth == tree->height) {
			switch (mode) {
			case BTREE_ITER_FIND_KEY:
				return NULL;
			case BTREE_ITER_LOWER_BOUND_INCLUSIVE:
			case BTREE_ITER_LOWER_BOUND_EXCLUSIVE:
				if (pos->idx == pos->node->num_items) {
					return _btree_iter_next(iter, info);
				}
				break;
			case BTREE_ITER_UPPER_BOUND_INCLUSIVE:
			case BTREE_ITER_UPPER_BOUND_EXCLUSIVE:
				return _btree_iter_prev(iter, info);
			}
			break;
		}
		node = btree_node_get_child(node, pos->idx, info);
	}
	return btree_node_item(pos->node, pos->idx, info);
}

static struct _btree_node *btree_new_node(bool leaf, const struct btree_info *info)
{
	size_t items_size = info->max_items * info->item_size;
	size_t children_size = leaf ? 0 : (info->max_items + 1) * sizeof(struct _btree_node *);
	struct _btree_node *node = malloc(sizeof(*node) + info->alignment_offset + items_size + children_size);
	node->num_items = 0;
	return node;
}

void _btree_init(struct _btree *tree)
{
	memset(tree, 0, sizeof(*tree));
}

void _btree_destroy(struct _btree *tree, const struct btree_info *info)
{
	if (tree->height == 0) {
		return;
	}
	struct _btree_pos path[32];
	struct _btree_node *node = tree->root;
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
		struct _btree_pos *pos = &path[depth - 1];
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
			struct _btree_node *child = btree_node_get_child(pos->node, pos->idx, info);
			pos = &path[depth++];
			pos->node = child;
			pos->idx = 0;
		} while (depth < tree->height);
	}
}

void *_btree_find(const struct _btree *tree, const void *key, const struct btree_info *info)
{
	if (tree->height == 0) {
		return NULL;
	}
	struct _btree_node *node = tree->root;
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

void *_btree_get_leftmost_rightmost(const struct _btree *tree, bool leftmost, const struct btree_info *info)
{
	if (tree->height == 0) {
		return NULL;
	}
	struct _btree_node *node = tree->root;
	unsigned int depth = 0;
	for (;;) {
		if (++depth == tree->height) {
			break;
		}
		unsigned int idx = leftmost ? 0 : node->num_items;
		node = btree_node_get_child(node, idx, info);
	}
	unsigned int idx = leftmost ? 0 : node->num_items - 1;
	return btree_node_item(node, idx, info);
}

static void btree_node_shift_items_right(struct _btree_node *node, unsigned int idx,
					 const struct btree_info *info)
{
	memmove(btree_node_item(node, idx + 1, info),
		btree_node_item(node, idx, info),
		(node->num_items - idx) * info->item_size);
}

static void btree_node_shift_children_right(struct _btree_node *node, unsigned int idx,
					    const struct btree_info *info)
{
	memmove(btree_node_children(node, info) + idx + 1,
		btree_node_children(node, info) + idx,
		(node->num_items + 1 - idx) * sizeof(struct _btree_node *));
}

static void btree_node_shift_items_left(struct _btree_node *node, unsigned int idx,
					const struct btree_info *info)
{
	memmove(btree_node_item(node, idx, info),
		btree_node_item(node, idx + 1, info),
		(node->num_items - idx - 1) * info->item_size);
}

static void btree_node_shift_children_left(struct _btree_node *node, unsigned int idx,
					   const struct btree_info *info)
{
	memmove(btree_node_children(node, info) + idx,
		btree_node_children(node, info) + idx + 1,
		(node->num_items - idx) * sizeof(struct _btree_node *));
}

bool _btree_delete(struct _btree *tree, enum _btree_deletion_mode mode, const void *key, void *ret_item,
		   const struct btree_info *info)
{
	if (tree->height == 0) {
		return false;
	}
	struct _btree_node *node = tree->root;
	unsigned int depth = 1;
	struct _btree_pos path[32];
	unsigned int idx;
	bool leaf = false;
	for (;;) {
		leaf = depth == tree->height;
		bool found = false;
		switch (mode) {
		case __BTREE_DELETE_KEY:
			if (btree_node_search(node, key, &idx, info)) {
				found = true;
			} else if (leaf) {
				// at leaf and not found
				return false;
			}
			break;
		case __BTREE_DELETE_MIN:
			idx = 0;
			break;
		case __BTREE_DELETE_MAX:
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
			mode = __BTREE_DELETE_MAX;
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
	// assert(node->num_items != 0);
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
		struct _btree_node *left = btree_node_get_child(node, idx, info);
		struct _btree_node *right = btree_node_get_child(node, idx + 1, info);

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
				       (right->num_items + 1) * sizeof(struct _btree_node *));
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
static struct _btree_node *btree_node_split_and_insert(struct _btree_node *node, unsigned int idx,
						      void *item, struct _btree_node *right,
						      const struct btree_info *info)
{
	// assert(node->num_items == info->max_items);
	struct _btree_node *new_node = btree_new_node(!right, info);
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
			       (info->min_items + 1) * sizeof(struct _btree_node *));
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
			       info->min_items * sizeof(struct _btree_node *));
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
			       info->min_items * sizeof(struct _btree_node *));
			btree_node_shift_children_right(new_node, idx + 1, info);
			btree_node_set_child(new_node, idx + 1, right, info);
		}
	}
	new_node->num_items = info->min_items;

	return new_node;
}

static void _btree_insert_and_rebalance_even(struct _btree *tree, void *item, unsigned int idx,
					     struct _btree_node *node, struct _btree_pos path[static 32],
					     unsigned int depth, unsigned int last_nonfull_node_depth,
					     const struct btree_info *info)
{
	(void)last_nonfull_node_depth;
	struct _btree_node *right = NULL;
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
	struct _btree_node *new_root = btree_new_node(false, info);
	btree_node_set_item(new_root, 0, item, info);
	new_root->num_items = 1;
	btree_node_set_child(new_root, 0, node, info);
	btree_node_set_child(new_root, 1, right, info);
	tree->root = new_root;
	tree->height++;
}

static struct _btree_node *btree_node_split(struct _btree_node *node, void *median, bool leaf,
					   const struct btree_info *info)
{
	// assert(node->num_items == info->max_items);
	struct _btree_node *new_node = btree_new_node(leaf, info);
	node->num_items = info->min_items;
	btree_node_get_item(median, node, info->min_items, info);
	memcpy(btree_node_item(new_node, 0, info),
	       btree_node_item(node, info->min_items + 1, info),
	       info->min_items * info->item_size);
	if (!leaf) {
		memcpy(btree_node_children(new_node, info),
		       btree_node_children(node, info)+ info->min_items + 1,
		       (info->min_items + 1) * sizeof(struct _btree_node *));
	}
	new_node->num_items = info->min_items;
	return new_node;
}

static void _btree_insert_and_rebalance_odd(struct _btree *tree, void *item, unsigned int idx,
					    struct _btree_node *node, struct _btree_pos path[static 32],
					    unsigned int depth, unsigned int last_nonfull_node_depth,
					    const struct btree_info *info)
{
	if (last_nonfull_node_depth != depth) {
		unsigned int d = last_nonfull_node_depth;
		if (d == 0) {
			struct _btree_node *new_root = btree_new_node(false, info);
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
			struct _btree_node *right = btree_node_split(btree_node_get_child(node, idx, info),
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
					struct _btree_node *node, struct _btree_pos path[static 32],
					unsigned int depth, unsigned int last_nonfull_node_depth,
					const struct btree_info *info)
{
	((info->max_items & 1) ? _btree_insert_and_rebalance_odd : _btree_insert_and_rebalance_even)
		(tree, item, idx, node, path, depth, last_nonfull_node_depth, info);
}

bool _btree_insert(struct _btree *tree, void *item, bool update, const struct btree_info *info)
{
	if (tree->height == 0) {
		tree->root = btree_new_node(true, info);
		tree->height = 1;
	}
	struct _btree_node *node = tree->root;
	struct _btree_pos path[32];
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

bool _btree_insert_sequential(struct _btree *tree, void *item, const struct btree_info *info)
{
	if (tree->height == 0) {
		return _btree_insert(tree, item, false, info);
	}
	struct _btree_node *node = tree->root;
	struct _btree_pos path[32];
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

static struct _btree_node *btree_node_copy(struct _btree_node *node, unsigned int depth,
					  const struct btree_info *info)
{
	struct _btree_node *copy = btree_new_node(depth == 0, info);
	memcpy(btree_node_item(copy, 0, info),
	       btree_node_item(node, 0, info),
	       node->num_items * info->item_size);
	copy->num_items = node->num_items;
	if (depth == 0) {
		return copy;
	}
	for (unsigned int i = 0; i < node->num_items + 1u; i++) {
		struct _btree_node *child = btree_node_get_child(node, i, info);
		struct _btree_node *child_copy = btree_node_copy(child, depth - 1, info);
		btree_node_set_child(copy, i, child_copy, info);
	}
	return copy;
}

struct _btree _btree_debug_copy(const struct _btree *tree, const struct btree_info *info)
{
	unsigned int height = tree->height;
	struct _btree copy;
	copy.height = height;
	copy.root = height == 0 ? NULL : btree_node_copy(tree->root, height - 1, info);
	return copy;
}
