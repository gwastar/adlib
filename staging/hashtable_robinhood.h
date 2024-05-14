#ifndef __HASH_TABLE_INCLUDE__
#define __HASH_TABLE_INCLUDE__

#include <stdlib.h>
#include <stdbool.h>

#define DEFINE_HASHTABLE(name, key_type, item_type, THRESHOLD, ...)	\
									\
	struct name##_bucket {						\
		unsigned int hash;					\
		key_type key;						\
	};								\
									\
	struct name {							\
		unsigned int num_items;					\
		unsigned int size;					\
		struct name##_bucket *buckets;				\
		item_type *items;					\
	};								\
									\
	static inline unsigned int __##name##_hash_to_idx(unsigned int hash, unsigned int table_size) \
	{								\
		return hash & (table_size - 1);				\
	}								\
									\
	static inline unsigned int __##name##_distance(unsigned int idx, unsigned int hash, unsigned int table_size) \
	{								\
		return (idx - __##name##_hash_to_idx(hash, table_size)) & (table_size - 1); \
	}								\
									\
	static void name##_init(struct name *table, unsigned int size)	\
	{								\
		if ((size & (size - 1)) != 0) {				\
			/* round to next power of 2 */			\
			size--;						\
			size |= size >> 1;				\
			size |= size >> 2;				\
			size |= size >> 4;				\
			size |= size >> 8;				\
			size |= size >> 16;				\
			/* size |= size >> 32; */			\
			size++;						\
		}							\
		size_t buckets_size = size * sizeof(*table->buckets);	\
		size_t items_size = size * sizeof(*table->items);	\
		char *mem = aligned_alloc(64, buckets_size + items_size); \
		table->buckets = (struct name##_bucket *)mem;		\
		mem += buckets_size;					\
		table->items = (item_type *)mem;			\
		for (unsigned int i = 0; i < size; i++) {		\
			table->buckets[i].hash = 0;			\
		}							\
		table->num_items = 0;					\
		table->size = size;					\
	}								\
									\
	static void name##_destroy(struct name *table)			\
	{								\
		free(table->buckets);					\
		table->size = 0;					\
	}								\
									\
	static unsigned int __##name##_lookup_internal(struct name *table, key_type key, unsigned int hash) \
	{								\
		hash = hash == 0 ? -1 : hash;				\
		unsigned int index = __##name##_hash_to_idx(hash, table->size);	\
		unsigned int distance = 0;				\
		for (;;) {						\
			struct name##_bucket *bucket = &table->buckets[index]; \
			key_type const *a = &key;			\
			key_type const *b = &bucket->key;		\
			if (bucket->hash == 0) {			\
				return -1;				\
			}						\
									\
			unsigned int d = __##name##_distance(index, bucket->hash, table->size);	\
			if (distance > d) {				\
				return -1;				\
			}						\
									\
			if (hash == bucket->hash && (__VA_ARGS__)) {	\
				return index;				\
			}						\
									\
			index = (index + 1) & (table->size - 1);	\
			distance++;					\
		}							\
		return -1;						\
	}								\
									\
	static item_type *name##_lookup(struct name *table, key_type key, unsigned int hash) \
	{								\
		unsigned int index = __##name##_lookup_internal(table, key, hash); \
		if (index == -1) {					\
			return NULL;					\
		}							\
		return &table->items[index];				\
	}								\
									\
	static bool name##_remove(struct name *table, key_type key, unsigned int hash, key_type *ret_key, item_type *ret_item) \
	{								\
		unsigned int index = __##name##_lookup_internal(table, key, hash); \
		if (index == -1) {					\
			return false;					\
		}							\
									\
		if (ret_key) {						\
			*ret_key = table->buckets[index].key;		\
		}							\
		if (ret_item) {						\
			*ret_item = table->items[index];		\
		}							\
									\
		for (;;) {						\
			unsigned int next_index = (index + 1) & (table->size - 1); \
			struct name##_bucket *current = &table->buckets[index];	\
			struct name##_bucket *next = &table->buckets[next_index]; \
			unsigned int d = __##name##_distance(next_index, next->hash, table->size); \
			if (next->hash == 0 || d == 0) {		\
				current->hash = 0;			\
				break;					\
			}						\
			current->hash = next->hash;			\
			current->key = next->key;			\
			table->items[index] = table->items[next_index];	\
			index = next_index;				\
		}							\
									\
		table->num_items--;					\
		return true;						\
	}								\
									\
	static void __##name##_move(struct name *table, unsigned int index) \
	{								\
		struct name##_bucket *bucket = &table->buckets[index];	\
		unsigned int hash = bucket->hash;			\
		key_type key = bucket->key;				\
		item_type item = table->items[index];			\
		unsigned int distance = __##name##_distance(index, bucket->hash, table->size); \
		for (;;) {						\
			index = (index + 1) & (table->size - 1);	\
			distance++;					\
			bucket = &table->buckets[index];		\
			if (bucket->hash == 0) {			\
				bucket->hash = hash;			\
				bucket->key = key;			\
				table->items[index] = item;		\
				break;					\
			}						\
			unsigned int d = __##name##_distance(index, bucket->hash, table->size);	\
			if (d < distance) {				\
				unsigned int tmp_hash = bucket->hash;	\
				key_type tmp_key = bucket->key;		\
				item_type tmp_item = table->items[index]; \
				bucket->hash = hash;			\
				bucket->key = key;			\
				table->items[index] = item;		\
				hash = tmp_hash;			\
				key = tmp_key;				\
				item = tmp_item;			\
				distance = d;				\
			}						\
		}							\
	}								\
									\
	static item_type *__##name##_insert_internal(struct name *table, key_type *key, unsigned int hash) \
	{								\
		struct name##_bucket *bucket;				\
		unsigned int index = __##name##_hash_to_idx(hash, table->size);	\
		unsigned int distance = 0;				\
		for (;;) {						\
			bucket = &table->buckets[index];		\
			if (bucket->hash == 0) {			\
				break;					\
			}						\
			unsigned int d = __##name##_distance(index, bucket->hash, table->size);	\
			if (d < distance) {				\
				__##name##_move(table, index);		\
				break;					\
			}						\
			index = (index + 1) & (table->size - 1);	\
			distance++;					\
		}							\
									\
		bucket->hash = hash;					\
		bucket->key = *key;					\
									\
		return &table->items[index];				\
	}								\
									\
	static void name##_resize(struct name *table, unsigned int size) \
	{								\
		struct name new_table;					\
		name##_init(&new_table, size);				\
									\
		for (unsigned int i = 0; i < table->size; i++) {	\
			struct name##_bucket *bucket = &table->buckets[i]; \
			if (bucket->hash != 0) {			\
				item_type *item = __##name##_insert_internal(&new_table, &bucket->key, \
									     bucket->hash); \
				*item = table->items[i];		\
			}						\
		}							\
		new_table.num_items = table->num_items;			\
		name##_destroy(table);					\
		*table = new_table;					\
	}								\
									\
	static item_type *name##_insert(struct name *table, key_type key, unsigned int hash) \
	{								\
		table->num_items++;					\
		if (table->num_items * 10 > THRESHOLD * table->size) {	\
			name##_resize(table, 2 * table->size);		\
		}							\
									\
		hash = hash == 0 ? -1 : hash;				\
		return __##name##_insert_internal(table, &key, hash);	\
	}								\

#endif
