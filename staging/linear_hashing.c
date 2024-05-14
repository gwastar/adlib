#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>

#define THRESHOLD 7

#define hashtable_get(key, table) __hashtable_get((void *)key, table)
#define hashtable_remove(key, table) __hashtable_remove((void *)key, table)
#define hashtable_insert(key, data, table)	  \
	__hashtable_insert((void *)key, (void *)data, table)

typedef unsigned long (*hashfunc_t)(const void *key);
typedef bool (*matchfunc_t)(const void *key1, const void *key2);
typedef bool (*iterfunc_t)(const void *key, void **data, void *iter_data);

struct item {
	void *key;
	void *data;
};

struct bucket {
	struct bucket  *next;
	unsigned long	num_items;
	struct item	items[];
};

struct hashtable {
	hashfunc_t	hashfunc; /* function to compute hash from key */
	matchfunc_t	matchfunc; /* function to compare to keys */
	unsigned long	n; /* number of buckets - p */
	unsigned long	p; /* index of next bucket to be split (<n) */
	unsigned long	bucketsize;
	unsigned long	num_items;
	struct bucket **buckets;
};

// murmur 3
static unsigned long string_hashfunc(const void *string)
{
	const void *data = string;
	const size_t nbytes = strlen(string);
	if (data == NULL || nbytes == 0) {
		return 0;
	}

	const uint32_t c1 = 0xcc9e2d51;
	const uint32_t c2 = 0x1b873593;

	const int nblocks = nbytes / 4;
	const uint32_t *blocks = (const uint32_t *) (data);
	const uint8_t *tail = (const uint8_t *) data + (nblocks * 4);

	uint32_t h = 0;

	int i;
	uint32_t k;
	for (i = 0; i < nblocks; i++) {
		k = blocks[i];

		k *= c1;
		k = (k << 15) | (k >> (32 - 15));
		k *= c2;

		h ^= k;
		h = (h << 13) | (h >> (32 - 13));
		h = (h * 5) + 0xe6546b64;
	}

	k = 0;
	switch (nbytes & 3) {
	case 3:
		k ^= tail[2] << 16;
	case 2:
		k ^= tail[1] << 8;
	case 1:
		k ^= tail[0];
		k *= c1;
		k = (k << 15) | (k >> (32 - 15));
		k *= c2;
		h ^= k;
	};

	h ^= nbytes;

	h ^= h >> 16;
	h *= 0x85ebca6b;
	h ^= h >> 13;
	h *= 0xc2b2ae35;
	h ^= h >> 16;

	return h;
}

static bool string_matchfunc(const void *key1, const void *key2)
{
	return strcmp(key1, key2) == 0;
}

static unsigned long default_hashfunc(const void *key)
{
	return (unsigned long)key;
}

static bool default_matchfunc(const void *key1, const void *key2)
{
	return key1 == key2;
}

static unsigned long get_idx(void *key, struct hashtable *table)
{
	unsigned long hash, idx1, idx2;

	hash = table->hashfunc(key);
	idx1 = hash % table->n;
	if (idx1 >= table->p)
		return idx1;

	idx2 = hash % (table->n * 2);
	return idx2;
}

static inline struct bucket *alloc_bucket(struct hashtable *table)
{
	size_t size = sizeof(struct bucket) + table->bucketsize * sizeof(struct item);

	return calloc(1, size);
}

static int insert_internal(void *key, void *data, unsigned long idx,
                           struct hashtable *table, bool check_duplicate)
{
	struct bucket *bucket, **indirect;
	struct item *item;
	unsigned long i;

	indirect = &table->buckets[idx];
	bucket = table->buckets[idx];
	while (bucket) {
		if (check_duplicate) {
			for (i = 0; i < bucket->num_items; i++) {
				item = &bucket->items[i];
				if (table->matchfunc(key, item->key))
					return EEXIST;
			}
		}

		if (bucket->num_items < table->bucketsize)
			goto found_bucket;

		indirect = &bucket->next;
		bucket = bucket->next;
	}

	bucket = alloc_bucket(table);
	if (!bucket)
		return ENOMEM;

	*indirect = bucket;

found_bucket:
	item = &bucket->items[bucket->num_items++];
	item->key = key;
	item->data = data;
	return 0;
}

