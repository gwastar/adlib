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

#pragma once

#include "compiler.h"
#include <stdbool.h>
#include <stddef.h>
#include <limits.h>

struct _btree_node {
	unsigned short num_items;
	unsigned char data[];
	/* char padding[info->alignment_offset] */
	/* item_t items[info->max_items]; */
	/* struct _btree_node *children[leaf ? 0 : info->max_items + 1]; */
};

struct _btree {
	struct _btree_node *root;
	unsigned char height; // 0 means root is NULL, 1 means root is leaf
};

struct btree_iter {
	const struct _btree *tree;
	unsigned int depth;
	struct _btree_pos {
		struct _btree_node *node;
		unsigned int idx;
	} path[32];
};

struct btree_info {
	size_t item_size;
	unsigned short max_items;
	unsigned short min_items; // dont really need to store this
	unsigned short alignment_offset; // this could be unsigned char
	unsigned short linear_search_threshold;
	int (*cmp)(const void *a, const void *b);
	void (*destroy_item)(void *item);
};

enum btree_iter_start_at_mode {
	BTREE_ITER_FIND_KEY,
	BTREE_ITER_LOWER_BOUND_INCLUSIVE,
	BTREE_ITER_LOWER_BOUND_EXCLUSIVE,
	BTREE_ITER_UPPER_BOUND_INCLUSIVE,
	BTREE_ITER_UPPER_BOUND_EXCLUSIVE,
};

enum _btree_deletion_mode {
	__BTREE_DELETE_MIN,
	__BTREE_DELETE_MAX,
	__BTREE_DELETE_KEY,
};

// use a simple heuristic to determine the threshold at which linear search becomes faster than binary search
// TODO add float and double?
#define __BTREE_LINEAR_SEARCH_THRESHOLD(type) _Generic(*(type *)0,	\
						       char : 32,	\
						       unsigned char : 32, \
						       unsigned short : 32, \
						       unsigned int : 32, \
						       unsigned long : 32, \
						       unsigned long long : 32,	\
						       signed char : 32, \
						       signed short : 32, \
						       signed int : 32,	\
						       signed long : 32, \
						       signed long long : 32, \
						       char *: 8,	\
						       const char *:  8, \
						       default: 0)

