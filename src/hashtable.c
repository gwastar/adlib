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

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "hashtable.h"
#include "macros.h"

// TODO write a new implementation from scratch with these ideas
// TODO try using a hash function instead of storing the hash values (evaluate performance with and without user-cached hashes) (update: this requires a very different API...)
// TODO make this hashtable ordered (insertion order) (see python dict) (how to implement in-place resize efficiently?)
// TODO add generation and check it during iteration?
// TODO make it possible to choose the implementation for each instance? (probably too slow or messy...)
// TODO make _hashtable_lookup return the first eligable slot for insertion to speed up subsequent insertion
//      and make hashtable_set (lookup + insert) helper
// TODO try a bucket-based API instead (find_bucket, lookup_bucket_for_insertion, bucket_delete_entry, bucket_update_entry, ...)

// TODO make a linked hashtable with inline links like:
// struct hash_node {
//     struct rb_node rb_node;
//     struct list_head list_head;
// };

/* Memory layout:
 * For in-place resizing the memory layout needs to look like this (e=entry, m=metadata):
 * eeeeemmmmm
 * So that when we double the capacity we get this:
 * eeeeemmmmm
 * eeeeeeeeeemmmmmmmmmm
 * Notice how the old entries remain in the same place!
 * (The old metadata gets copied to the back early on, so it's fine to overwrite it.
 *  But we don't want to copy any entries since those tend to be bigger.)
 */

#ifdef __HASHTABLE_PROFILING
size_t lookup_found_search_length;
size_t lookup_notfound_search_length;
size_t num_lookups_found;
size_t num_lookups_notfound;
size_t collisions1;
size_t collisions2;
size_t num_inserts;
#endif

// TODO put this into utils
static _hashtable_uint_t _hashtable_round_capacity(_hashtable_uint_t capacity)
{
	// round to next power of 2
	capacity--;
	capacity |= capacity >> 1;
	capacity |= capacity >> 2;
	capacity |= capacity >> 4;
	capacity |= capacity >> 8;
	capacity |= capacity >> 16;
	capacity |= capacity >> (sizeof(capacity) * 4);
	capacity++;
	return capacity;
}

// static void check(const unsigned int t)
// {
// 	for (unsigned int c = 8; c != 0; c++) {
// 		unsigned int n = (c / 10) * t + (c % 10) * t / 10;
// 		unsigned int m = (unsigned int)((double)c * (0.1 * t));
// 		unsigned int c2 = (n / t) * 10 + ((n % t) * 10 + t - 1) / t;
// 		unsigned int d = ceil(10.0 / t * n);
// 		unsigned int n2 = (c2 / 10) * t + (c2 % 10) * t / 10;
// 		// printf("%u %u %u\n", c, c2, x); if (c == 100) break;
// 		assert(n == m);
// 		assert(c2 == d);
// 		assert(n == n2);
// 	}
// }

// static _hashtable_uint_t _hashtable_min_capacity(_hashtable_uint_t num_entries,
// 							      const struct _hashtable_info *info)
// {
// 	return (num_entries / info->threshold) * 10 +
// 		((num_entries % info->threshold) * 10 + info->threshold - 1) / info->threshold;
// }

static _hashtable_uint_t _hashtable_max_entries(_hashtable_uint_t capacity,
						const struct _hashtable_info *info)
{
	return (capacity / 10) * info->threshold + (capacity % 10) * info->threshold / 10;
}

static _hashtable_idx_t _hashtable_hash_to_index(const struct _hashtable *table,
						 _hashtable_hash_t hash)
{
	_hashtable_idx_t h = hash;

#if defined(HASHTABLE_ROBINHOOD)
	// for quadratic hashing this helps with bad hash functions but hurts performance
	// for integer keys with identity hash
	return (11 * h) & (table->capacity - 1);
#elif defined(HASHTABLE_QUADRATIC) || defined(HASHTABLE_HOPSCOTCH)
	// this is really bad for bad hash functions
	return h & (table->capacity - 1);
#elif 0
	// for quadratic hashing this helps a lot with bad hash functions

	const size_t shift_amount = __builtin_clzll(table->capacity) + 1;
	// const size_t shift_amount = table->hash_to_index_shift;
	h ^= h >> shift_amount;
	h *= sizeof(h) == 8 ? 11400714819323198485llu : 2654435769;
	// h *= sizeof(h) == 8 ? 7046029254386353131llu : 1640531527;
	return h >> shift_amount;
#endif
}

#if defined(HASHTABLE_QUADRATIC)

#define __HASHTABLE_EMPTY_HASH 0
#define __HASHTABLE_TOMBSTONE_HASH 1
#define __HASHTABLE_MIN_VALID_HASH 2

typedef struct _hashtable_metadata {
	_hashtable_hash_t hash;
} _hashtable_metadata_t;

struct _hashtable_probe_iter {
	_hashtable_idx_t index;
	// _hashtable_idx_t start;
	_hashtable_uint_t increment;
	_hashtable_uint_t mask;
};

static struct _hashtable_probe_iter _hashtable_probe_iter_start(const struct _hashtable *table,
								_hashtable_hash_t hash)
{
	_hashtable_idx_t start = _hashtable_hash_to_index(table, hash);
	struct _hashtable_probe_iter iter = {
		.index = start,
		// .start = start,
		.increment = 0,
		.mask = table->capacity - 1,
	};
	return iter;
}

static void _hashtable_probe_iter_advance(struct _hashtable_probe_iter *iter)
{
	// http://www.chilton-computing.org.uk/acl/literature/reports/p012.htm
	iter->increment++;
	iter->index = (iter->index + iter->increment) & iter->mask;
	// iter->index = (iter->start + ((iter->increment + 1) * iter->increment) / 2) & iter->mask;
}

static _hashtable_uint_t _hashtable_metadata_offset(_hashtable_uint_t capacity,
						    const struct _hashtable_info *info)
{
	return capacity * info->entry_size;
}

static _hashtable_metadata_t *_hashtable_metadata(struct _hashtable *table, _hashtable_idx_t index,
						  const struct _hashtable_info *info)
{
	(void)info;
	return &table->metadata[index];
}

static void _hashtable_realloc_storage(struct _hashtable *table, const struct _hashtable_info *info)
{
	assert((table->capacity & (table->capacity - 1)) == 0);
	_hashtable_uint_t size = info->entry_size + sizeof(_hashtable_metadata_t);
	assert(((_hashtable_uint_t)-1) / size >= table->capacity);
	size *= table->capacity;
	table->storage = realloc(table->storage, size);
	if (unlikely(!table->storage && table->capacity != 0)) {
		abort();
	}
	table->metadata = (_hashtable_metadata_t *)(table->storage +
						    _hashtable_metadata_offset(table->capacity, info));
	table->max_entries = _hashtable_max_entries(table->capacity, info);
}