static int split_bucket(unsigned long idx, struct hashtable *table)
{
	struct bucket *bucket, *insert_bucket, *next;
	struct bucket **indirect, **insert_indirect;
	struct item *item, *new_slot;
	unsigned long i, insert_idx, new_idx, num_items;

	bucket = table->buckets[idx];

	/* info about where to insert the next item in the old bucket */
	insert_indirect = &table->buckets[idx];
	insert_bucket = bucket;
	insert_idx = 0;

	while (bucket) {
		num_items = bucket->num_items;
		for (i = 0; i < num_items; i++) {
			item = &bucket->items[i];
			bucket->num_items--;
			new_idx = get_idx(item->key, table);
			if (new_idx == idx) {
				new_slot = &insert_bucket->items[insert_idx];
				if (item != new_slot) {
					*new_slot = *item;
					item->key = NULL;
					item->data = NULL;
				}
				insert_bucket->num_items++;

				insert_idx++;
				if (insert_idx == table->bucketsize) {
					insert_indirect = &insert_bucket->next;
					insert_bucket = insert_bucket->next;
					insert_idx = 0;
				}
			} else {
				/*
				 * This can only fail with ENOMEM here,
				 * we can't currently recover from that.
				 */
				if (insert_internal(item->key, item->data,
				                    new_idx, table, false) != 0)
					return -ENOMEM;
				item->key = NULL;
				item->data = NULL;
			}
		}
		bucket = bucket->next;
	}

	if (insert_idx != 0) {
		insert_indirect = &insert_bucket->next;
		insert_bucket = insert_bucket->next;
	}
	*insert_indirect = NULL;
	while (insert_bucket) {
		next = insert_bucket->next;
		free(insert_bucket);
		insert_bucket = next;
	}

	return 0;
}

int grow_hashtable(struct hashtable *table)
{
	unsigned long old_p, num_buckets = table->n + table->p;
	size_t new_size;

	new_size = (num_buckets + 1) * sizeof(struct bucket *);
	table->buckets = realloc(table->buckets, new_size);
	if (!table->buckets)
		return ENOMEM;

	table->buckets[num_buckets] = NULL;

	old_p = table->p++;
	if (table->p == table->n) {
		table->n *= 2;
		table->p = 0;
	}

	return split_bucket(old_p, table);
}

static inline int maybe_grow_table(struct hashtable *table)
{
	unsigned long base_capacity, num_buckets;

	num_buckets = table->n + table->p;
	base_capacity = num_buckets * table->bucketsize;
	if (table->num_items * 10  > THRESHOLD * base_capacity)
		return grow_hashtable(table);
	return 0;
}

int __hashtable_insert(void *key, void *data, struct hashtable *table)
{
	int retval;

	table->num_items++;
	/* grow before inserting, so we don't move the new item right after inserting */
	retval = maybe_grow_table(table);
	if (retval != 0)
		goto error;

	retval = insert_internal(key, data, get_idx(key, table), table, true);
	if (retval != 0)
		goto error;

	return 0;

error:
	table->num_items--;
	return retval;
}

void *__hashtable_get(void *key, struct hashtable *table)
{
	struct bucket *bucket;
	struct item *item;
	unsigned long i;

	bucket = table->buckets[get_idx(key, table)];
	while (bucket) {
		for (i = 0; i < bucket->num_items; i++) {
			item = &bucket->items[i];

			if (table->matchfunc(key, item->key))
				return item->data;
		}
		bucket = bucket->next;
	}

	/* TODO we can't return NULL if we accept NULL as data! */
	return NULL;
}

void hashtable_iterate(iterfunc_t f, void *iter_data, struct hashtable *table)
{
	struct bucket *bucket, *next;
	struct item *item;
	unsigned long i, k, num_buckets = table->n + table->p;

	for (i = 0; i < num_buckets; i++) {
		bucket = table->buckets[i];
		while (bucket) {
			for (k = 0; k < bucket->num_items; k++) {
				item = &bucket->items[k];

				if (!f(item->key, &item->data, iter_data))
					return;
			}
			bucket = bucket->next;
		}
	}
}

void *__hashtable_remove(void *key, struct hashtable *table)
{
	struct bucket *bucket, **indirect;
	struct item *item, *last;
	void *data;
	unsigned long i, idx = get_idx(key, table);

	indirect = &table->buckets[idx];
	bucket = table->buckets[idx];
	while (bucket) {
		for (i = 0; i < bucket->num_items; i++) {
			item = &bucket->items[i];
			if (!table->matchfunc(key, item->key))
				continue;

			data = item->data;
			while (bucket->next) {
				indirect = &bucket->next;
				bucket = bucket->next;
			}
			last = &bucket->items[--bucket->num_items];
			*item = *last;
			if (bucket->num_items == 0) {
				*indirect = NULL;
				free(bucket);
			}
			table->num_items--;
			return data;
		}
		indirect = &bucket->next;
		bucket = bucket->next;
	}

	/* TODO we can't return NULL if we accept NULL as data! */
	return NULL;
}

void clear_hashtable(struct hashtable *table)
{
	struct bucket *bucket, *next;
	unsigned long i, num_buckets = table->n + table->p;

	for (i = 0; i < num_buckets; i++) {
		bucket = table->buckets[i];
		table->buckets[i] = NULL;
		while (bucket) {
			next = bucket->next;
			free(bucket);
			bucket = next;
		}
	}
	table->num_items = 0;
}

void destroy_hashtable(struct hashtable *table)
{
	clear_hashtable(table);
	free(table->buckets);
	free(table);
}

