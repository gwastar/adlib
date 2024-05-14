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

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include "config.h"
#include "compiler.h"

// #define __HASHTABLE_PROFILING

// TODO documentation (see tests for now)
// TODO split off type and function declarations for headers

#define DEFINE_HASHTABLE(name, key_type, entry_type, THRESHOLD, ...)	\
									\
	struct name {							\
		struct _hashtable impl;					\
	};								\
									\
	static bool _##name##_keys_match(const void *_key, const void *_entry) \
	{								\
		key_type const * const key = _key;			\
		entry_type const * const entry = _entry;		\
		return (__VA_ARGS__);					\
	}								\
									\
	_Static_assert(5 <= (THRESHOLD) && (THRESHOLD) <= 9,		\
		       "resize threshold (max load factor) must be an integer in the range of 5 to 9 (50%-90%)"); \
									\
	typedef _hashtable_hash_t name##_hash_t;			\
	typedef _hashtable_uint_t name##_uint_t;			\
									\
	static _Alignas(16) const struct _hashtable_info _##name##_info = { \
		.entry_size = sizeof(entry_type),			\
		.threshold = (THRESHOLD),				\
		.keys_match = _##name##_keys_match,			\
	};								\
									\
	static void name##_init(struct name *table, name##_uint_t initial_capacity) \
	{								\
		_hashtable_init(&table->impl, initial_capacity, &_##name##_info); \
	}								\
									\
	static _attr_unused void name##_destroy(struct name *table)	\
	{								\
		_hashtable_destroy(&table->impl);			\
	}								\
									\
	static _attr_unused void name##_clear(struct name *table)	\
	{								\
		_hashtable_clear(&table->impl, &_##name##_info);	\
	}								\
									\
	static _attr_unused void name##_resize(struct name *table, name##_uint_t new_capacity) \
	{								\
		_hashtable_resize(&table->impl, new_capacity, &_##name##_info); \
	}								\
									\
	static _attr_unused name##_uint_t name##_capacity(struct name *table) \
	{								\
		return table->impl.capacity;				\
	}								\
									\
	static _attr_unused name##_uint_t name##_num_entries(struct name *table) \
	{								\
		return table->impl.num_entries;				\
	}								\
									\
	typedef struct name##_iterator {				\
		entry_type *entry;					\
		_hashtable_idx_t _index;				\
		struct name *_table;					\
		bool _finished;						\
	} name##_iter_t;						\
									\
	static _attr_unused bool name##_iter_finished(struct name##_iterator *iter) \
	{								\
		return iter->_finished;					\
	}								\
									\
	static _attr_unused void name##_iter_advance(struct name##_iterator *iter) \
	{								\
		iter->entry = NULL;					\
		if (iter->_finished) {					\
			return;						\
		}							\
		iter->_index = _hashtable_get_next(&iter->_table->impl, iter->_index + 1, &_##name##_info); \
		if (iter->_index >= iter->_table->impl.capacity) {	\
			iter->_finished = true;				\
		} else {						\
			iter->entry = _hashtable_entry(&iter->_table->impl, iter->_index, &_##name##_info); \
		}							\
	}								\
									\
	static _attr_unused struct name##_iterator name##_iter_start(struct name *table) \
	{								\
		struct name##_iterator iter = {0};			\
		iter._table = table;					\
		iter._index = (_hashtable_idx_t)-1; /* iter_advance increments this to 0 */ \
		iter._finished = false;					\
		name##_iter_advance(&iter);				\
		return iter;						\
	}								\
									\
	static _attr_unused entry_type *name##_lookup(struct name *table, key_type key, name##_hash_t hash) \
	{								\
		_hashtable_idx_t index;					\
		if (!_hashtable_lookup(&table->impl, &key, hash, &index, &_##name##_info)) { \
			return NULL;					\
		}							\
		return _hashtable_entry(&table->impl, index, &_##name##_info); \
	}								\
									\
	static _attr_unused entry_type *name##_insert(struct name *table, key_type key, name##_hash_t hash) \
	{								\
		(void)key;						\
		_hashtable_idx_t index = _hashtable_insert(&table->impl, hash, &_##name##_info); \
		return _hashtable_entry(&table->impl, index, &_##name##_info); \
	}								\
									\
	static _attr_unused bool name##_remove(struct name *table, key_type key, name##_hash_t hash, entry_type *ret_entry) \
	{								\
		_hashtable_idx_t index;					\
		if (!_hashtable_lookup(&table->impl, &key, hash, &index, &_##name##_info)) { \
			return false;					\
		}							\
									\
		if (ret_entry) {					\
			*ret_entry = *(entry_type *)_hashtable_entry(&table->impl, index, &_##name##_info); \
		}							\
		_hashtable_remove(&table->impl, index, &_##name##_info); \
		return true;						\
	}								\


// private API

typedef uint32_t _hashtable_hash_t;
typedef uint32_t _hashtable_uint_t;
typedef _hashtable_uint_t _hashtable_idx_t;

struct _hashtable_info {
	_hashtable_uint_t entry_size;
	_hashtable_uint_t threshold;
	bool (*keys_match)(const void *key, const void *entry);
};

struct _hashtable {
	_hashtable_uint_t num_entries;
#ifdef HASHTABLE_QUADRATIC
	_hashtable_uint_t num_tombstones;
#endif
	_hashtable_uint_t max_entries;
	_hashtable_uint_t capacity;
	unsigned char *storage;
	struct _hashtable_metadata *metadata;
};

void _hashtable_init(struct _hashtable *table, _hashtable_uint_t capacity, const struct _hashtable_info *info);
void _hashtable_destroy(struct _hashtable *table);
bool _hashtable_lookup(struct _hashtable *table, void *key, _hashtable_hash_t hash, _hashtable_idx_t *ret_index,
		       const struct _hashtable_info *info) _attr_nodiscard;
_hashtable_idx_t _hashtable_get_next(struct _hashtable *table, _hashtable_idx_t start,
				     const struct _hashtable_info *info) _attr_pure;
void _hashtable_resize(struct _hashtable *table, _hashtable_uint_t new_capacity,
		       const struct _hashtable_info *info);
_hashtable_idx_t _hashtable_insert(struct _hashtable *table, _hashtable_hash_t hash,
				   const struct _hashtable_info *info) _attr_nodiscard;
void _hashtable_remove(struct _hashtable *table, _hashtable_idx_t index, const struct _hashtable_info *info);
void _hashtable_clear(struct _hashtable *table, const struct _hashtable_info *info);

static inline void *_hashtable_entry(struct _hashtable *table, _hashtable_idx_t index,
				     const struct _hashtable_info *info)
{
	return table->storage + index * info->entry_size;
}