void _hashtable_init(struct _hashtable *table, _hashtable_uint_t capacity,
		     const struct _hashtable_info *info)
{
	if (capacity < 8) {
		capacity = 8;
	}
	capacity = _hashtable_round_capacity(capacity);
	table->storage = NULL;
	table->capacity = capacity;
	table->num_entries = 0;
	table->num_tombstones = 0;
	_hashtable_realloc_storage(table, info);
	for (_hashtable_uint_t i = 0; i < capacity; i++) {
		_hashtable_metadata_t *m = _hashtable_metadata(table, i, info);
		m->hash = __HASHTABLE_EMPTY_HASH;
	}
}

void _hashtable_destroy(struct _hashtable *table)
{
	free(table->storage);
	memset(table, 0, sizeof(*table));
}

static _hashtable_hash_t _hashtable_sanitize_hash(_hashtable_hash_t hash)
{
	_hashtable_metadata_t m;
	m.hash = hash < __HASHTABLE_MIN_VALID_HASH ? hash - __HASHTABLE_MIN_VALID_HASH : hash;
	return m.hash;
}

bool _hashtable_lookup(struct _hashtable *table, void *key, _hashtable_hash_t hash,
		       _hashtable_idx_t *ret_index, const struct _hashtable_info *info)
{
#ifdef __HASHTABLE_PROFILING
	_hashtable_uint_t search_length = 0;
#endif
	hash = _hashtable_sanitize_hash(hash);
	for (struct _hashtable_probe_iter iter = _hashtable_probe_iter_start(table, hash);;
	     _hashtable_probe_iter_advance(&iter)) {
#ifdef __HASHTABLE_PROFILING
		search_length++;
#endif
		_hashtable_idx_t index = iter.index;
		_hashtable_metadata_t *m = _hashtable_metadata(table, index, info);
		if (m->hash == __HASHTABLE_EMPTY_HASH) {
			break;
		}
		if (hash == m->hash && info->keys_match(key, _hashtable_entry(table, index, info))) {
			*ret_index = index;
#ifdef __HASHTABLE_PROFILING
			num_lookups_found++;
			lookup_found_search_length += search_length;
#endif
			return true;
		}
	}
#ifdef __HASHTABLE_PROFILING
	num_lookups_notfound++;
	lookup_notfound_search_length += search_length;
#endif
	return false;
}

_hashtable_idx_t _hashtable_get_next(struct _hashtable *table, _hashtable_idx_t start,
				     const struct _hashtable_info *info)
{
	for (_hashtable_idx_t index = start; index < table->capacity; index++) {
		_hashtable_metadata_t *m = _hashtable_metadata(table, index, info);
		if (m->hash >= __HASHTABLE_MIN_VALID_HASH) {
			return index;
		}
	}
	return table->capacity;
}

static _hashtable_idx_t _hashtable_do_insert(struct _hashtable *table, _hashtable_hash_t hash,
					     const struct _hashtable_info *info)
{
#ifdef __HASHTABLE_PROFILING
	num_inserts++;
#endif
	_hashtable_metadata_t *m;
	_hashtable_idx_t index;
	for (struct _hashtable_probe_iter iter = _hashtable_probe_iter_start(table, hash);;
	     _hashtable_probe_iter_advance(&iter)) {
		index = iter.index;
		m = _hashtable_metadata(table, index, info);
		if (m->hash < __HASHTABLE_MIN_VALID_HASH) {
			if (m->hash == __HASHTABLE_TOMBSTONE_HASH) {
				table->num_tombstones--;
			}
			break;
		}
#ifdef __HASHTABLE_PROFILING
		if (_hashtable_hash_to_index(table, hash) == _hashtable_hash_to_index(table, m->hash)) {
			collisions2++;
		} else {
			collisions1++;
		}
#endif
	}
	m->hash = hash;
	return index;
}

static bool _hashtable_slot_needs_rehash(uint32_t *bitmap, _hashtable_idx_t index)
{
	return !(bitmap[index / 32] & (1u << (index % 32)));
}

static void _hashtable_slot_clear_needs_rehash(uint32_t *bitmap, _hashtable_idx_t index)
{
	bitmap[index / 32] |= 1u << (index % 32);
}

static bool _hashtable_insert_during_resize(struct _hashtable *table, _hashtable_hash_t *phash,
					    void *entry, uint32_t *bitmap,
					    const struct _hashtable_info *info)
{
	for (struct _hashtable_probe_iter iter = _hashtable_probe_iter_start(table, *phash);;
	     _hashtable_probe_iter_advance(&iter)) {
		_hashtable_idx_t index = iter.index;
		_hashtable_metadata_t *m = _hashtable_metadata(table, index, info);
		if (m->hash < __HASHTABLE_MIN_VALID_HASH) {
			_hashtable_slot_clear_needs_rehash(bitmap, index);
			m->hash = *phash;
			memcpy(_hashtable_entry(table, index, info), entry, info->entry_size);
			return false;
		}

		if (_hashtable_slot_needs_rehash(bitmap, index)) {
			_hashtable_slot_clear_needs_rehash(bitmap, index);
			// if (_hashtable_hash_to_index(table, m->hash) == index) {
			// 	continue;
			// }
			void *tmp_entry = alloca(info->entry_size);
			_hashtable_hash_t tmp_hash = m->hash;
			memcpy(tmp_entry, _hashtable_entry(table, index, info), info->entry_size);

			m->hash = *phash;
			memcpy(_hashtable_entry(table, index, info), entry, info->entry_size);

			*phash = tmp_hash;
			memcpy(entry, tmp_entry, info->entry_size);

			return true;
		}
	}
}