struct hashtable *make_hashtable(unsigned long num_buckets, unsigned long bucketsize,
                                 hashfunc_t hashfunc, matchfunc_t matchfunc)
{
	struct hashtable *table;

	if (num_buckets == 0 || bucketsize == 0)
		return NULL;

	table = malloc(sizeof(*table));
	if (!table)
		return NULL;

	if (hashfunc)
		table->hashfunc = hashfunc;
	else
		table->hashfunc = default_hashfunc;
	if (matchfunc)
		table->matchfunc = matchfunc;
	else
		table->matchfunc = default_matchfunc;

	table->n	  = num_buckets;
	table->p	  = 0;
	table->bucketsize = bucketsize;
	table->num_items  = 0;

	table->buckets = calloc(num_buckets, sizeof(struct bucket *));
	if (!table->buckets) {
		free(table);
		return NULL;
	}

	return table;
}

struct hashtable *make_string_hashtable(unsigned long num_buckets,
                                        unsigned long bucketsize)
{
	return make_hashtable(num_buckets, bucketsize,
	                      string_hashfunc, string_matchfunc);
}


static void debug_print_item(struct item *item)
{
	printf("\tkey: %p\n", item->key);
	printf("\tdata: %p\n", item->data);
}

static void debug_print_bucket(struct bucket *bucket, struct hashtable *table)
{
	struct item *item;
	unsigned long i;

	for (int i = 0; i < bucket->num_items; i++) {
		item = &bucket->items[i];
		debug_print_item(item);
	}
	bucket = bucket->next;
	while (bucket) {
		printf("\t\t|\n");
		printf("\t\t|\n");
		printf("\t\tV\n");
		for (int i = 0; i < bucket->num_items; i++) {
			item = &bucket->items[i];
			debug_print_item(item);
		}
		bucket = bucket->next;
	}
}

static void debug_print_table(struct hashtable *table)
{
	unsigned long i, num_buckets;

	num_buckets = table->n + table->p;
	for (i = 0; i < num_buckets; i++) {
		if (i == table->p)
			printf("bucket %lu: P\n", i);
		else
			printf("bucket %lu:\n", i);
		if (table->buckets[i]) {
			debug_print_bucket(table->buckets[i], table);
			printf("\n");
		} else {
			printf("(empty)\n\n");
		}
	}
}

#include <assert.h>
#include <x86intrin.h>

static bool increment(const void *key, void **data, void *iter_data)
{
	assert(key == *data);
	(*(unsigned long*)data)++;
	return true;
}

int main(void)
{
	struct hashtable *table;
	unsigned long i, num_items;
	unsigned long long time;


#if 0
	table = make_hashtable(2, 2, NULL, NULL);
	for (i = 0; i < 10; i++) {
		printf("------------------------------------\n");
		printf("INSERT %lu\n", i);
		printf("------------------------------------\n");
		hashtable_insert(i, i, table);
		debug_print_table(table);
	}

	printf("------------------------------------\n");
	printf("------------------------------------\n");
	printf("------------------------------------\n\n");

	for (i = 0; i < 10; i++) {
		printf("%p\n", hashtable_remove(i, table));
	}
	printf("\n");
	debug_print_table(table);
	destroy_hashtable(table);

	printf("------------------------------------\n");
	printf("------------------------------------\n");
	printf("------------------------------------\n\n");
#endif

#if 1
	table = make_hashtable(64, 128, NULL, NULL);
	num_items = 100000;
	for (i = 0; i < num_items; i++) {
		hashtable_insert(i, i, table);
	}
	assert(table->num_items == num_items);
	printf("num_buckets = %lu\n", table->p + table->n);

	hashtable_iterate(increment, NULL, table);

	for (i = 0; i < num_items; i++) {
		void *data = hashtable_remove(i, table);
		assert(data == (void *)(i + 1));
	}
	assert(table->num_items == 0);
	destroy_hashtable(table);

	printf("------------------------------------\n");
	printf("------------------------------------\n");
	printf("------------------------------------\n\n");
#endif

#if 1
	table = make_hashtable(64, 128, NULL, NULL);
	for (i = 0; i < 100000; i++) {
		hashtable_insert(i, i, table);
	}
	assert(hashtable_insert(1, 0, table) == EEXIST);
	assert(hashtable_get(1, table) == (void *)1);
	destroy_hashtable(table);
#endif

#if 1
	table = make_string_hashtable(4, 2);

	num_items = 100;

	for (i = 0; i < num_items; i++) {
		char *key = malloc(8);
		char *data = malloc(8);
		sprintf(key, "key%lu", i);
		sprintf(data, "data%lu", i);
		hashtable_insert(key, data, table);
	}

	for (i = 0; i < num_items; i++) {
		char *s;
		char key[8];
		char data[8];
		sprintf(key, "key%lu", i);
		sprintf(data, "data%lu", i);
		s = hashtable_remove(key, table);
		assert(strcmp(s, data) == 0);
	}
	destroy_hashtable(table);
#endif

#if 1
	/* last measurement: clang: 148068991 gcc: 145770842 */
	table = make_hashtable(32, 128, NULL, NULL);
	time = __rdtsc();
	for (i = 0; i < 100000; i++) {
		hashtable_insert(i, i, table);
	}
	for (i = 0; i < 100000; i++) {
		hashtable_get(i, table);
	}
	for (i = 0; i < 100000; i++) {
		hashtable_remove(i, table);
	}
	printf("cycles elapsed: %llu\n", __rdtsc() - time);
	destroy_hashtable(table);
#endif
}
