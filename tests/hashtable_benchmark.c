#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include "array.h"
#include "config.h"
#include "hash.h"
#include "macros.h"
#include "random.h"
#include "hashtable.h"

#ifdef __HASHTABLE_PROFILING
# define N 1
#else
# define N 30
#endif

// TODO remove this eventually
#include <stdarg.h>
static char * _attr_format_printf(1, 2)
	mprintf(const char *fmt, ...)
{
	char buf[256];

	va_list args;
	va_start(args, fmt);
	size_t n = vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	char *str = malloc(n + 1);
	if (!str) {
		return NULL;
	}

	if (n < sizeof(buf)) {
		memcpy(str, buf, n + 1);
		return str;
	}

	va_start(args, fmt);
	vsnprintf(str, n + 1, fmt, args);
	va_end(args);

	return str;
}

static struct random_state g_random_state;
static unsigned int seed = 12345;

static size_t random_size_t(void)
{
	return (size_t)random_next_u64(&g_random_state);
}

static int int_cmp(const void *a, const void *b)
{
	return *(const int *)a - *(const int *)b;
}

static int int_cmp_rev(const void *a, const void *b)
{
	return int_cmp(b, a);
}

static inline uint32_t integer_hash(uint32_t x)
{
	x ^= x >> 16;
	x *= 0x7feb352d;
	x ^= x >> 15;
	x *= 0x846ca68b;
	x ^= x >> 16;
	return x;
}

static inline uint32_t bad_integer_hash(uint32_t x)
{
	return x;
}

static int string_cmp(const void *a, const void *b)
{
	return strcmp(*(const char **)a, *(const char **)b);
}

static int string_cmp_rev(const void *a, const void *b)
{
	return string_cmp(b, a);
}

static uint32_t string_hash(const void *string)
{
	return murmurhash3_x86_32(string, strlen(string), 0xdeadbeef).u32;
}

static _attr_pure uint32_t bad_string_hash(const void *string)
{
	const unsigned char *s = string;
	uint32_t h = 0, high;
	while (*s) {
		h = (h << 4) + *s++;
		high = h & 0xF0000000;
		if (high) {
			h ^= high >> 24;
		}
		h &= ~high;
	}
	return h;
}

struct short_string {
	char s[128];
};

static int short_string_cmp(const void *a, const void *b)
{
	return strcmp(((const struct short_string *)a)->s, ((const struct short_string *)b)->s);
}

static int short_string_cmp_rev(const void *a, const void *b)
{
	return short_string_cmp(b, a);
}

static unsigned long short_string_hash(const struct short_string s)
{
	return string_hash(s.s);
}

static _attr_pure unsigned long bad_short_string_hash(const struct short_string s)
{
	return bad_string_hash(s.s);
}

// static unsigned long long tp_to_ns(struct timespec *tp)
// {
// 	return tp->tv_nsec + 1000000000 * tp->tv_sec;
// }

static unsigned long long ns_elapsed(struct timespec *start, struct timespec *end)
{
	unsigned long long s = end->tv_sec - start->tv_sec;
	unsigned long long ns = end->tv_nsec - start->tv_nsec;
	return ns + 1000000000 * s;
}

static int ull_cmp(const void *_a, const void *_b)
{
	unsigned long long a = *(const unsigned long long *)_a;
	unsigned long long b = *(const unsigned long long *)_b;
	return a > b ? 1 : (a < b ? -1 : 0);
}

static double get_avg_rate(unsigned long long nanoseconds[N], size_t num_entries)
{
#if 0
	double avg = 0.0;
	for (unsigned int i = 0; i < N; i++) {
		avg += nanoseconds[i];
	}
	avg /= N;
#else
	qsort(nanoseconds, N, sizeof(nanoseconds[0]), ull_cmp);
	double q = 0.5;
	double x = q * (N - 1);
	unsigned int idx0 = (unsigned int)x;
	unsigned int idx1 = (N == 1) ? idx0 : idx0 + 1;
	double fract = x - idx0;
	double avg = (1.0 - fract) * (double)nanoseconds[idx0] + fract * (double)nanoseconds[idx1];
#endif
	return num_entries / avg;
}

static unsigned int benchmark_counter;