static void _hashtable_resize_common(struct _hashtable *table, _hashtable_uint_t old_capacity,
				     const struct _hashtable_info *info)
{
	size_t max_capacity = old_capacity > table->capacity ? old_capacity : table->capacity;
	size_t bitmap_size = (max_capacity + 31) / 32 * sizeof(uint32_t);
	uint32_t *bitmap, *bitmap_to_free = NULL;
	if (bitmap_size <= 1024) {
		bitmap = alloca(bitmap_size);
		memset(bitmap, 0, bitmap_size);
	} else {
		bitmap = calloc(1, bitmap_size);
		if (unlikely(!bitmap)) {
			abort();
		}
		bitmap_to_free = bitmap;
	}

	void *entry = alloca(info->entry_size);
#ifdef __HASHTABLE_PROFILING
	_hashtable_uint_t num_rehashed = 0;
#endif
	for (_hashtable_idx_t index = 0; index < old_capacity; index++) {
		_hashtable_metadata_t *m = _hashtable_metadata(table, index, info);
		if (m->hash < __HASHTABLE_MIN_VALID_HASH) {
			m->hash = __HASHTABLE_EMPTY_HASH;
			continue;
		}
		if (!_hashtable_slot_needs_rehash(bitmap, index)) {
			continue;
		}
		_hashtable_hash_t hash = m->hash;
		_hashtable_idx_t optimal_index = _hashtable_hash_to_index(table, hash);
		if (optimal_index == index) {
			_hashtable_slot_clear_needs_rehash(bitmap, index);
#ifdef __HASHTABLE_PROFILING
			num_rehashed++;
#endif
			continue;
		}
		m->hash = __HASHTABLE_EMPTY_HASH;
		memcpy(entry, _hashtable_entry(table, index, info), info->entry_size);

		bool need_rehash;
		do {
			need_rehash = _hashtable_insert_during_resize(table, &hash, entry, bitmap, info);
#ifdef __HASHTABLE_PROFILING
			num_rehashed++;
#endif
		} while (need_rehash);
	}

	if (bitmap_to_free) {
		free(bitmap_to_free);
	}
}

static void _hashtable_shrink(struct _hashtable *table, _hashtable_uint_t new_capacity,
			      const struct _hashtable_info *info)
{
	assert(new_capacity < table->capacity && new_capacity > table->num_entries);
	if (new_capacity < 8) {
		new_capacity = 8;
	}
	_hashtable_uint_t old_capacity = table->capacity;
	table->capacity = new_capacity;
	table->num_tombstones = 0;

	_hashtable_resize_common(table, old_capacity, info);

	size_t new_metadata_offset = _hashtable_metadata_offset(table->capacity, info);
	_hashtable_metadata_t *new_metadata = (_hashtable_metadata_t *)(table->storage + new_metadata_offset);
	memmove(new_metadata, table->metadata, old_capacity * sizeof(table->metadata[0]));
	_hashtable_realloc_storage(table, info);
}

static void _hashtable_grow(struct _hashtable *table, _hashtable_uint_t new_capacity,
			    const struct _hashtable_info *info)
{
	assert(new_capacity >= table->capacity && new_capacity > table->num_entries);

	_hashtable_uint_t old_capacity = table->capacity;
	table->capacity = new_capacity;
	table->num_tombstones = 0;
	_hashtable_realloc_storage(table, info);
	size_t old_metadata_offset = _hashtable_metadata_offset(old_capacity, info);
	_hashtable_metadata_t *old_metadata = (_hashtable_metadata_t *)(table->storage + old_metadata_offset);

	/* Need to use memmove in case the metadata is bigger than the entries:
	 * eeeeemmmmmmmmmm
	 * eeeeeeeeeemmmmmmmmmmmmmmmmmmmm
	 */
	memmove(table->metadata, old_metadata, old_capacity * sizeof(table->metadata[0]));
	for (_hashtable_uint_t i = old_capacity; i < table->capacity; i++) {
		_hashtable_metadata(table, i, info)->hash = __HASHTABLE_EMPTY_HASH;
	}

	_hashtable_resize_common(table, old_capacity, info);
}

void _hashtable_resize(struct _hashtable *table, _hashtable_uint_t new_capacity,
		       const struct _hashtable_info *info)
{
	new_capacity = _hashtable_round_capacity(new_capacity);
	while (_hashtable_max_entries(new_capacity, info) < table->num_entries) {
		new_capacity *= 2;
	}
	if (new_capacity < table->capacity) {
		_hashtable_shrink(table, new_capacity, info);
	} else {
		_hashtable_grow(table, new_capacity, info);
	}
}

_hashtable_idx_t _hashtable_insert(struct _hashtable *table, _hashtable_hash_t hash,
				   const struct _hashtable_info *info)
{
	hash = _hashtable_sanitize_hash(hash);
	table->num_entries++;
	if ((table->num_entries + table->num_tombstones) > table->max_entries) {
		_hashtable_grow(table, 2 * table->capacity, info);
	}
	return _hashtable_do_insert(table, hash, info);
}

void _hashtable_remove(struct _hashtable *table, _hashtable_idx_t index,
		       const struct _hashtable_info *info)
{
	_hashtable_metadata(table, index, info)->hash = __HASHTABLE_TOMBSTONE_HASH;
	table->num_entries--;
	table->num_tombstones++;
	if (table->num_entries < table->capacity / 8) {
		_hashtable_shrink(table, table->capacity / 4, info);
	} else if (table->num_tombstones > table->capacity / 2) {
		_hashtable_grow(table, table->capacity, info);
	}
}

void _hashtable_clear(struct _hashtable *table, const struct _hashtable_info *info)
{
	for (_hashtable_uint_t i = 0; i < table->capacity; i++) {
		_hashtable_metadata_t *m = _hashtable_metadata(table, i, info);
		m->hash = __HASHTABLE_EMPTY_HASH;
	}
	table->num_entries = 0;
	table->num_tombstones = 0;
}

#undef __HASHTABLE_EMPTY_HASH
#undef __HASHTABLE_TOMBSTONE_HASH
#undef __HASHTABLE_MIN_VALID_HASH

#elif defined(HASHTABLE_HOPSCOTCH)

#define __HASHTABLE_EMPTY_HASH 0
#define __HASHTABLE_MIN_VALID_HASH 1

// setting this too small (<~8) may cause issues with shrink failing in remove among other problems
#define __HASHTABLE_NEIGHBORHOOD 32

typedef _hashtable_hash_t _hashtable_bitmap_t;

_Static_assert(8 * sizeof(_hashtable_bitmap_t) >= __HASHTABLE_NEIGHBORHOOD,
	       "hopscotch neighborhood size too big for bitmap");

typedef struct _hashtable_metadata {
	_hashtable_hash_t hash;
	_hashtable_bitmap_t bitmap;
} _hashtable_metadata_t;

static _hashtable_idx_t _hashtable_wrap_index(_hashtable_idx_t index, _hashtable_uint_t capacity)
{
	return index & (capacity - 1);
}

static _hashtable_uint_t _hashtable_metadata_offset(_hashtable_uint_t capacity,
						    const struct _hashtable_info *info)
{
	return capacity * info->entry_size;
}

static _hashtable_metadata_t *_hashtable_metadata(struct _hashtable *table, _hashtable_idx_t index,
						  const struct _hashtable_info *info)
{
	return &table->metadata[index];
}

