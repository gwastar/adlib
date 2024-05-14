#ifndef __HASH_TABLE_INCLUDE__
#define __HASH_TABLE_INCLUDE__

#include <stdlib.h>
#include <stdbool.h>

#define DEFINE_HASHTABLE(name, key_type, item_type, THRESHOLD, ...)	\
									\
	static const unsigned int name##_INVALID_INDEX = -1;		\
									\
	struct name##_bucket {						\
		key_type key;						\
		unsigned int first;					\
		unsigned int hash;					\
		unsigned int next;					\
	};								\
									\
	struct name {							\
		unsigned int num_items;					\
		unsigned int size;					\
		struct name##_bucket *list_entries;			\
		item_type *items;					\
	};								\
									\
	static inline unsigned int __##name##_hash_to_idx(unsigned int hash, unsigned int table_size) \
	{								\
		return hash & (table_size - 1);				\
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
		/* size_t indices_size = size * sizeof(*table->indices); */ \
		size_t list_entries_size = size * sizeof(*table->list_entries); \
		size_t items_size = size * sizeof(*table->items);	\
		char *mem = aligned_alloc(64, list_entries_size + items_size); \
		/* table->indices = (unsigned int *)mem; */		\
		/* mem += indices_size; */				\
		table->list_entries = (struct name##_bucket *)mem;	\
		mem += list_entries_size;				\
		table->items = (item_type *)mem;			\
		/* memset(table->indices, 0xff, indices_size); */	\
		/* memset(table->list_entries, 0, list_entries_size); */ \
		for (unsigned int i = 0; i < size; i++) {		\
			table->list_entries[i].first = name##_INVALID_INDEX; \
			table->list_entries[i].hash = 0;		\
		}							\
		table->num_items = 0;					\
		table->size = size;					\
	}								\
									\
	static void name##_destroy(struct name *table)			\
	{								\
		free(table->list_entries);				\
		table->size = 0;					\
	}								\
									\
	static item_type *name##_lookup(struct name *table, key_type key, unsigned int hash) \
	{								\
		hash = hash == 0 ? -1 : hash;				\
		unsigned int index = table->list_entries[__##name##_hash_to_idx(hash, table->size)].first; \
		for (;;) {						\
			if (index == name##_INVALID_INDEX) {		\
				return NULL;				\
			}						\
			struct name##_bucket *bucket = &table->list_entries[index]; \
			key_type const *a = &key;			\
			key_type const *b = &bucket->key;		\
			if (hash == bucket->hash && (__VA_ARGS__)) {	\
				return &table->items[index];		\
			}						\
			index = bucket->next;				\
		}							\
	}								\
									\
	static bool name##_remove(struct name *table, key_type key, unsigned int hash, key_type *ret_key, item_type *ret_item) \
	{								\
		hash = hash == 0 ? -1 : hash;				\
		unsigned int *indirect = &table->list_entries[__##name##_hash_to_idx(hash, table->size)].first; \
		unsigned int index = *indirect;				\
		struct name##_bucket *bucket;				\
		for (;;) {						\
			if (index == name##_INVALID_INDEX) {		\
				return false;				\
			}						\
			bucket = &table->list_entries[index];		\
			key_type const *a = &key;			\
			key_type const *b = &bucket->key;		\
			if (hash == bucket->hash && (__VA_ARGS__)) {	\
				break;					\
			}						\
			indirect = &bucket->next;			\
			index = *indirect;				\
		}							\
		if (ret_key) {						\
			*ret_key = bucket->key;				\
		}							\
		if (ret_item) {						\
			*ret_item = table->items[index];		\
		}							\
		*indirect = bucket->next;				\
		bucket->hash = 0;					\
		table->num_items--;					\
		return true;						\
	}								\
									\
	static item_type *__##name##_insert_internal(struct name *table, key_type key, unsigned int hash) \
	{								\
		unsigned int index = __##name##_hash_to_idx(hash, table->size); \
		unsigned int *pfirst = &table->list_entries[index].first; \
		struct name##_bucket *bucket;				\
		for (;;) {						\
			bucket = &table->list_entries[index];		\
			if (bucket->hash == 0) {			\
				break;					\
			}						\
			index = (index + 1) & (table->size - 1);	\
		}							\
									\
		bucket->hash = hash;					\
		bucket->next = *pfirst;					\
		bucket->key = key;					\
		*pfirst = index;					\
									\
		return &table->items[index];				\
	}								\
									\
	static void name##_resize(struct name *table, unsigned int size) \
	{								\
		unsigned int min_size = (table->num_items * 10 + THRESHOLD - 1) / THRESHOLD; \
		if (size < min_size) {					\
			size = min_size;				\
		}							\
		if (size == table->size) {				\
			return;						\
		}							\
		struct name new_table;					\
		name##_init(&new_table, size);				\
									\
		for (unsigned int i = 0; i < table->size; i++) {	\
			struct name##_bucket *bucket = &table->list_entries[i]; \
			if (bucket->hash != 0) {			\
				item_type *item = __##name##_insert_internal(&new_table, bucket->key, \
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
		return __##name##_insert_internal(table, key, hash);	\
	}

#endif