DEFINE_HASHTABLE(itable, int, int, 8, (*key == *entry))
	DEFINE_HASHTABLE(stable, char *, char *, 8, (strcmp(*key, *entry) == 0))
	DEFINE_HASHTABLE(sstable, char *, struct short_string, 8, (strcmp(*key, entry->s) == 0))
	DEFINE_HASHTABLE(ssstable, struct short_string, struct short_string, 8, (strcmp(entry->s, key->s) == 0))

#define BENCHMARK(name, hash, key_type, entry_type, keys1, values1, keys2, values2, keys3, values3, keys4, values4, ...) \
	for (unsigned int n = 0, c = benchmark_counter++; n < N; n++) {	\
		struct name name;					\
		struct timespec start_tp, end_tp;			\
									\
		fprintf(stderr, "\033[2K\r %u %u/%u", c, n, N);		\
									\
		name##_init(&name, 128);				\
									\
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp);	\
		for (size_t i = 0; i < num_entries; i++) {		\
			entry_type *entry = name##_insert(&name, keys1[i], (hash)(keys1[i])); \
			*entry = values1[i];				\
		}							\
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);	\
		insert[n] = ns_elapsed(&start_tp, &end_tp);		\
									\
									\
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp);	\
		for (size_t i = 0; i < num_entries; i++) {		\
			entry_type *entry = name##_lookup(&name, keys2[i], (hash)(keys2[i])); \
			key_type *k = &keys2[i];			\
			entry_type *v = entry;				\
			assert(__VA_ARGS__);				\
		}							\
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);	\
		lookup1[n] = ns_elapsed(&start_tp, &end_tp);		\
									\
									\
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp);	\
		for (size_t i = 0; i < num_entries; i++) {		\
			entry_type *entry = name##_lookup(&name, keys3[i], (hash)(keys3[i])); \
			assert(!entry);					\
		}							\
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);	\
		lookup2[n] = ns_elapsed(&start_tp, &end_tp);		\
									\
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp);	\
		for (size_t i = 0; i < num_entries; i++) {		\
			entry_type entry;				\
			bool found = name##_remove(&name, keys4[i], (hash)(keys4[i]), &entry); \
			key_type *k = &keys4[i];			\
			entry_type *v = &entry;				\
			assert(found && (__VA_ARGS__));			\
		}							\
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);	\
		delete[n] = ns_elapsed(&start_tp, &end_tp);		\
									\
		name##_destroy(&name);					\
									\
		name##_init(&name, 128);				\
									\
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp);	\
		for (size_t i = 0; i < num_entries; i++) {		\
			{						\
				entry_type *entry = name##_lookup(&name, keys2[i], (hash)(keys2[i])); \
				assert(!entry);				\
			}						\
			{						\
				entry_type *entry = name##_insert(&name, keys2[i], (hash)(keys2[i])); \
				*entry = values2[i];			\
			}						\
			{						\
				entry_type entry;			\
				bool found = name##_remove(&name, keys1[i], (hash)(keys1[i]), &entry); \
				key_type *k = &keys1[i];		\
				entry_type *v = &entry;			\
				assert(!found || (__VA_ARGS__));	\
			}						\
			{						\
				entry_type *entry = name##_lookup(&name, keys4[i], (hash)(keys4[i])); \
				key_type *k = &keys4[i];		\
				entry_type *v = entry;			\
				assert(!entry || (__VA_ARGS__));	\
			}						\
		}							\
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);	\
		mixed[n] = ns_elapsed(&start_tp, &end_tp);		\
									\
		name##_destroy(&name);					\
									\
		name##_init(&name, 128);				\
									\
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp);	\
		for (size_t i = 0; i < num_entries; i++) {		\
			{						\
				entry_type *entry = name##_insert(&name, keys1[i], (hash)(keys1[i])); \
				*entry = values1[i];			\
			}						\
			for (unsigned int j = i - 10; j < i; j++) {	\
				entry_type *entry = name##_lookup(&name, keys1[j], (hash)(keys1[j])); \
				key_type *k = &keys1[j];		\
				entry_type *v = entry;			\
				assert(__VA_ARGS__);			\
			}						\
		}							\
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);	\
		mixed2[n] = ns_elapsed(&start_tp, &end_tp);		\
									\
		name##_destroy(&name);					\
	}								\




enum insertion_order {
	INSERT_SORTED,
	INSERT_RANDOM,
	INSERT_REVERSE,
	INSERT_SORTED_PARTITIONS,
	INSERT_UP_DOWN,
	INSERT_MOSTLY_SORTED,

	__INSERT_ORDER_COUNT
};