static void _hashtable_realloc_storage(struct _hashtable *table, const struct _hashtable_info *info)
{
	assert((table->capacity & (table->capacity - 1)) == 0);
	_hashtable_uint_t size = info->entry_size + sizeof(_hashtable_metadata_t);
	assert(((_hashtable_uint_t)-1) / size >= table->capacity);
	size *= table->capacity;
	table->storage = realloc(table->storage, size);
	if (unlikely(!table->storage && table->capacity != 0)) {
		abort();
	}
	table->metadata = (_hashtable_metadata_t *)(table->storage +
						    _hashtable_metadata_offset(table->capacity, info));
	table->max_entries = _hashtable_max_entries(table->capacity, info);
}

void _hashtable_init(struct _hashtable *table, _hashtable_uint_t capacity,
		     const struct _hashtable_info *info)
{
	if (capacity < 8) {
		capacity = 8;
	}
	capacity = _hashtable_round_capacity(capacity);
	table->storage = NULL;
	table->capacity = capacity;
	table->num_entries = 0;
	_hashtable_realloc_storage(table, info);
	for (_hashtable_uint_t i = 0; i < capacity; i++) {
		_hashtable_metadata_t *m = _hashtable_metadata(table, i, info);
		m->hash = __HASHTABLE_EMPTY_HASH;
		m->bitmap = 0;
	}
}

void _hashtable_destroy(struct _hashtable *table)
{
	free(table->storage);
	memset(table, 0, sizeof(*table));
}

static _hashtable_hash_t _hashtable_sanitize_hash(_hashtable_hash_t hash)
{
	hash = hash < __HASHTABLE_MIN_VALID_HASH ? hash - __HASHTABLE_MIN_VALID_HASH : hash;
	_hashtable_metadata_t m;
	m.hash = hash;
	return m.hash;
}

bool _hashtable_lookup(struct _hashtable *table, void *key, _hashtable_hash_t hash,
		       _hashtable_idx_t *ret_index, const struct _hashtable_info *info)
{
	hash = _hashtable_sanitize_hash(hash);
	_hashtable_idx_t home = _hashtable_hash_to_index(table, hash);
	_hashtable_bitmap_t bitmap = _hashtable_metadata(table, home, info)->bitmap;
	if (bitmap == 0) {
		return false;
	}
	for (_hashtable_uint_t i = 0; i < __HASHTABLE_NEIGHBORHOOD; i++) {
		if (!(bitmap & ((_hashtable_bitmap_t)1 << i))) {
			continue;
		}
		_hashtable_idx_t index = _hashtable_wrap_index(home + i, table->capacity);
		_hashtable_metadata_t *m = _hashtable_metadata(table, index, info);
		if (hash == m->hash &&
		    info->keys_match(key, _hashtable_entry(table, index, info))) {
			*ret_index = index;
			return true;
		}
	}
	return false;
}

_hashtable_idx_t _hashtable_get_next(struct _hashtable *table, _hashtable_idx_t start,
				     const struct _hashtable_info *info)
{
	for (_hashtable_idx_t index = start; index < table->capacity; index++) {
		_hashtable_metadata_t *m = _hashtable_metadata(table, index, info);
		if (m->hash >= __HASHTABLE_MIN_VALID_HASH) {
			return index;
		}
	}
	return table->capacity;
}

static bool _hashtable_move_into_neighborhood(struct _hashtable *table, _hashtable_idx_t *pindex,
					      _hashtable_uint_t *pdistance, const struct _hashtable_info *info)
{
	_hashtable_idx_t index = *pindex;
	_hashtable_uint_t distance = *pdistance;
	while (distance >= __HASHTABLE_NEIGHBORHOOD) {
		_hashtable_idx_t empty_index = index;
		for (_hashtable_uint_t i = 1;; i++) {
			if (i == __HASHTABLE_NEIGHBORHOOD) {
				return false;
			}
			index = _hashtable_wrap_index(index - 1, table->capacity);
			distance--;
			_hashtable_metadata_t *m = _hashtable_metadata(table, index, info);
			_hashtable_hash_t hash = m->hash;
			_hashtable_idx_t home = _hashtable_hash_to_index(table, hash);
			_hashtable_uint_t old_distance = _hashtable_wrap_index(index - home, table->capacity);
			_hashtable_uint_t new_distance = old_distance + i;
			if (new_distance >= __HASHTABLE_NEIGHBORHOOD) {
				continue;
			}
			m->hash = __HASHTABLE_EMPTY_HASH;
			_hashtable_bitmap_t *bitmap = &_hashtable_metadata(table, home, info)->bitmap;
			*bitmap &= ~((_hashtable_bitmap_t)1 << old_distance);
			*bitmap |= (_hashtable_bitmap_t)1 << new_distance;
			memcpy(_hashtable_entry(table, empty_index, info),
			       _hashtable_entry(table, index, info), info->entry_size);
			_hashtable_metadata(table, empty_index, info)->hash = hash;
			*pindex = index;
			*pdistance = distance;
			break;
		}
	}
	return true;
}

static bool _hashtable_do_insert(struct _hashtable *table, _hashtable_hash_t hash,
				 _hashtable_idx_t *pindex, const struct _hashtable_info *info)
{
	_hashtable_idx_t home = _hashtable_hash_to_index(table, hash);
	_hashtable_idx_t index = home;
	_hashtable_uint_t distance;
	for (distance = 0;; distance++) {
		index = _hashtable_wrap_index(home + distance, table->capacity);
		_hashtable_metadata_t *m = _hashtable_metadata(table, index, info);
		if (m->hash == __HASHTABLE_EMPTY_HASH) {
			break;
		}
	}

	if (distance >= __HASHTABLE_NEIGHBORHOOD &&
	    !_hashtable_move_into_neighborhood(table, &index, &distance, info)) {
		return false;
	}

	_hashtable_bitmap_t *bitmap = &_hashtable_metadata(table, home, info)->bitmap;
	*bitmap |= (_hashtable_bitmap_t)1 << distance;
	_hashtable_metadata(table, index, info)->hash = hash;
	*pindex = index;
	return true;
}

static bool _hashtable_slot_needs_rehash(uint32_t *bitmap, _hashtable_idx_t index)
{
	return !(bitmap[index / 32] & (1u << (index % 32)));
}

static void _hashtable_slot_clear_needs_rehash(uint32_t *bitmap, _hashtable_idx_t index)
{
	bitmap[index / 32] |= 1u << (index % 32);
}