#define BTREE_EMPTY {{.root = NULL, .height = 0}}

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
	_Static_assert((max_items_per_node) <= USHRT_MAX, "cannot have more than USHRT_MAX items per node"); \
									\
	static _Alignas(32) const struct btree_info name##_info = {	\
		.max_items = (max_items_per_node),			\
		.min_items = (max_items_per_node) / 2,			\
		.item_size = sizeof(name##_key_t),			\
		.alignment_offset = (_Alignof(name##_key_t) - (sizeof(struct _btree_node) % _Alignof(name##_key_t))) % _Alignof(name##_key_t), \
		.linear_search_threshold = __BTREE_LINEAR_SEARCH_THRESHOLD(name##_key_t), \
		.cmp = _##name##_compare,				\
		.destroy_item = _##name##_destroy_item,			\
	};								\
									\
	typedef struct btree_iter name##_iter_t;			\
									\
	static _attr_unused const name##_key_t *name##_iter_start_leftmost(name##_iter_t *iter, \
									   const struct name *tree) \
	{								\
		return _btree_iter_start(iter, &tree->_impl, false, &name##_info); \
	}								\
									\
	static _attr_unused const name##_key_t *name##_iter_start_rightmost(name##_iter_t *iter, \
									    const struct name *tree) \
	{								\
		return _btree_iter_start(iter, &tree->_impl, true, &name##_info); \
	}								\
									\
	static _attr_unused const name##_key_t *name##_iter_start_at(name##_iter_t *iter, \
								     const struct name *tree, \
								     name##_key_t key, \
								     enum btree_iter_start_at_mode mode) \
	{								\
		return _btree_iter_start_at(iter, &tree->_impl, &key, mode, &name##_info); \
	}								\
									\
	static _attr_unused const name##_key_t *name##_iter_next(name##_iter_t *iter) \
	{								\
		return _btree_iter_next(iter, &name##_info);		\
	}								\
									\
	static _attr_unused const name##_key_t *name##_iter_prev(name##_iter_t *iter) \
	{								\
		return _btree_iter_prev(iter, &name##_info);		\
	}								\
									\
	static _attr_unused void name##_init(struct name *tree)		\
	{								\
		_btree_init(&tree->_impl);				\
	}								\
									\
	static _attr_unused void name##_destroy(struct name *tree)	\
	{								\
		_btree_destroy(&tree->_impl, &name##_info);		\
	}								\
									\
	static _attr_unused const name##_key_t *name##_find(const struct name *tree, name##_key_t key) \
	{								\
		return (const name##_key_t *)_btree_find(&tree->_impl, &key, &name##_info); \
	}								\
									\
	static _attr_unused const name##_key_t *name##_get_leftmost(const struct name *tree) \
	{								\
		return _btree_get_leftmost_rightmost(&tree->_impl, true, &name##_info); \
	}								\
									\
	static _attr_unused const name##_key_t *name##_get_rightmost(const struct name *tree) \
	{								\
		return _btree_get_leftmost_rightmost(&tree->_impl, false, &name##_info); \
	}								\
									\
	static _attr_unused bool name##_delete(struct name *tree, name##_key_t key, name##_key_t *ret_key) \
	{								\
		return _btree_delete(&tree->_impl, __BTREE_DELETE_KEY, &key, (void *)ret_key, &name##_info); \
	}								\
									\
	static _attr_unused bool name##_delete_min(struct name *tree, name##_key_t *ret_key) \
	{								\
		return _btree_delete(&tree->_impl, __BTREE_DELETE_MIN, NULL, (void *)ret_key, &name##_info); \
	}								\
									\
	static _attr_unused bool name##_delete_max(struct name *tree, name##_key_t *ret_key) \
	{								\
		return _btree_delete(&tree->_impl, __BTREE_DELETE_MAX, NULL, (void *)ret_key, &name##_info); \
	}								\
									\
	static _attr_unused bool name##_insert(struct name *tree, name##_key_t key) \
	{								\
		return _btree_insert(&tree->_impl, &key, false, &name##_info); \
	}								\
									\
	/* TODO does this even make sense for a btree set? */		\
	/* I guess you can use this to destruct and replace a key... */	\
	static _attr_unused bool name##_set(struct name *tree, name##_key_t key) \
	{								\
		return _btree_insert(&tree->_impl, &key, true, &name##_info); \
	}								\
									\
	static _attr_unused bool name##_insert_sequential(struct name *tree, name##_key_t key) \
	{								\
		return _btree_insert_sequential(&tree->_impl, &key, &name##_info); \
	}

#define __BTREE_MAP_RETURN_KEY_AND_VALUE	\
	if (!item) {				\
		return NULL;			\
	}					\
	if (ret_key) {				\
		*ret_key = item->key;		\
	}					\
	return &item->value

#define __BTREE_MAP_DELETE_RETURN			\
	if (found) {					\
		if (ret_key) {				\
			*ret_key = item.key;		\
		}					\
		if (ret_value) {			\
			*ret_value = item.value;	\
		}					\
	}						\
	return found

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
	_Static_assert((max_items_per_node) <= USHRT_MAX, "cannot have more than USHRT_MAX items per node"); \
									\
	static _Alignas(32) const struct btree_info name##_info = {	\
		.max_items = (max_items_per_node),			\
		.min_items = (max_items_per_node) / 2,			\
		.item_size = sizeof(_##name##_item_t),			\
		.alignment_offset = (_Alignof(_##name##_item_t) - (sizeof(struct _btree_node) % _Alignof(_##name##_item_t))) % _Alignof(_##name##_item_t), \
		.linear_search_threshold = __BTREE_LINEAR_SEARCH_THRESHOLD(name##_key_t), \
		.cmp = _##name##_compare,				\
		.destroy_item = _##name##_destroy_item,			\
	};								\
									\
	typedef struct btree_iter name##_iter_t;			\
									\
	static _attr_unused name##_value_t *name##_iter_start_leftmost(name##_iter_t *iter, \
								       const struct name *tree, \
								       name##_key_t *ret_key) \
	{								\
		_##name##_item_t *item = _btree_iter_start(iter, &tree->_impl, false, &name##_info); \
		__BTREE_MAP_RETURN_KEY_AND_VALUE;			\
	}								\
									\
	static _attr_unused name##_value_t *name##_iter_start_rightmost(name##_iter_t *iter, \
									const struct name *tree, \
									name##_key_t *ret_key) \
	{								\
		_##name##_item_t *item = _btree_iter_start(iter, &tree->_impl, true, &name##_info); \
		__BTREE_MAP_RETURN_KEY_AND_VALUE;			\
	}								\
									\
	static _attr_unused name##_value_t *name##_iter_start_at(name##_iter_t *iter, const struct name *tree, \
								 name##_key_t key, name##_key_t *ret_key, \
								 enum btree_iter_start_at_mode mode) \
	{								\
		_##name##_item_t *item = _btree_iter_start_at(iter, &tree->_impl, &key, mode, &name##_info); \
		__BTREE_MAP_RETURN_KEY_AND_VALUE;			\
	}								\
									\
	static _attr_unused name##_value_t *name##_iter_next(name##_iter_t *iter, name##_key_t *ret_key) \
	{								\
		_##name##_item_t *item = _btree_iter_next(iter, &name##_info); \
		__BTREE_MAP_RETURN_KEY_AND_VALUE;			\
	}								\
									\
	static _attr_unused name##_value_t *name##_iter_prev(name##_iter_t *iter, name##_key_t *ret_key) \
	{								\
		_##name##_item_t *item = _btree_iter_prev(iter, &name##_info); \
		__BTREE_MAP_RETURN_KEY_AND_VALUE;			\
	}								\
									\
	static _attr_unused void name##_init(struct name *tree)		\
	{								\
		_btree_init(&tree->_impl);				\
	}								\
									\
	static _attr_unused void name##_destroy(struct name *tree)	\
	{								\
		_btree_destroy(&tree->_impl, &name##_info);		\
	}								\
									\
	static _attr_unused name##_value_t *name##_find(const struct name *tree, name##_key_t key) \
	{								\
		_##name##_item_t *item = _btree_find(&tree->_impl, &key, &name##_info); \
		return item ? &item->value : NULL;			\
	}								\
									\
	static _attr_unused name##_value_t *name##_get_leftmost(const struct name *tree, name##_key_t *ret_key) \
	{								\
		_##name##_item_t *item = _btree_get_leftmost_rightmost(&tree->_impl, true, &name##_info); \
		__BTREE_MAP_RETURN_KEY_AND_VALUE;			\
	}								\
									\
	static _attr_unused name##_value_t *name##_get_rightmost(const struct name *tree, name##_key_t *ret_key) \
	{								\
		_##name##_item_t *item = _btree_get_leftmost_rightmost(&tree->_impl, false, &name##_info); \
		__BTREE_MAP_RETURN_KEY_AND_VALUE;			\
	}								\
									\
	static _attr_unused bool name##_delete(struct name *tree, name##_key_t key, name##_key_t *ret_key, \
					       name##_value_t *ret_value) \
	{								\
		_##name##_item_t item;					\
		bool found = _btree_delete(&tree->_impl, __BTREE_DELETE_KEY, &key, \
					   (ret_key || ret_value) ? &item : NULL, &name##_info); \
		__BTREE_MAP_DELETE_RETURN;				\
	}								\
									\
	static _attr_unused bool name##_delete_min(struct name *tree, name##_key_t *ret_key, \
						   name##_value_t *ret_value) \
	{								\
		_##name##_item_t item;					\
		bool found = _btree_delete(&tree->_impl, __BTREE_DELETE_MIN, NULL, \
					   (ret_key || ret_value) ? &item : NULL, &name##_info); \
		__BTREE_MAP_DELETE_RETURN;				\
	}								\
									\
	static _attr_unused bool name##_delete_max(struct name *tree, name##_key_t *ret_key, \
						   name##_value_t *ret_value) \
	{								\
		_##name##_item_t item;					\
		bool found = _btree_delete(&tree->_impl, __BTREE_DELETE_MAX, NULL, \
					   (ret_key || ret_value) ? &item : NULL, &name##_info); \
		__BTREE_MAP_DELETE_RETURN;				\
	}								\
									\
	static _attr_unused bool name##_insert(struct name *tree, name##_key_t key, name##_value_t value) \
	{								\
		return _btree_insert(&tree->_impl, &(_##name##_item_t){.key = key, .value = value}, false, \
				     &name##_info);			\
	}								\
									\
	static _attr_unused bool name##_set(struct name *tree, name##_key_t key, name##_value_t value)	\
	{								\
		return _btree_insert(&tree->_impl, &(_##name##_item_t){.key = key, .value = value}, true, \
				     &name##_info);			\
	}								\
									\
	static _attr_unused bool name##_insert_sequential(struct name *tree, name##_key_t key, \
							  name##_value_t value) \
	{								\
		return _btree_insert_sequential(&tree->_impl, &(_##name##_item_t){.key = key, .value = value}, \
						&name##_info);		\
	}

void *_btree_iter_start(struct btree_iter *iter, const struct _btree *tree, bool rightmost,
			const struct btree_info *info);
void *_btree_iter_next(struct btree_iter *iter, const struct btree_info *info);
void *_btree_iter_prev(struct btree_iter *iter, const struct btree_info *info);
void *_btree_iter_start_at(struct btree_iter *iter, const struct _btree *tree, void *key,
			   enum btree_iter_start_at_mode mode, const struct btree_info *info);
void _btree_init(struct _btree *tree);
void _btree_destroy(struct _btree *tree, const struct btree_info *info);
void *_btree_find(const struct _btree *tree, const void *key, const struct btree_info *info);
void *_btree_get_leftmost_rightmost(const struct _btree *tree, bool leftmost, const struct btree_info *info) _attr_pure;
bool _btree_delete(struct _btree *tree, enum _btree_deletion_mode mode, const void *key, void *ret_item,
		   const struct btree_info *info);
bool _btree_insert(struct _btree *tree, void *item, bool update, const struct btree_info *info);
bool _btree_insert_sequential(struct _btree *tree, void *item, const struct btree_info *info);

// TODO should these be public API?
void *_btree_debug_node_item(struct _btree_node *node, unsigned int idx, const struct btree_info *info);
struct _btree_node *_btree_debug_node_get_child(struct _btree_node *node, unsigned int idx,
						const struct btree_info *info);
struct _btree _btree_debug_copy(const struct _btree *tree, const struct btree_info *info);