static void print_header(const char *name, size_t num_elements)
{
#ifndef __HASHTABLE_PROFILING
	printf(" %-3.3s %-8zu \u2502 %-12.12s \u2502 %-12.12s \u2502 %-12.12s \u2502 %-12.12s"
	       " \u2502 %-12.12s \u2502 %-12.12s\n",
	       name, num_elements, " insertions", "lookups (y)", "lookups (n)",
	       " deletions", "mixed (+del)", "mixed (-del)");
	for (unsigned int i = 0; i < 7 * 15 - 1; i++) {
		if (i % 15 == 14) {
			fputs("\u253c", stdout);
		} else {
			fputs("\u2500", stdout);
		}
	}
	putchar('\n');
#else
	printf(" %-3.3s %-8zu \u2502 %-12.12s \u2502 %-12.12s \u2502 %-12.12s \u2502 %-12.12s"
	       " \u2502 %-12.12s\n",
	       name, num_elements, " lookup (y)", " lookup (n)", " insertion",
	       "collisions1", "collisions2");
	for (unsigned int i = 0; i < 6 * 15 - 1; i++) {
		if (i % 15 == 14) {
			fputs("\u253c", stdout);
		} else {
			fputs("\u2500", stdout);
		}
	}
	putchar('\n');
#endif
}

static const char *insertion_order_string(enum insertion_order order)
{
	switch (order) {
	case INSERT_SORTED:            return "sorted";
	case INSERT_RANDOM:            return "random";
	case INSERT_REVERSE:           return "revers";
	case INSERT_SORTED_PARTITIONS: return "partit";
	case INSERT_UP_DOWN:           return "updown";
	case INSERT_MOSTLY_SORTED:     return "mostly";
	case __INSERT_ORDER_COUNT:     break;
	}
	assert(false);
	return "";
}

static void print_results(size_t num_entries, enum insertion_order order, bool bad_hash,
			  unsigned long long insert[N], unsigned long long lookup1[N],
			  unsigned long long lookup2[N], unsigned long long delete[N],
			  unsigned long long mixed[N], unsigned long long mixed2[N])
{
	fputc('\r', stderr);
#ifndef __HASHTABLE_PROFILING
	double i  = 1000.0 * get_avg_rate(insert, num_entries);
	double l1 = 1000.0 * get_avg_rate(lookup1, num_entries);
	double l2 = 1000.0 * get_avg_rate(lookup2, num_entries);
	double d  = 1000.0 * get_avg_rate(delete, num_entries);
	double m1 = 1000.0 * get_avg_rate(mixed, num_entries);
	double m2 = 1000.0 * get_avg_rate(mixed2, num_entries);
	printf(" %-6.6s+%-5.5s \u2502%9.2f M/s \u2502%9.2f M/s \u2502%9.2f M/s \u2502%9.2f M/s "
	       "\u2502%9.2f M/s \u2502%9.2f M/s\n",
	       insertion_order_string(order), bad_hash ? "badh " : "goodh",
	       i, l1, l2, d, m1, m2);
#else
	extern size_t lookup_found_search_length;
	extern size_t lookup_notfound_search_length;
	extern size_t num_lookups_found;
	extern size_t num_lookups_notfound;
	extern size_t collisions1;
	extern size_t collisions2;
	extern size_t num_inserts;
	double avg_lookup_found_search_length = (double)lookup_found_search_length / num_lookups_found;
	double avg_lookup_notfound_search_length = (double)lookup_notfound_search_length / num_lookups_notfound;
	double avg_insert_search_length = (double)(collisions1 + collisions2 + 1) / num_inserts;
	double avg_collisions1 = (double)collisions1 / num_inserts;
	double avg_collisions2 = (double)collisions2 / num_inserts;
	printf(" %-6.6s+%-5.5s \u2502%13.2f \u2502%13.2f \u2502%13.2f \u2502%13.2f \u2502%13.2f\n",
	       insertion_order_string(order), bad_hash ? "badh " : "goodh",
	       avg_lookup_found_search_length, avg_lookup_notfound_search_length,
	       avg_insert_search_length, avg_collisions1, avg_collisions2);
	lookup_found_search_length = 0;
	lookup_notfound_search_length = 0;
	num_lookups_found = 0;
	num_lookups_notfound = 0;
	collisions1 = 0;
	collisions2 = 0;
	num_inserts = 0;
#endif
}