// if an entry was replaced *phash will be != __HASHTABLE_EMPTY_HASH
static bool _hashtable_insert_during_resize(struct _hashtable *table, _hashtable_hash_t *phash,
					    void *entry, uint32_t *bitmap,
					    const struct _hashtable_info *info)
{
	_hashtable_hash_t hash = *phash;
	*phash = __HASHTABLE_EMPTY_HASH;
	_hashtable_idx_t home = _hashtable_hash_to_index(table, hash);
	_hashtable_idx_t index = home;
	_hashtable_uint_t distance;
	for (distance = 0;; distance++) {
		index = _hashtable_wrap_index(home + distance, table->capacity);
		_hashtable_metadata_t *m = _hashtable_metadata(table, index, info);
		if (m->hash == __HASHTABLE_EMPTY_HASH) {
			_hashtable_slot_clear_needs_rehash(bitmap, index);
			break;
		}
		if (_hashtable_slot_needs_rehash(bitmap, index)) {
			_hashtable_slot_clear_needs_rehash(bitmap, index);
			void *tmp_entry = alloca(info->entry_size);
			memcpy(tmp_entry, entry, info->entry_size);
			*phash = m->hash;
			memcpy(entry, _hashtable_entry(table, index, info), info->entry_size);
			entry = tmp_entry;
			m->hash = __HASHTABLE_EMPTY_HASH;
			break;
		}
	}

	bool success = true;
	if (distance >= __HASHTABLE_NEIGHBORHOOD &&
	    !_hashtable_move_into_neighborhood(table, &index, &distance, info)) {
		success = false;
		// if we didn't succeed, we insert the entry anyway so it doesn't get lost
	}

	_hashtable_metadata(table, home, info)->bitmap |= (_hashtable_bitmap_t)1 << distance;
	_hashtable_metadata(table, index, info)->hash = hash;
	memcpy(_hashtable_entry(table, index, info), entry, info->entry_size);
	return success;
}

static void _hashtable_shrink(struct _hashtable *table, _hashtable_uint_t new_capacity,
			      const struct _hashtable_info *info)
{
	assert(new_capacity < table->capacity && new_capacity > table->num_entries);
	if (new_capacity < 8) {
		new_capacity = 8;
	}

	_hashtable_uint_t old_capacity = table->capacity;
	size_t bitmap_size = (old_capacity + 31) / 32 * sizeof(uint32_t);
	uint32_t *bitmap, *bitmap_to_free = NULL;
	if (bitmap_size <= 1024) {
		bitmap = alloca(bitmap_size);
		memset(bitmap, 0, bitmap_size);
	} else {
		bitmap = calloc(1, bitmap_size);
		if (unlikely(!bitmap)) {
			abort();
		}
		bitmap_to_free = bitmap;
	}

retry:
	table->capacity = new_capacity;
	// TODO is there a (simple) way to avoid this work?
	for (_hashtable_uint_t i = 0; i < old_capacity; i++) {
		_hashtable_metadata_t *m = _hashtable_metadata(table, i, info);
		m->bitmap = 0;
	}

	void *entry = alloca(info->entry_size);
	_hashtable_uint_t num_rehashed = 0;
	for (_hashtable_idx_t index = 0; index < old_capacity; index++) {
		_hashtable_metadata_t *m = _hashtable_metadata(table, index, info);
		if (m->hash < __HASHTABLE_MIN_VALID_HASH) {
			m->hash = __HASHTABLE_EMPTY_HASH;
			continue;
		}
		if (!_hashtable_slot_needs_rehash(bitmap, index)) {
			continue;
		}
		_hashtable_hash_t hash = m->hash;
		_hashtable_idx_t optimal_index = _hashtable_hash_to_index(table, hash);
		if (optimal_index == index) {
			_hashtable_slot_clear_needs_rehash(bitmap, index);
			m->bitmap |= 1;
			num_rehashed++;
			continue;
		}
		m->hash = __HASHTABLE_EMPTY_HASH;
		memcpy(entry, _hashtable_entry(table, index, info), info->entry_size);

		do {
			if (!_hashtable_insert_during_resize(table, &hash, entry, bitmap, info)) {
				table->capacity = old_capacity;
				new_capacity *= 2;
				// insert the currently displaced entry somewhere, so it doesn't get lost
				for (_hashtable_uint_t i = 0; hash != __HASHTABLE_EMPTY_HASH; i++) {
					_hashtable_metadata_t *m = _hashtable_metadata(table, i, info);
					if (m->hash != __HASHTABLE_EMPTY_HASH) {
						continue;
					}
					m->hash = hash;
					memcpy(_hashtable_entry(table, i, info), entry, info->entry_size);
					break;
				}
				goto retry;
			}
			num_rehashed++;
		} while (hash != __HASHTABLE_EMPTY_HASH);
	}

	free(bitmap_to_free);

	size_t new_metadata_offset = _hashtable_metadata_offset(table->capacity, info);
	_hashtable_metadata_t *new_metadata = (_hashtable_metadata_t *)(table->storage + new_metadata_offset);
	for (_hashtable_uint_t i = 0; i < old_capacity; i++) {
		new_metadata[i] = table->metadata[i];
	}
	_hashtable_realloc_storage(table, info);
}

static void _hashtable_grow(struct _hashtable *table, _hashtable_uint_t new_capacity,
			    const struct _hashtable_info *info)
{
	assert(new_capacity >= table->capacity && new_capacity > table->num_entries);

	_hashtable_uint_t old_capacity = table->capacity;
	table->capacity = new_capacity;
	_hashtable_realloc_storage(table, info);
	size_t old_metadata_offset = _hashtable_metadata_offset(old_capacity, info);
	_hashtable_metadata_t *old_metadata = (_hashtable_metadata_t *)(table->storage + old_metadata_offset);

	size_t bitmap_size = (new_capacity + 31) / 32 * sizeof(uint32_t);
	uint32_t *bitmap, *bitmap_to_free = NULL;
	if (bitmap_size <= 1024) {
		bitmap = alloca(bitmap_size);
		memset(bitmap, 0, bitmap_size);
	} else {
		bitmap = calloc(1, bitmap_size);
		if (unlikely(!bitmap)) {
			abort();
		}
		bitmap_to_free = bitmap;
	}

	/* Need to iterate backwards in case the metadata is bigger than the entries:
	 * eeeeemmmmmmmmmm
	 * eeeeeeeeeemmmmmmmmmmmmmmmmmmmm
	 */
	for (_hashtable_uint_t i = old_capacity; i-- > 0;) {
		_hashtable_metadata_t *m = _hashtable_metadata(table, i, info);
		m->hash = old_metadata[i].hash;
		m->bitmap = 0;
	}
	for (_hashtable_uint_t i = old_capacity; i < table->capacity; i++) {
		_hashtable_metadata_t *m = _hashtable_metadata(table, i, info);
		m->hash = __HASHTABLE_EMPTY_HASH;
		m->bitmap = 0;
	}

	void *entry = alloca(info->entry_size);
	_hashtable_uint_t num_rehashed = 0;
	for (_hashtable_idx_t index = 0; index < old_capacity; index++) {
		_hashtable_metadata_t *m = _hashtable_metadata(table, index, info);
		if (m->hash < __HASHTABLE_MIN_VALID_HASH) {
			m->hash = __HASHTABLE_EMPTY_HASH;
			continue;
		}
		if (!_hashtable_slot_needs_rehash(bitmap, index)) {
			continue;
		}
		_hashtable_hash_t hash = m->hash;
		_hashtable_idx_t optimal_index = _hashtable_hash_to_index(table, hash);
		if (optimal_index == index) {
			_hashtable_slot_clear_needs_rehash(bitmap, index);
			m->bitmap |= 1;
			num_rehashed++;
			continue;
		}
		m->hash = __HASHTABLE_EMPTY_HASH;
		memcpy(entry, _hashtable_entry(table, index, info), info->entry_size);

		do {
			// can't fail here
			_hashtable_insert_during_resize(table, &hash, entry, bitmap, info);
			num_rehashed++;
		} while (hash != __HASHTABLE_EMPTY_HASH);
	}
	free(bitmap_to_free);
}

