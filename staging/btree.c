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

// TODO figure out the API
// TODO investigate the performance difference between gcc and clang
// TODO implement APIs with hints for optimized bulk operations
//      (figure out why the initial implementation did not improve performance...)

// #define STRING_MAP

#define LINEAR_SEARCH_THRESHOLD 0

#define BTREE_EMPTY {.root = NULL, .height = 0}

enum _btree_deletion_mode {
	DELETE_MIN,
	DELETE_MAX,
	DELETE_KEY,
};

#define __DEFINE_BTREE_COMMON(name, MAX_ITEMS, MIN_ITEMS, ...)		\
	_Static_assert(MAX_ITEMS >= 2, "use an AVL or RB tree for 1 item per node"); \
	static const unsigned int _##name##_max_items = MAX_ITEMS;	\
	static const unsigned int _##name##_min_items = MIN_ITEMS;	\
									\
	static int _##name##_compare(const name##_key_t a, const name##_key_t b) \
	{								\
		return (__VA_ARGS__);					\
	}								\
									\
	struct name##_node {						\
		unsigned int num_items;					\
		_##name##_item_t items[MAX_ITEMS];			\
		struct name##_node *children[/* MAX_ITEMS + 1 */];	\
	};								\
									\
	struct name {							\
		struct name##_node *root;				\
		unsigned int height; /* 0 means root is NULL, 1 means root is leaf */ \
	};								\
									\
	struct name##_iter {						\
		const struct name *tree;				\
		unsigned int depth;					\
		struct name##_pos {					\
			struct name##_node *node;			\
			unsigned int idx;				\
		} path[32];						\
	};								\
									\
	static struct name##_iter name##_iter_start(const struct name *tree) \
	{								\
		struct name##_iter iter;					\
		iter.tree = tree;					\
		iter.depth = 0;						\
									\
		if (tree->height == 0) {				\
			return iter;					\
		}							\
									\
		struct name##_node *node = tree->root;			\
		iter.path[0].idx = 0;					\
		iter.path[0].node = node;				\
									\
		for (iter.depth = 1; iter.depth < tree->height; iter.depth++) { \
			node = node->children[0];			\
			iter.path[iter.depth].idx = 0;			\
			iter.path[iter.depth].node = node;		\
		}							\
		return iter;						\
	}								\
									\
	static bool _##name##_iter_get_next(struct name##_iter *iter, _##name##_item_t *item) \
	{								\
		if (iter->depth == 0) {					\
			return false;					\
		}							\
		struct name##_pos *pos = &iter->path[iter->depth - 1];	\
		*item = pos->node->items[pos->idx];			\
		pos->idx++;						\
		/* descend into the leftmost child of the right child */ \
		while (iter->depth < iter->tree->height) {		\
			struct name##_node *child = pos->node->children[pos->idx]; \
			pos = &iter->path[iter->depth++];		\
			pos->node = child;				\
			pos->idx = 0;					\
		}							\
		while (pos->idx >= pos->node->num_items) {		\
			if (--iter->depth == 0) {			\
				break;					\
			}						\
			pos = &iter->path[iter->depth - 1];		\
		}							\
		return true;						\
	}								\
									\
	static struct name##_node *_##name##_new_node(bool leaf)		\
	{								\
		struct name##_node *node = malloc(sizeof(*node) + (leaf ? 0 : (_##name##_max_items + 1) * sizeof(node->children[0]))); \
		node->num_items = 0;					\
		return node;						\
	}								\
									\
									\
	static void name##_init(struct name *tree)			\
	{								\
		*tree = (struct btree)BTREE_EMPTY;			\
	}								\
									\
	static void _##name##_destroy(struct name *tree, struct _##name##_destructors destructors) \
	{								\
		if (tree->height == 0) {				\
			return;						\
		}							\
		struct name##_pos path[32];				\
		struct name##_node *node = tree->root;			\
		path[0].idx = 0;					\
		path[0].node = node;					\
		unsigned int depth;					\
		for (depth = 1; depth < tree->height; depth++) {	\
			node = node->children[0];			\
			path[depth].idx = 0;				\
			path[depth].node = node;			\
		}							\
									\
		for (;;) {						\
			/* start at leaf */				\
			struct name##_pos *pos = &path[depth - 1];	\
			/* free the leaf and go up, if we are at the end of the current node free it and keep going up */ \
			do {						\
				for (unsigned int i = 0; i < pos->node->num_items; i++) { \
					_##name##_destroy_item(&pos->node->items[i], destructors); \
				}					\
				free(pos->node);			\
				if (--depth == 0) {			\
					tree->root = NULL;		\
					tree->height = 0;		\
					return;				\
				}					\
				pos = &path[depth - 1];			\
			} while (pos->idx >= pos->node->num_items);	\
			/* descend into the leftmost child of the right child */ \
			pos->idx++;					\
			do {						\
				struct name##_node *child = pos->node->children[pos->idx]; \
				pos = &path[depth++];			\
				pos->node = child;			\
				pos->idx = 0;				\
			} while (depth < tree->height);			\
		}							\
	}								\
									\
	static bool _##name##_node_search(const struct name##_node *node, const name##_key_t key, unsigned int *ret_idx) \
	{								\
		unsigned int start = 0;					\
		unsigned int end = node->num_items;			\
		compiler_assume(end <= _##name##_max_items);			\
		bool found = false;					\
		unsigned int idx;					\
		while (start + LINEAR_SEARCH_THRESHOLD < end) {		\
			unsigned int mid = (start + end) / 2; /* start + end should never overflow */ \
			int cmp = _##name##_compare(key, node->items[mid].key);	\
			if (cmp == 0) {					\
				idx = mid;				\
				found = true;				\
				goto done;				\
			} else if (cmp > 0) {				\
				start = mid + 1;			\
			} else {					\
				end = mid;				\
			}						\
		}							\
									\
		if (LINEAR_SEARCH_THRESHOLD == 0) {			\
			idx = start;					\
			goto done;					\
		}							\
									\
		while (start < end) {					\
			int cmp = _##name##_compare(key, node->items[start].key); \
			if (cmp == 0) {					\
				idx = start;				\
				found = true;				\
				goto done;				\
			}						\
			if (cmp < 0) {					\
				break;					\
			}						\
			start++;					\
			end--;						\
			cmp = _##name##_compare(key, node->items[end].key);	\
			if (cmp == 0) {					\
				idx = end;				\
				found = true;				\
				goto done;				\
			}						\
			if (cmp > 0) {					\
				start = end + 1;			\
				break;					\
			}						\
		}							\
		idx = start;						\
	done:								\
		*ret_idx = idx;						\
		return found;						\
	}								\
									\
	static _##name##_item_t *_##name##_find(const struct name *tree, name##_key_t key) \
	{								\
		if (tree->height == 0) {				\
			return NULL;					\
		}							\
		struct name##_node *node = tree->root;			\
		unsigned int depth = 1;					\
		for (;;) {						\
			unsigned int idx;				\
			if (_##name##_node_search(node, key, &idx)) {	\
				return &node->items[idx];		\
			}						\
			if (depth++ == tree->height) {			\
				break;					\
			}						\
			node = node->children[idx];			\
		}							\
		return NULL;						\
	}								\
									\
	static bool _##name##_get_minmax(const struct name *tree, _##name##_item_t *item, bool min) \
	{								\
		if (tree->height == 0) {				\
			return false;					\
		}							\
		const struct name##_node *node = tree->root;		\
		unsigned int depth = 0;					\
		for (;;) {						\
			if (++depth == tree->height) {			\
				break;					\
			}						\
			unsigned int idx = min ? 0 : node->num_items;	\
			node = node->children[idx];			\
		}							\
		unsigned int idx = min ? 0 : node->num_items - 1;	\
		*item = node->items[idx];				\
		return true;						\
	}								\
									\
	static void _##name##_node_shift_items_right(struct name##_node *node, unsigned int idx) \
	{								\
		memmove(node->items + idx + 1, node->items + idx, (node->num_items - idx) * sizeof(node->items[0])); \
	}								\
									\
	static void _##name##_node_shift_children_right(struct name##_node *node, unsigned int idx) \
	{								\
		memmove(node->children + idx + 1,			\
			node->children + idx,				\
			(node->num_items + 1 - idx) * sizeof(node->children[0])); \
	}								\
									\
	static void _##name##_node_shift_items_left(struct name##_node *node, unsigned int idx) \
	{								\
		memmove(node->items + idx, node->items + idx + 1, (node->num_items - idx - 1) * sizeof(node->items[0])); \
	}								\
									\
	static void _##name##_node_shift_children_left(struct name##_node *node, unsigned int idx) \
	{								\
		memmove(node->children + idx,				\
			node->children + idx + 1,			\
			(node->num_items - idx) * sizeof(node->children[0])); \
	}								\
									\
	static bool _##name##_delete(struct name *tree, enum _btree_deletion_mode mode, name##_key_t key, \
				  _##name##_item_t *ret_item)		\
	{								\
		if (tree->height == 0) {				\
			return false;					\
		}							\
		struct name##_node *node = tree->root;			\
		unsigned int depth = 1;					\
		struct name##_pos path[32];				\
		unsigned int idx;					\
		bool leaf = false;					\
		for (;;) {						\
			leaf = depth == tree->height;			\
			bool found = false;				\
			switch (mode) {					\
			case DELETE_KEY:				\
				if (_##name##_node_search(node, key, &idx)) { \
					found = true;			\
				} else if (leaf) {			\
					/* at leaf and not found */	\
					return false;			\
				}					\
				break;					\
			case DELETE_MIN:				\
				idx = 0;				\
				break;					\
			case DELETE_MAX:				\
				idx = leaf ? node->num_items - 1 : node->num_items; \
				break;					\
			}						\
			if (leaf) {					\
				break;					\
			}						\
			if (found) {					\
				/* we found the key in an internal node */ \
				/* now return it and then replace it with the max value of the left suname */ \
				if (ret_item) {				\
					*ret_item = node->items[idx];	\
				}					\
				ret_item = &node->items[idx];		\
				mode = DELETE_MAX;			\
			}						\
			path[depth - 1].idx = idx;			\
			path[depth - 1].node = node;			\
			depth++;					\
			node = node->children[idx];			\
		}							\
									\
		/* we are at the leaf and have found the item to delete */ \
		/* return the item and rebalance */			\
		if (ret_item) {						\
			*ret_item = node->items[idx];			\
		}							\
		_##name##_node_shift_items_left(node, idx);		\
		assert(node->num_items != 0);				\
		node->num_items--;					\
									\
		while (--depth > 0) {					\
			if (node->num_items >= _##name##_min_items) {		\
				return true;				\
			}						\
									\
			node = path[depth - 1].node;			\
			idx = path[depth - 1].idx;			\
									\
			/* eagerly merging with the left child slightly improves performance for delete-only benchmarks */ \
			if (idx == node->num_items ||			\
			    (idx != 0 && node->children[idx - 1]->num_items + node->children[idx]->num_items < _##name##_max_items)) { \
				idx--;					\
			}						\
			struct name##_node *left = node->children[idx];	\
			struct name##_node *right = node->children[idx + 1]; \
									\
			if (left->num_items + right->num_items < _##name##_max_items) { \
				left->items[left->num_items] = node->items[idx]; \
				_##name##_node_shift_items_left(node, idx); \
				_##name##_node_shift_children_left(node, idx + 1); \
				node->num_items--;			\
				left->num_items++;			\
				memcpy(left->items + left->num_items, right->items, right->num_items * sizeof(right->items[0])); \
				if (!leaf) {				\
					memcpy(left->children + left->num_items, \
					       right->children,		\
					       (right->num_items + 1) * sizeof(right->children[0])); \
				}					\
				left->num_items += right->num_items;	\
				free(right);				\
			} else if (left->num_items > right->num_items) { \
				_##name##_node_shift_items_right(right, 0); \
				right->items[0] = node->items[idx];	\
				node->items[idx] = left->items[left->num_items - 1]; \
				if (!leaf) {				\
					_##name##_node_shift_children_right(right, 0); \
					right->children[0] = left->children[left->num_items]; \
				}					\
				left->num_items--;			\
				right->num_items++;			\
			} else {					\
				left->items[left->num_items] = node->items[idx]; \
				node->items[idx] = right->items[0];	\
				_##name##_node_shift_items_left(right, 0);	\
				if (!leaf) {				\
					left->children[left->num_items + 1] = right->children[0]; \
					_##name##_node_shift_children_left(right, 0); \
				}					\
				right->num_items--;			\
				left->num_items++;			\
			}						\
									\
			leaf = false;					\
		}							\
									\
		/* assert(node == tree->root); */			\
									\
		if (node->num_items == 0) {				\
			tree->root = NULL;				\
			if (tree->height > 1) {				\
				tree->root = node->children[0];		\
			}						\
			tree->height--;					\
			free(node);					\
		}							\
									\
		return true;						\
	}								\
									\
	static struct name##_node *name##_node_split_and_insert(struct name##_node *node, unsigned int idx, \
							      _##name##_item_t item, struct name##_node *right, \
							      _##name##_item_t *median) \
	{								\
		assert(node->num_items == _##name##_max_items);			\
		struct name##_node *new_node = _##name##_new_node(!right);	\
		node->num_items = _##name##_min_items;				\
		if (idx < _##name##_min_items) {					\
			*median = node->items[_##name##_min_items - 1];		\
			memcpy(new_node->items, node->items + _##name##_min_items, _##name##_min_items * sizeof(node->items[0])); \
			_##name##_node_shift_items_right(node, idx);	\
			node->items[idx] = item;			\
			if (right) {					\
				memcpy(new_node->children,		\
				       node->children + _##name##_min_items,	\
				       (_##name##_min_items + 1) * sizeof(node->children[0])); \
				_##name##_node_shift_children_right(node, idx + 1); \
				node->children[idx + 1] = right;	\
			}						\
		} else if (idx == _##name##_min_items) {				\
			*median = item; /* TODO could avoid this copy... but is it even worth it? */ \
			memcpy(new_node->items, node->items + _##name##_min_items, _##name##_min_items * sizeof(node->items[0])); \
			if (right) {					\
				memcpy(new_node->children + 1,		\
				       node->children + _##name##_min_items + 1,	\
				       _##name##_min_items * sizeof(node->children[0])); \
				new_node->children[0] = right;		\
			}						\
		} else {						\
			idx -= _##name##_min_items + 1;				\
			*median = node->items[_##name##_min_items];		\
			new_node->num_items = _##name##_min_items - 1;		\
			/* it is not worth splitting the memcpys here in two to avoid the shifts */ \
			memcpy(new_node->items, node->items + _##name##_min_items + 1, (_##name##_min_items - 1) * sizeof(node->items[0])); \
			_##name##_node_shift_items_right(new_node, idx);	\
			new_node->items[idx] = item;			\
			if (right) {					\
				memcpy(new_node->children,		\
				       node->children + _##name##_min_items + 1,	\
				       _##name##_min_items * sizeof(node->children[0])); \
				_##name##_node_shift_children_right(new_node, idx + 1); \
				new_node->children[idx + 1] = right;	\
			}						\
		}							\
		new_node->num_items = _##name##_min_items;			\
									\
		return new_node;					\
	}								\
									\
	static void _##name##_insert_and_rebalance_even(struct name *tree, _##name##_item_t item, unsigned int idx, \
						     struct name##_node *node, struct name##_pos path[static 32],	\
						     unsigned int depth, unsigned int last_nonfull_node_depth) \
	{								\
		(void)last_nonfull_node_depth;				\
		struct name##_node *right = NULL;			\
		for (;;) {						\
			if (node->num_items < _##name##_max_items) {		\
				_##name##_node_shift_items_right(node, idx); \
				node->items[idx] = item;		\
				if (right) {				\
					_##name##_node_shift_children_right(node, idx + 1); \
					node->children[idx + 1] = right; \
				}					\
				node->num_items++;			\
				return;					\
			}						\
									\
			right = name##_node_split_and_insert(node, idx, item, right, &item); \
									\
			if (--depth == 0) {				\
				break;					\
			}						\
			idx = path[depth - 1].idx;			\
			node = path[depth - 1].node;			\
		}							\
		struct name##_node *new_root = _##name##_new_node(false);	\
		new_root->items[0] = item;				\
		new_root->num_items = 1;				\
		new_root->children[0] = node;				\
		new_root->children[1] = right;				\
		tree->root = new_root;					\
		tree->height++;						\
	}								\
									\
	static struct name##_node *_##name##_node_split(struct name##_node *node, _##name##_item_t *median, bool leaf) \
	{								\
		assert(node->num_items == _##name##_max_items);			\
		struct name##_node *new_node = _##name##_new_node(leaf);	\
		node->num_items = _##name##_min_items;				\
		*median = node->items[_##name##_min_items];			\
		memcpy(new_node->items, node->items + _##name##_min_items + 1, (_##name##_min_items) * sizeof(node->items[0])); \
		if (!leaf) {						\
			memcpy(new_node->children,			\
			       node->children + _##name##_min_items + 1,		\
			       (_##name##_min_items + 1) * sizeof(node->children[0])); \
		}							\
		new_node->num_items = _##name##_min_items;			\
		return new_node;					\
	}								\
									\
	static void _##name##_insert_and_rebalance_odd(struct name *tree, _##name##_item_t item, unsigned int idx, \
						    struct name##_node *node, struct name##_pos path[static 32], \
						    unsigned int depth, unsigned int last_nonfull_node_depth) \
	{								\
		if (last_nonfull_node_depth != depth) {			\
			unsigned int d = last_nonfull_node_depth;	\
			if (d == 0) {					\
				struct name##_node *new_root = _##name##_new_node(false); \
				new_root->children[0] = tree->root;	\
				tree->root = new_root;			\
				tree->height++;				\
				idx = 0;				\
				node = new_root;			\
			} else {					\
				node = path[d - 1].node;		\
				idx = path[d - 1].idx;			\
			}						\
									\
			do {						\
				d++;					\
				_##name##_item_t median;			\
				struct name##_node *right = _##name##_node_split(node->children[idx], &median, d == depth); \
				_##name##_node_shift_items_right(node, idx); \
				node->items[idx] = median;		\
				_##name##_node_shift_children_right(node, idx + 1); \
				node->children[idx + 1] = right;	\
				node->num_items++;			\
				if (_##name##_compare(item.key, median.key) < 0) { \
					node = node->children[idx];	\
					idx = path[d - 1].idx;		\
				} else {				\
					node = node->children[idx + 1];	\
					idx = path[d - 1].idx - _##name##_min_items - 1; \
				}					\
			} while (d != depth);				\
		}							\
									\
		_##name##_node_shift_items_right(node, idx);		\
		node->items[idx] = item;				\
		node->num_items++;					\
	}								\
									\
	static void _##name##_insert_and_rebalance(struct name *tree, _##name##_item_t item, unsigned int idx, \
						struct name##_node *node, struct name##_pos path[static 32], \
						unsigned int depth, unsigned int last_nonfull_node_depth) \
	{								\
		if (_##name##_max_items % 2 == 0) {				\
			_##name##_insert_and_rebalance_even(tree, item, idx, node, path, depth, last_nonfull_node_depth); \
		} else {						\
			_##name##_insert_and_rebalance_odd(tree, item, idx, node, path, depth, last_nonfull_node_depth); \
									\
		}							\
	}								\
									\
	static bool _##name##_insert(struct name *tree, _##name##_item_t item, bool update, \
				  struct _##name##_destructors destructors)	\
	{								\
		if (tree->height == 0) {				\
			tree->root = _##name##_new_node(true);		\
			tree->height = 1;				\
		}							\
		struct name##_node *node = tree->root;			\
		struct name##_pos path[32];				\
		unsigned int depth = 1;					\
		unsigned int idx;					\
		unsigned int last_nonfull_node_depth = 0;		\
		for (;;) {						\
			if (_##name##_node_search(node, item.key, &idx)) { \
				if (update) {				\
					_##name##_destroy_item(&node->items[idx], destructors); \
					node->items[idx] = item;	\
				}					\
				return false;				\
			}						\
			if (node->num_items < _##name##_max_items) {		\
				last_nonfull_node_depth = depth;	\
			}						\
			path[depth - 1].idx = idx;			\
			path[depth - 1].node = node;			\
			if (depth == tree->height) {			\
				break;					\
			}						\
			depth++;					\
			node = node->children[idx];			\
		}							\
		_##name##_insert_and_rebalance(tree, item, idx, node, path, depth, last_nonfull_node_depth); \
		return true;						\
	}								\
									\
	static bool _##name##_insert_sequential(struct name *tree, _##name##_item_t item) \
	{								\
		if (tree->height == 0) {				\
			return _##name##_insert(tree, item, false, (struct _##name##_destructors){0}); \
		}							\
		struct name##_node *node = tree->root;			\
		struct name##_pos path[32];				\
		unsigned int depth = 1;					\
		unsigned int idx;					\
		unsigned int last_nonfull_node_depth = 0;		\
		for (;;) {						\
			idx = node->num_items;				\
			if (unlikely(_##name##_compare(item.key, node->items[idx - 1].key) <= 0)) { \
				return _##name##_insert(tree, item, false, (struct _##name##_destructors){0}); \
			}						\
			if (node->num_items < _##name##_max_items) {		\
				last_nonfull_node_depth = depth;	\
			}						\
			path[depth - 1].idx = idx;			\
			path[depth - 1].node = node;			\
			if (depth == tree->height) {			\
				break;					\
			}						\
			depth++;					\
			node = node->children[idx];			\
		}							\
		_##name##_insert_and_rebalance(tree, item, idx, node, path, depth, last_nonfull_node_depth); \
		return true;						\
	}

#define DEFINE_BTREE_MAP(name, key_type, value_type, MAX_ITEMS_PER_NODE, ...) \
	typedef key_type name##_key_t;					\
	typedef value_type name##_value_t;				\
	typedef struct { name##_key_t key; name##_value_t value; } _##name##_item_t; \
									\
	typedef void (*name##_key_destructor)(name##_key_t key);	\
	typedef void (*name##_value_destructor)(name##_value_t *value);	\
									\
	struct _##name##_destructors {					\
		name##_key_destructor key_destructor;			\
		name##_value_destructor value_destructor;		\
	};								\
									\
	static void _##name##_destroy_item(_##name##_item_t *item, struct _##name##_destructors callbacks) \
	{								\
		if (callbacks.key_destructor) {				\
			callbacks.key_destructor(item->key);		\
		}							\
		if (callbacks.value_destructor) {			\
			callbacks.value_destructor(&item->value);	\
		}							\
	}								\
									\
	__DEFINE_BTREE_COMMON(name, (MAX_ITEMS_PER_NODE), (MAX_ITEMS_PER_NODE / 2), __VA_ARGS__) \
									\
	static bool name##_iter_get_next(struct name##_iter *iter, name##_key_t *key, name##_value_t *value) \
	{								\
		_##name##_item_t item;					\
		bool have_next = _##name##_iter_get_next(iter, &item);	\
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
	static void name##_destroy(struct name *tree, name##_key_destructor key_destructor, \
				   name##_value_destructor value_destructor) \
	{								\
		struct _##name##_destructors destructors = {		\
			.key_destructor = key_destructor,		\
			.value_destructor = value_destructor,		\
		};							\
		_##name##_destroy(tree, destructors);			\
	}								\
									\
	static name##_value_t *name##_find(const struct name *tree, name##_key_t key) \
	{								\
		_##name##_item_t *item = _##name##_find(tree, key);	\
		return item ? &item->value : NULL;			\
	}								\
									\
	static bool name##_get_min(const struct name *tree, name##_key_t *ret_key, name##_value_t *ret_value) \
	{								\
		_##name##_item_t item;					\
		bool found = _##name##_get_minmax(tree, &item, true);	\
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
	static bool name##_get_max(const struct name *tree, name##_key_t *ret_key, name##_value_t *ret_value) \
	{								\
		_##name##_item_t item;					\
		bool found = _##name##_get_minmax(tree, &item, false);	\
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
	static bool name##_delete(struct name *tree, name##_key_t key, name##_key_t *ret_key, \
				  name##_value_t *ret_value)		\
	{								\
		_##name##_item_t item;					\
		bool found = _##name##_delete(tree, DELETE_KEY, key, &item); \
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
		bool found = _##name##_delete(tree, DELETE_MIN, item.key /* dummy */, &item); \
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
		bool found = _##name##_delete(tree, DELETE_MAX, item.key /* dummy */, &item); \
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
		return _##name##_insert(tree, (_##name##_item_t){.key = key, .value = value}, false, \
					(struct _##name##_destructors){0}); \
	}								\
									\
	static bool name##_set(struct name *tree, name##_key_t key, name##_value_t value, \
			       name##_key_destructor key_destructor, name##_value_destructor value_destructor) \
	{								\
		struct _##name##_destructors destructors = {		\
			.key_destructor = key_destructor,		\
			.value_destructor = value_destructor,		\
		};							\
		return _##name##_insert(tree, (_##name##_item_t){.key = key, .value = value}, true, destructors); \
	}								\
									\
	static bool name##_insert_sequential(struct name *tree, name##_key_t key, name##_value_t value) \
	{								\
		return _##name##_insert_sequential(tree, (_##name##_item_t){.key = key, .value = value}); \
	}

#define DEFINE_BTREE_SET(name, key_type, MAX_ITEMS_PER_NODE, ...)	\
	typedef key_type name##_key_t;					\
	typedef struct { name##_key_t key; } _##name##_item_t;		\
	typedef void (*name##_key_destructor)(name##_key_t key);	\
									\
	struct _##name##_destructors {					\
		name##_key_destructor key_destructor;			\
	};								\
									\
	static void _##name##_destroy_item(_##name##_item_t *item, struct _##name##_destructors callbacks) \
	{								\
		if (callbacks.key_destructor) {				\
			callbacks.key_destructor(item->key);		\
		}							\
	}								\
									\
	__DEFINE_BTREE_COMMON(name, (MAX_ITEMS_PER_NODE), (MAX_ITEMS_PER_NODE / 2), __VA_ARGS__) \
									\
	static bool name##_iter_get_next(struct name##_iter *iter, name##_key_t *key) \
	{								\
		return _##name##_iter_get_next(iter, (_##name##_item_t *)key); \
	}								\
									\
	static void name##_destroy(struct name *tree, name##_key_destructor key_destructor) \
	{								\
		struct _##name##_destructors destructors = {		\
			.key_destructor = key_destructor,		\
		};							\
		_##name##_destroy(tree, destructors);			\
	}								\
									\
	static bool name##_find(const struct name *tree, name##_key_t key) \
	{								\
		return _##name##_find(tree, key);				\
	}								\
									\
	static bool name##_get_min(const struct name *tree, name##_key_t *ret_key) \
	{								\
		return _##name##_get_minmax(tree, (_##name##_item_t *)ret_key, true); \
	}								\
									\
	static bool name##_get_max(const struct name *tree, name##_key_t *ret_key) \
	{								\
		return _##name##_get_minmax(tree, (_##name##_item_t *)ret_key, false); \
	}								\
									\
	static bool name##_delete(struct name *tree, name##_key_t key, name##_key_t *ret_key) \
	{								\
		return _##name##_delete(tree, DELETE_KEY, key, (_##name##_item_t *)ret_key); \
	}								\
									\
	static bool name##_delete_min(struct name *tree, name##_key_t *ret_key) \
	{								\
		name##_key_t dummy = 0;					\
		return _##name##_delete(tree, DELETE_MIN, dummy, (_##name##_item_t *)ret_key); \
	}								\
									\
	static bool name##_delete_max(struct name *tree, name##_key_t *ret_key) \
	{								\
		name##_key_t dummy = 0;					\
		return _##name##_delete(tree, DELETE_MAX, dummy, (_##name##_item_t *)ret_key); \
	}								\
									\
	static bool name##_insert(struct name *tree, name##_key_t key)	\
	{								\
		return _##name##_insert(tree, (_##name##_item_t){.key = key}, false, (struct _##name##_destructors){0}); \
	}								\
									\
	/* TODO does this even make sense for a btree set? */		\
	/* I guess you can use this to destruct and replace a key... */ \
	static bool name##_set(struct name *tree, name##_key_t key, name##_key_destructor key_destructor) \
	{								\
		struct _##name##_destructors destructors = {		\
			.key_destructor = key_destructor,		\
		};							\
		return _##name##_insert(tree, (_##name##_item_t){.key = key}, true, destructors); \
	}								\
									\
	static bool name##_insert_sequential(struct name *tree, name##_key_t key) \
	{								\
		return _##name##_insert_sequential(tree, (_##name##_item_t){.key = key}); \
	}


#ifdef STRING_MAP
static int compare(char *a, char *b)
{
	return strcmp(a, b);
}
DEFINE_BTREE_MAP(btree, char *, uint32_t, 127, compare(a, b))
DEFINE_HASHTABLE(btable, char *, char *, 8, *key == *entry)
#else
static int compare(int64_t a, int64_t b)
{
	return (a < b) ? -1 : (a > b);
}
DEFINE_BTREE_SET(btree, int64_t, 128, compare(a, b))
DEFINE_HASHTABLE(btable, int64_t, int64_t, 8, *key == *entry)
#endif



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
	for (unsigned int i = 0; i < node->num_items + 1; i++) {
		if (!btree_check_node(node->children[i], height - 1)) {
			return false;
		}
	}
	return true;
}

static bool btree_check_node_sorted(const struct btree_node *node)
{
	for (unsigned int i = 1; i < node->num_items; i++) {
		CHECK(compare(node->items[i - 1].key, node->items[i].key) < 0);
	}
	return true;
}

static bool btree_check_node(const struct btree_node *node, unsigned int height)
{
	CHECK(node->num_items >= _btree_min_items && node->num_items <= _btree_max_items);
	return btree_check_node_sorted(node) && btree_check_children(node, height);
}

static bool btree_check(const struct btree *btree)
{
	struct btree_node *root = btree->root;
	if (btree->height == 0) {
		CHECK(!root);
		return true;
	}
	CHECK(root->num_items >= 1 && root->num_items <= _btree_max_items);
	return btree_check_children(root, btree->height);
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
			assert(btree_check(&btree));
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
			assert(btree_check(&btree));
		}
		btable_clear(&btable);

		assert(!btree.root && btree.height == 0);

		for (size_t i = 0; i < N; i++) {
			btree_key_t key = get_key(i);
#ifdef STRING_MAP
			btree_insert_sequential(&btree, key, i);
#else
			btree_insert_sequential(&btree, key);
#endif
			assert(btree_check(&btree));
			assert(btree_find(&btree, key));
		}

		for (size_t i = 0; i < N; i++) {
			btree_key_t key = get_key(i);
#ifdef STRING_MAP
			bool inserted = btree_set(&btree, key, i + 1, NULL, NULL);
#else
			bool inserted = btree_set(&btree, key, NULL);
#endif
			assert(!inserted);
			assert(btree_check(&btree));
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
			assert(btree_check(&btree));
		}

#ifdef STRING_MAP
		btree_destroy(&btree, NULL, NULL);
#else
		btree_destroy(&btree, NULL);
#endif

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
			assert(btree_check(&btree));
			assert(btree_find(&btree, key));
		}

#ifdef STRING_MAP
		btree_destroy(&btree, NULL, NULL);
#else
		btree_destroy(&btree, NULL);
#endif
		assert(btree.height == 0);
		assert(btree_check(&btree));

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

		assert(btree_check(&btree));

		for (size_t i = 0; i < N; i++) {
			btree_key_t min;
#ifdef STRING_MAP
			btree_value_t value;
			bool found = btree_get_min(&btree, &min, &value);
			assert(found && strtoumax(min, NULL, 10) == value);
#else
			bool found = btree_get_min(&btree, &min);
			assert(found);
#endif
			assert(compare(min, get_key(i)) == 0);
#ifdef STRING_MAP
			bool removed = btree_delete_min(&btree, &min, &value);
			assert(removed && strtoumax(min, NULL, 10) == value);
#else
			bool removed = btree_delete_min(&btree, &min);
			assert(removed);
#endif
			assert(compare(min, get_key(i)) == 0);
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

		assert(btree_check(&btree));

		for (size_t i = 0; i < N; i++) {
			btree_key_t max;
#ifdef STRING_MAP
			btree_value_t value;
			bool found = btree_get_max(&btree, &max, &value);
			assert(found && strtoumax(max, NULL, 10) == value);
#else
			bool found = btree_get_max(&btree, &max);
			assert(found);
#endif
			assert(compare(max, get_key(N - 1 - i)) == 0);
#ifdef STRING_MAP
			bool removed = btree_delete_max(&btree, &max, &value);
			assert(removed && strtoumax(max, NULL, 10) == value);
#else
			bool removed = btree_delete_max(&btree, &max);
			assert(removed);
#endif
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
		struct btree_node *copy = _btree_new_node(depth == 0);
		memcpy(copy->items, node->items, node->num_items * sizeof(node->items[0]));
		copy->num_items = node->num_items;
		if (depth == 0) {
			return copy;
		}
		for (unsigned int i = 0; i < node->num_items + 1; i++) {
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

			assert(btree_check(&btree));
#ifdef STRING_MAP
			btree_destroy(&btree, NULL, NULL);
#else
			btree_destroy(&btree, NULL);
#endif

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

			assert(btree_check(&btree));
#ifdef STRING_MAP
			btree_destroy(&btree, NULL, NULL);
#else
			btree_destroy(&btree, NULL);
#endif

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

			assert(btree_check(&btree));
#ifdef STRING_MAP
			btree_destroy(&btree, NULL, NULL);
#else
			btree_destroy(&btree, NULL);
#endif

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
#ifdef STRING_MAP
				btree_value_t value;
				btree_iter_get_next(&iter, &prev_key, &value);
				while (btree_iter_get_next(&iter, &key, &value)) {
					assert(compare(prev_key, key) < 0);
					prev_key = key;
				}
#else
				btree_iter_get_next(&iter, &prev_key);
				while (btree_iter_get_next(&iter, &key)) {
					assert(compare(prev_key, key) < 0);
					prev_key = key;
				}
#endif
			}
			clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);
			iteration[k] = ns_elapsed(start_tp, end_tp);

			struct btree copy = btree_copy(&btree);

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
			copy = btree_copy(&btree);

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
			copy = btree_copy(&btree);

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

#ifdef STRING_MAP
			btree_destroy(&btree, NULL, NULL);
#else
			btree_destroy(&btree, NULL);
#endif
			btree = copy;

			clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp);
#ifdef STRING_MAP
			btree_destroy(&btree, NULL, NULL);
#else
			btree_destroy(&btree, NULL);
#endif
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

#ifdef STRING_MAP
			btree_destroy(&btree, NULL, NULL);
#else
			btree_destroy(&btree, NULL);
#endif

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

#ifdef STRING_MAP
			btree_destroy(&btree, NULL, NULL);
#else
			btree_destroy(&btree, NULL);
#endif

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

#ifdef STRING_MAP
			btree_destroy(&btree, NULL, NULL);
#else
			btree_destroy(&btree, NULL);
#endif
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

	int main(void)
	{
		test();
		// benchmark();
	}