static void itable_order_array(int *arr, enum insertion_order order)
{
	switch (order) {
	case INSERT_SORTED:
		array_sort(arr, int_cmp);
		return;
	case INSERT_RANDOM:
		array_shuffle(arr, random_size_t);
		return;
	case INSERT_REVERSE:
		array_sort(arr, int_cmp_rev);
		return;
	case INSERT_SORTED_PARTITIONS: {
		array_shuffle(arr, random_size_t);
		const size_t partition_size = 1000;
		size_t len = array_length(arr);
		const size_t partitions = len / partition_size;
		for (size_t i = 0; i < partitions; i++) {
			size_t idx = i * partition_size;
			size_t n = partition_size;
			if (i == partitions - 1) {
				n += len % partition_size;
			}
			qsort(&arr[idx], n, sizeof(arr[0]), int_cmp);
		}
		return;
	}
	case INSERT_UP_DOWN: {
		size_t l = array_length(arr);
		size_t h = l / 2;
		qsort(arr, h, sizeof(arr[0]), int_cmp);
		qsort(&arr[h], l - h, sizeof(arr[0]), int_cmp_rev);
		return;
	}
	case INSERT_MOSTLY_SORTED: {
		array_sort(arr, int_cmp);
		size_t len = array_length(arr);
		size_t n = len / 10;
		for (size_t i = 0; i < n; i++) {
			size_t idx1 = random_size_t() % len;
			size_t idx2 = random_size_t() % len;
			array_swap(arr, idx1, idx2);
		}
		return;
	}
	case __INSERT_ORDER_COUNT:
		break;
	}
	assert(false);
}

static void itable_benchmark(size_t num_entries, enum insertion_order order, bool bad_hash)
{
	random_state_init(&g_random_state, seed);

	int *arr1 = NULL;
	array_reserve(arr1, num_entries);
	for (size_t i = 0; i < num_entries; i++) {
		array_add(arr1, i);
	}
	itable_order_array(arr1, order);

	int *arr2 = array_copy(arr1);
	array_shuffle(arr2, random_size_t);

	int *arr3 = NULL;
	array_reserve(arr3, num_entries);
	for (size_t i = 0; i < num_entries; i++) {
		array_add(arr3, i + num_entries);
	}
	array_shuffle(arr3, random_size_t);
	itable_order_array(arr3, order);

	int *arr4 = array_copy(arr1);
	array_shuffle(arr4, random_size_t);

	unsigned long long insert[N], lookup1[N], lookup2[N], delete[N], mixed[N], mixed2[N];

	BENCHMARK(itable, bad_hash ? bad_integer_hash : integer_hash, int, int, arr1, arr1, arr2, arr2, arr3, arr3, arr4, arr4, *k == *v);

	array_free(arr1);
	array_free(arr2);
	array_free(arr3);
	array_free(arr4);

	print_results(num_entries, order, bad_hash, insert, lookup1, lookup2, delete, mixed, mixed2);
}

static void stable_order_array(char **arr, enum insertion_order order)
{
	switch (order) {
	case INSERT_SORTED:
		array_sort(arr, string_cmp);
		return;
	case INSERT_RANDOM:
		array_shuffle(arr, random_size_t);
		return;
	case INSERT_REVERSE:
		array_sort(arr, string_cmp_rev);
		return;
	case INSERT_SORTED_PARTITIONS: {
		array_shuffle(arr, random_size_t);
		const size_t partition_size = 1000;
		size_t len = array_length(arr);
		const size_t partitions = len / partition_size;
		for (size_t i = 0; i < partitions; i++) {
			size_t idx = i * partition_size;
			size_t n = partition_size;
			if (i == partitions - 1) {
				n += len % partition_size;
			}
			qsort(&arr[idx], n, sizeof(arr[0]), string_cmp);
		}
		return;
	}
	case INSERT_UP_DOWN: {
		size_t l = array_length(arr);
		size_t h = l / 2;
		qsort(arr, h, sizeof(arr[0]), string_cmp);
		qsort(&arr[h], l - h, sizeof(arr[0]), string_cmp_rev);
		return;
	}
	case INSERT_MOSTLY_SORTED: {
		array_sort(arr, string_cmp);
		size_t len = array_length(arr);
		size_t n = len / 10;
		for (size_t i = 0; i < n; i++) {
			size_t idx1 = random_size_t() % len;
			size_t idx2 = random_size_t() % len;
			array_swap(arr, idx1, idx2);
		}
		return;
	}
	case __INSERT_ORDER_COUNT:
		break;
	}
	assert(false);
}