void _hashtable_resize(struct _hashtable *table, _hashtable_uint_t new_capacity,
		       const struct _hashtable_info *info)
{
	new_capacity = _hashtable_round_capacity(new_capacity);
	while (_hashtable_max_entries(new_capacity, info) < table->num_entries) {
		new_capacity *= 2;
	}
	if (new_capacity < table->capacity) {
		_hashtable_shrink(table, new_capacity, info);
	} else {
		_hashtable_grow(table, new_capacity, info);
	}
}

_hashtable_idx_t _hashtable_insert(struct _hashtable *table, _hashtable_hash_t hash,
				   const struct _hashtable_info *info)
{
	hash = _hashtable_sanitize_hash(hash);
	table->num_entries++;
	if (table->num_entries > table->max_entries) {
		_hashtable_grow(table, 2 * table->capacity, info);
	}
	_hashtable_idx_t index;
	while (!_hashtable_do_insert(table, hash, &index, info)) {
		_hashtable_grow(table, 2 * table->capacity, info);
	}
	return index;
}

void _hashtable_remove(struct _hashtable *table, _hashtable_idx_t index,
		       const struct _hashtable_info *info)
{
	_hashtable_metadata_t *m = _hashtable_metadata(table, index, info);
	_hashtable_idx_t home = _hashtable_hash_to_index(table, m->hash);
	_hashtable_metadata_t *home_m = _hashtable_metadata(table, home, info);
	_hashtable_uint_t distance = _hashtable_wrap_index(index - home, table->capacity);
	home_m->bitmap &= ~((_hashtable_bitmap_t)1 << distance);
	m->hash = __HASHTABLE_EMPTY_HASH;
	table->num_entries--;
	if (table->num_entries < table->capacity / 8) {
		_hashtable_shrink(table, table->capacity / 4, info);
	}
}

void _hashtable_clear(struct _hashtable *table, const struct _hashtable_info *info)
{
	for (_hashtable_uint_t i = 0; i < table->capacity; i++) {
		_hashtable_metadata(table, i, info)->hash = __HASHTABLE_EMPTY_HASH;
		_hashtable_metadata(table, i, info)->bitmap = 0;
	}
	table->num_entries = 0;
}



#undef __HASHTABLE_NEIGHBORHOOD
#undef __HASHTABLE_EMPTY_HASH
#undef __HASHTABLE_MIN_VALID_HASH

#elif defined(HASHTABLE_ROBINHOOD)

#define __HASHTABLE_EMPTY_HASH 0
#define __HASHTABLE_MIN_VALID_HASH 1

typedef struct _hashtable_metadata {
	_hashtable_hash_t hash;
} _hashtable_metadata_t;

static _hashtable_idx_t _hashtable_wrap_index(_hashtable_idx_t start, _hashtable_uint_t i,
					      _hashtable_uint_t capacity)
{
	return (start + i) & (capacity - 1);
}

static _hashtable_hash_t _hashtable_get_hash(struct _hashtable *table, _hashtable_idx_t index,
					     const struct _hashtable_info *info)
{
	return table->metadata[index].hash;
}

static void _hashtable_set_hash(struct _hashtable *table, _hashtable_idx_t index, _hashtable_hash_t hash,
				const struct _hashtable_info *info)
{
	table->metadata[index].hash = hash;
}

static _hashtable_uint_t _hashtable_metadata_offset(_hashtable_uint_t capacity,
						    const struct _hashtable_info *info)
{
	return capacity * info->entry_size;
}

static _hashtable_metadata_t *_hashtable_metadata(struct _hashtable *table, _hashtable_idx_t index,
						  const struct _hashtable_info *info)
{
	return &table->metadata[index];
}

static _hashtable_uint_t _hashtable_get_distance(struct _hashtable *table, _hashtable_idx_t index,
						 const struct _hashtable_info *info)
{
	_hashtable_hash_t hash = _hashtable_get_hash(table, index, info);
	return (index - _hashtable_hash_to_index(table, hash)) & (table->capacity - 1);
}

static void _hashtable_realloc_storage(struct _hashtable *table, const struct _hashtable_info *info)
{
	assert((table->capacity & (table->capacity - 1)) == 0);
	_hashtable_uint_t size = info->entry_size + sizeof(_hashtable_metadata_t);
	assert(((_hashtable_uint_t)-1) / size >= table->capacity);
	size *= table->capacity;
	table->storage = realloc(table->storage, size);
	if (unlikely(!table->storage && table->capacity != 0)) {
		abort();
	}
	table->metadata = (_hashtable_metadata_t *)(table->storage +
						    _hashtable_metadata_offset(table->capacity, info));
	table->max_entries = _hashtable_max_entries(table->capacity, info);
}

void _hashtable_init(struct _hashtable *table, _hashtable_uint_t capacity,
		     const struct _hashtable_info *info)
{
	if (capacity < 8) {
		capacity = 8;
	}
	capacity = _hashtable_round_capacity(capacity);
	assert((capacity & (capacity - 1)) == 0);
	table->storage = NULL;
	table->num_entries = 0;
	table->capacity = capacity;
	_hashtable_realloc_storage(table, info);
	for (_hashtable_uint_t i = 0; i < capacity; i++) {
		_hashtable_metadata_t *m = _hashtable_metadata(table, i, info);
		m->hash = __HASHTABLE_EMPTY_HASH;
	}
}