static void stable_benchmark(size_t num_entries, enum insertion_order order, bool bad_hash)
{
	random_state_init(&g_random_state, seed);

	char **arr1 = NULL;
	array_reserve(arr1, num_entries);
	for (size_t i = 0; i < num_entries; i++) {
		array_add(arr1, mprintf("%zu", i));
	}
	stable_order_array(arr1, order);

	char **arr2 = array_copy(arr1);
	array_shuffle(arr2, random_size_t);

	char **arr3 = NULL;
	array_reserve(arr3, num_entries);
	for (size_t i = 0; i < num_entries; i++) {
		array_add(arr3, mprintf("%zu", i + num_entries));
	}
	stable_order_array(arr3, order);

	char **arr4 = array_copy(arr1);
	array_shuffle(arr4, random_size_t);

	unsigned long long insert[N], lookup1[N], lookup2[N], delete[N], mixed[N], mixed2[N];

	BENCHMARK(stable, bad_hash ? bad_string_hash : string_hash, char *, char *, arr1, arr1, arr2, arr2, arr3, arr3, arr4, arr4, strcmp(*k, *v) == 0);

	array_foreach_value(arr1, iter) {
		free(iter);
	}
	array_foreach_value(arr3, iter) {
		free(iter);
	}
	array_free(arr1);
	array_free(arr2);
	array_free(arr3);
	array_free(arr4);

	print_results(num_entries, order, bad_hash, insert, lookup1, lookup2, delete, mixed, mixed2);
}

static void sstable_order_array(struct short_string *arr, enum insertion_order order)
{
	switch (order) {
	case INSERT_SORTED:
		array_sort(arr, short_string_cmp);
		return;
	case INSERT_RANDOM:
		array_shuffle(arr, random_size_t);
		return;
	case INSERT_REVERSE:
		array_sort(arr, short_string_cmp_rev);
		return;
	case INSERT_SORTED_PARTITIONS: {
		array_shuffle(arr, random_size_t);
		const size_t partition_size = 1000;
		size_t len = array_length(arr);
		const size_t partitions = len / partition_size;
		for (size_t i = 0; i < partitions; i++) {
			size_t idx = i * partition_size;
			size_t n = partition_size;
			if (i == partitions - 1) {
				n += len % partition_size;
			}
			qsort(&arr[idx], n, sizeof(arr[0]), short_string_cmp);
		}
		return;
	}
	case INSERT_UP_DOWN: {
		size_t l = array_length(arr);
		size_t h = l / 2;
		qsort(arr, h, sizeof(arr[0]), short_string_cmp);
		qsort(&arr[h], l - h, sizeof(arr[0]), short_string_cmp_rev);
		return;
	}
	case INSERT_MOSTLY_SORTED: {
		array_sort(arr, short_string_cmp);
		size_t len = array_length(arr);
		size_t n = len / 10;
		for (size_t i = 0; i < n; i++) {
			size_t idx1 = random_size_t() % len;
			size_t idx2 = random_size_t() % len;
			array_swap(arr, idx1, idx2);
		}
		return;
	}
	case __INSERT_ORDER_COUNT:
		break;
	}
	assert(false);
}