void _hashtable_destroy(struct _hashtable *table)
{
	free(table->storage);
	memset(table, 0, sizeof(*table));
}

static _hashtable_hash_t _hashtable_sanitize_hash(_hashtable_hash_t hash)
{
	_hashtable_metadata_t m;
	m.hash = hash < __HASHTABLE_MIN_VALID_HASH ? hash - __HASHTABLE_MIN_VALID_HASH : hash;
	return m.hash;
}

bool _hashtable_lookup(struct _hashtable *table, void *key, _hashtable_hash_t hash,
		       _hashtable_idx_t *ret_index, const struct _hashtable_info *info)
{
	hash = _hashtable_sanitize_hash(hash);
	_hashtable_idx_t start = _hashtable_hash_to_index(table, hash);
	for (_hashtable_uint_t i = 0;; i++) {
		_hashtable_idx_t index = _hashtable_wrap_index(start, i, table->capacity);
		_hashtable_metadata_t *m = _hashtable_metadata(table, index, info);

		if (m->hash == __HASHTABLE_EMPTY_HASH) {
			break;
		}

		_hashtable_uint_t dist = _hashtable_get_distance(table, index, info);
		if (dist < i) {
			break;
		}

		if (hash == _hashtable_get_hash(table, index, info) &&
		    info->keys_match(key, _hashtable_entry(table, index, info))) {
			*ret_index = index;
			return true;
		}
	}
	return false;
}

_hashtable_idx_t _hashtable_get_next(struct _hashtable *table, _hashtable_idx_t start,
				     const struct _hashtable_info *info)
{
	for (_hashtable_idx_t index = start; index < table->capacity; index++) {
		_hashtable_metadata_t *m = _hashtable_metadata(table, index, info);
		if (m->hash >= __HASHTABLE_MIN_VALID_HASH) {
			return index;
		}
	}
	return table->capacity;
}

static bool _hashtable_slot_needs_rehash(uint32_t *bitmap, _hashtable_idx_t index)
{
	return !(bitmap[index / 32] & (1u << (index % 32)));
}

static void _hashtable_slot_clear_needs_rehash(uint32_t *bitmap, _hashtable_idx_t index)
{
	bitmap[index / 32] |= 1u << (index % 32);
}

static bool _hashtable_insert_robin_hood(struct _hashtable *table, _hashtable_idx_t start,
					 _hashtable_uint_t distance, _hashtable_hash_t *phash,
					 void *entry, uint32_t *bitmap,
					 const struct _hashtable_info *info)
{
	void *tmp_entry = alloca(info->entry_size);
	for (_hashtable_uint_t i = 0;; i++, distance++) {
		_hashtable_idx_t index = _hashtable_wrap_index(start, i, table->capacity);
		_hashtable_metadata_t *m = _hashtable_metadata(table, index, info);
		if (m->hash == __HASHTABLE_EMPTY_HASH) {
			if (bitmap) {
				_hashtable_slot_clear_needs_rehash(bitmap, index);
			}
			_hashtable_set_hash(table, index, *phash, info);
			memcpy(_hashtable_entry(table, index, info), entry, info->entry_size);
			return false;
		}

		_hashtable_uint_t d = 0;
		if ((bitmap && _hashtable_slot_needs_rehash(bitmap, index)) ||
		    (d = _hashtable_get_distance(table, index, info)) < distance) {
			_hashtable_hash_t tmp_hash = _hashtable_get_hash(table, index, info);
			memcpy(tmp_entry, _hashtable_entry(table, index, info), info->entry_size);

			_hashtable_set_hash(table, index, *phash, info);
			memcpy(_hashtable_entry(table, index, info), entry, info->entry_size);

			*phash = tmp_hash;
			memcpy(entry, tmp_entry, info->entry_size);

			if (bitmap && _hashtable_slot_needs_rehash(bitmap, index)) {
				_hashtable_slot_clear_needs_rehash(bitmap, index);
				return true;
			}

			distance = d;
		}
	}
}

static _hashtable_idx_t _hashtable_do_insert(struct _hashtable *table, _hashtable_hash_t hash,
					     const struct _hashtable_info *info)
{
	_hashtable_idx_t start = _hashtable_hash_to_index(table, hash);
	_hashtable_idx_t index;
	_hashtable_uint_t i;
	for (i = 0;; i++) {
		index = _hashtable_wrap_index(start, i, table->capacity);
		_hashtable_metadata_t *m = _hashtable_metadata(table, index, info);
		if (m->hash == __HASHTABLE_EMPTY_HASH) {
			break;
		}
		_hashtable_uint_t d = _hashtable_get_distance(table, index, info);
		if (d < i) {
			_hashtable_hash_t h = _hashtable_get_hash(table, index, info);
			void *entry = _hashtable_entry(table, index, info);
			start = _hashtable_wrap_index(start, i + 1, table->capacity);
			_hashtable_insert_robin_hood(table, start, d + 1, &h, entry, NULL, info);
			break;
		}
	}
	_hashtable_set_hash(table, index, hash, info);
	return index;
}

static void _hashtable_shrink(struct _hashtable *table, _hashtable_uint_t new_capacity,
			      const struct _hashtable_info *info)
{
	assert(new_capacity < table->capacity && new_capacity > table->num_entries);
	if (new_capacity < 8) {
		new_capacity = 8;
	}
	_hashtable_uint_t old_capacity = table->capacity;
	table->capacity = new_capacity;

	size_t bitmap_size = (old_capacity + 31) / 32 * sizeof(uint32_t);
	uint32_t *bitmap, *bitmap_to_free = NULL;
	if (bitmap_size <= 1024) {
		bitmap = alloca(bitmap_size);
		memset(bitmap, 0, bitmap_size);
	} else {
		bitmap = calloc(1, bitmap_size);
		if (unlikely(!bitmap)) {
			abort();
		}
		bitmap_to_free = bitmap;
	}

	void *entry = alloca(info->entry_size);
	_hashtable_uint_t num_rehashed = 0;
	for (_hashtable_idx_t index = 0; index < old_capacity; index++) {
		_hashtable_metadata_t *m = _hashtable_metadata(table, index, info);
		if (m->hash < __HASHTABLE_MIN_VALID_HASH) {
			m->hash = __HASHTABLE_EMPTY_HASH;
			continue;
		}
		if (!_hashtable_slot_needs_rehash(bitmap, index)) {
			continue;
		}
		_hashtable_hash_t hash = _hashtable_get_hash(table, index, info);
		_hashtable_idx_t optimal_index = _hashtable_hash_to_index(table, hash);
		if (optimal_index == index) {
			_hashtable_slot_clear_needs_rehash(bitmap, index);
			num_rehashed++;
			continue;
		}
		m->hash = __HASHTABLE_EMPTY_HASH;
		memcpy(entry, _hashtable_entry(table, index, info), info->entry_size);

		for (;;) {
			bool need_rehash = _hashtable_insert_robin_hood(table, optimal_index, 0,
									&hash, entry, bitmap, info);
			num_rehashed++;
			if (!need_rehash) {
				break;
			}
			optimal_index = _hashtable_hash_to_index(table, hash);
		}
	}

	free(bitmap_to_free);

	size_t new_metadata_offset = _hashtable_metadata_offset(table->capacity, info);
	_hashtable_metadata_t *new_metadata = (_hashtable_metadata_t *)(table->storage + new_metadata_offset);
	memmove(new_metadata, table->metadata, old_capacity * sizeof(table->metadata[0]));
	_hashtable_realloc_storage(table, info);
}