static void sstable_benchmark(size_t num_entries, enum insertion_order order, bool bad_hash)
{
	random_state_init(&g_random_state, seed);

	struct short_string *values1 = NULL;
	array_reserve(values1, num_entries);
	for (size_t i = 0; i < num_entries; i++) {
		struct short_string *entry = array_addn(values1, 1);
		sprintf(entry->s, "%zu", i);
	}
	sstable_order_array(values1, order);

	struct short_string *values2 = array_copy(values1);
	array_shuffle(values2, random_size_t);

	struct short_string *values3 = NULL;
	array_reserve(values3, num_entries);
	for (size_t i = 0; i < num_entries; i++) {
		struct short_string *entry = array_addn(values3, 1);
		sprintf(entry->s, "%zu", i + num_entries);
	}
	sstable_order_array(values3, order);

	struct short_string *values4 = array_copy(values1);
	array_shuffle(values4, random_size_t);

	char **keys1 = NULL;
	array_reserve(keys1, num_entries);
	array_foreach(values1, iter) {
		array_add(keys1, strdup(iter->s));
	}
	char **keys2 = NULL;
	array_reserve(keys2, num_entries);
	array_foreach(values2, iter) {
		array_add(keys2, strdup(iter->s));
	}
	char **keys3 = NULL;
	array_reserve(keys3, num_entries);
	array_foreach(values3, iter) {
		array_add(keys3, strdup(iter->s));
	}
	char **keys4 = NULL;
	array_reserve(keys4, num_entries);
	array_foreach(values4, iter) {
		array_add(keys4, strdup(iter->s));
	}

	unsigned long long insert[N], lookup1[N], lookup2[N], delete[N], mixed[N], mixed2[N];

	BENCHMARK(sstable, bad_hash ? bad_string_hash : string_hash, char *, struct short_string, keys1, values1, keys2, values2, keys3, values3, keys4, values4, strcmp(*k, v->s) == 0);

	array_foreach_value(keys1, s) {
		free(s);
	}
	array_foreach_value(keys2, s) {
		free(s);
	}
	array_foreach_value(keys3, s) {
		free(s);
	}
	array_foreach_value(keys4, s) {
		free(s);
	}
	array_free(keys1);
	array_free(keys2);
	array_free(keys3);
	array_free(keys4);
	array_free(values1);
	array_free(values2);
	array_free(values3);
	array_free(values4);

	print_results(num_entries, order, bad_hash, insert, lookup1, lookup2, delete, mixed, mixed2);
}

static void ssstable_benchmark(size_t num_entries, enum insertion_order order, bool bad_hash)
{
	random_state_init(&g_random_state, seed);

	struct short_string *arr1 = NULL;
	array_reserve(arr1, num_entries);
	for (size_t i = 0; i < num_entries; i++) {
		struct short_string *entry = array_addn(arr1, 1);
		sprintf(entry->s, "%zu", i);
	}
	sstable_order_array(arr1, order);

	struct short_string *arr2 = array_copy(arr1);
	array_shuffle(arr2, random_size_t);

	struct short_string *arr3 = NULL;
	array_reserve(arr3, num_entries);
	for (size_t i = 0; i < num_entries; i++) {
		struct short_string *entry = array_addn(arr3, 1);
		sprintf(entry->s, "%zu", i + num_entries);
	}
	sstable_order_array(arr3, order);

	struct short_string *arr4 = array_copy(arr1);
	array_shuffle(arr4, random_size_t);

	unsigned long long insert[N], lookup1[N], lookup2[N], delete[N], mixed[N], mixed2[N];

	BENCHMARK(ssstable, bad_hash ? bad_short_string_hash : short_string_hash, struct short_string, struct short_string, arr1, arr1, arr2, arr2, arr3, arr3, arr4, arr4, strcmp(k->s, v->s) == 0);

	array_free(arr1);
	array_free(arr2);
	array_free(arr3);
	array_free(arr4);

	print_results(num_entries, order, bad_hash, insert, lookup1, lookup2, delete, mixed, mixed2);
}

int main(void)
{
	size_t num_elements = 100000;

	if (1) {
		size_t itable_num_elements = 5 * num_elements;
		print_header("i", itable_num_elements);
		for (int hash = 1; hash < 2; hash++) {
			for (int order = 0; order < __INSERT_ORDER_COUNT; order++) {
				itable_benchmark(itable_num_elements, order, hash);
			}
		}
		putchar('\n');
	}

	if (1) {
		size_t stable_num_elements = 2 * num_elements;
		print_header("s", stable_num_elements);
		for (int hash = 0; hash < 1; hash++) {
			for (int order = 0; order < __INSERT_ORDER_COUNT; order++) {
				stable_benchmark(stable_num_elements, order, hash);
			}
		}
		putchar('\n');
	}

#ifndef __HASHTABLE_PROFILING
	if (1) {
		size_t sstable_num_elements = 3 * num_elements / 2;
		print_header("ss", sstable_num_elements);
		for (int hash = 0; hash < 1; hash++) {
			for (int order = 0; order < __INSERT_ORDER_COUNT; order++) {
				sstable_benchmark(sstable_num_elements, order, hash);
			}
		}
		putchar('\n');
	}
#endif

	if (1) {
		size_t ssstable_num_elements = 4 * num_elements / 3;
		print_header("sss", ssstable_num_elements);
		for (int hash = 0; hash < 1; hash++) {
			for (int order = 0; order < __INSERT_ORDER_COUNT; order++) {
				ssstable_benchmark(ssstable_num_elements, order, hash);
			}
		}
		putchar('\n');
	}
}