static void _hashtable_grow(struct _hashtable *table, _hashtable_uint_t new_capacity,
			    const struct _hashtable_info *info)
{
	assert(new_capacity >= table->capacity && new_capacity > table->num_entries);

	_hashtable_uint_t old_capacity = table->capacity;
	table->capacity = new_capacity;
	_hashtable_realloc_storage(table, info);
	size_t old_metadata_offset = _hashtable_metadata_offset(old_capacity, info);
	_hashtable_metadata_t *old_metadata = (_hashtable_metadata_t *)(table->storage + old_metadata_offset);

	size_t bitmap_size = (new_capacity + 31) / 32 * sizeof(uint32_t);
	uint32_t *bitmap, *bitmap_to_free = NULL;
	if (bitmap_size <= 1024) {
		bitmap = alloca(bitmap_size);
		memset(bitmap, 0, bitmap_size);
	} else {
		bitmap = calloc(1, bitmap_size);
		if (unlikely(!bitmap)) {
			abort();
		}
		bitmap_to_free = bitmap;
	}

	/* Need to use memmove in case the metadata is bigger than the entries:
	 * eeeeemmmmmmmmmm
	 * eeeeeeeeeemmmmmmmmmmmmmmmmmmmm
	 */
	memmove(table->metadata, old_metadata, old_capacity * sizeof(table->metadata[0]));
	for (_hashtable_uint_t i = old_capacity; i < table->capacity; i++) {
		_hashtable_metadata(table, i, info)->hash = __HASHTABLE_EMPTY_HASH;
	}

	void *entry = alloca(info->entry_size);
	_hashtable_uint_t num_rehashed = 0;
	for (_hashtable_idx_t index = 0; index < old_capacity; index++) {
		_hashtable_metadata_t *m = _hashtable_metadata(table, index, info);
		if (m->hash < __HASHTABLE_MIN_VALID_HASH) {
			m->hash = __HASHTABLE_EMPTY_HASH;
			continue;
		}
		if (!_hashtable_slot_needs_rehash(bitmap, index)) {
			continue;
		}
		_hashtable_hash_t hash = _hashtable_get_hash(table, index, info);
		_hashtable_idx_t optimal_index = _hashtable_hash_to_index(table, hash);
		if (optimal_index == index) {
			_hashtable_slot_clear_needs_rehash(bitmap, index);
			num_rehashed++;
			continue;
		}
		m->hash = __HASHTABLE_EMPTY_HASH;
		memcpy(entry, _hashtable_entry(table, index, info), info->entry_size);

		for (;;) {
			bool need_rehash = _hashtable_insert_robin_hood(table, optimal_index, 0,
									&hash, entry, bitmap, info);
			num_rehashed++;
			if (!need_rehash) {
				break;
			}
			optimal_index = _hashtable_hash_to_index(table, hash);
		}
	}
	free(bitmap_to_free);
}

void _hashtable_resize(struct _hashtable *table, _hashtable_uint_t new_capacity,
		       const struct _hashtable_info *info)
{
	new_capacity = _hashtable_round_capacity(new_capacity);
	while (_hashtable_max_entries(new_capacity, info) < table->num_entries) {
		new_capacity *= 2;
	}
	if (new_capacity < table->capacity) {
		_hashtable_shrink(table, new_capacity, info);
	} else {
		_hashtable_grow(table, new_capacity, info);
	}
}

_hashtable_idx_t _hashtable_insert(struct _hashtable *table, _hashtable_hash_t hash,
				   const struct _hashtable_info *info)
{
	hash = _hashtable_sanitize_hash(hash);
	table->num_entries++;
	if (table->num_entries > table->max_entries) {
		_hashtable_grow(table, 2 * table->capacity, info);
	}
	return _hashtable_do_insert(table, hash, info);
}

void _hashtable_remove(struct _hashtable *table, _hashtable_idx_t index,
		       const struct _hashtable_info *info)
{
	_hashtable_metadata(table, index, info)->hash = __HASHTABLE_EMPTY_HASH;
	table->num_entries--;
	if (table->num_entries < table->capacity / 8) {
		_hashtable_shrink(table, table->capacity / 4, info);
	} else {
		for (_hashtable_uint_t i = 0;; i++) {
			_hashtable_idx_t current_index = _hashtable_wrap_index(index, i, table->capacity);
			_hashtable_idx_t next_index = _hashtable_wrap_index(index, i + 1, table->capacity);
			_hashtable_metadata_t *m = _hashtable_metadata(table, next_index, info);
			_hashtable_uint_t distance;
			if (m->hash == __HASHTABLE_EMPTY_HASH ||
			    (distance = _hashtable_get_distance(table, next_index, info)) == 0) {
				_hashtable_metadata(table, current_index, info)->hash = __HASHTABLE_EMPTY_HASH;
				break;
			}
			_hashtable_hash_t hash = _hashtable_get_hash(table, next_index, info);
			_hashtable_set_hash(table, current_index, hash, info);
			const void *entry = _hashtable_entry(table, next_index, info);
			memcpy(_hashtable_entry(table, current_index, info), entry, info->entry_size);
		}
	}
}

void _hashtable_clear(struct _hashtable *table, const struct _hashtable_info *info)
{
	for (_hashtable_uint_t i = 0; i < table->capacity; i++) {
		_hashtable_metadata_t *m = _hashtable_metadata(table, i, info);
		m->hash = __HASHTABLE_EMPTY_HASH;
	}
	table->num_entries = 0;
}

#undef __HASHTABLE_EMPTY_HASH
#undef __HASHTABLE_MIN_VALID_HASH

#else
# error "No hashtable implementation selected"
#endif
