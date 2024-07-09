#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include "array.h"
#include "heap.h"
#include "random.h"
#include "sort.h"

#define N 5

static size_t comparisons;
static struct random_state global_rng = RANDOM_STATE_INITIALIZER(0xdeadbeef);

static size_t random_size_t(void)
{
	return (size_t)random_next_u64(&global_rng);
}

static
// __attribute__((noinline, noipa))
int int_cmp(const void *a, const void *b)
{
	comparisons++;
	return *(const int *)a - *(const int *)b;
}

static int int_cmp_rev(const void *a, const void *b)
{
	return *(const int *)b - *(const int *)a;
}

static int int_is_sorted(int *arr, size_t n)
{
	for (size_t i = 1; i < n; i++) {
		if (arr[i] < arr[i - 1]) {
			return 0;
		}
	}
	return 1;
}

static int (*int_cmp_ptr)(const void *a, const void *b) = int_cmp;

DEFINE_SORTFUNC(integer_sort, int, 16, int_cmp_ptr(a, b))
DEFINE_BINHEAP(intheap, int, int_cmp_ptr(a, b) > 0)

static unsigned long long ns_elapsed(struct timespec *start, struct timespec *end)
{
	unsigned long long s = end->tv_sec - start->tv_sec;
	unsigned long long ns = end->tv_nsec - start->tv_nsec;
	return ns + 1000000000 * s;
}

static int double_cmp(const void *_a, const void *_b)
{
	double a = *(const double *)_a;
	double b = *(const double *)_b;
	return a > b ? 1 : (a < b ? -1 : 0);
}

static double get_median_time(double nanoseconds[N])
{
	qsort(nanoseconds, N, sizeof(nanoseconds[0]), double_cmp);
	double q = 0.5;
	double x = q * (N - 1);
	unsigned int idx0 = (unsigned int)x;
	unsigned int idx1 = (N == 1) ? idx0 : idx0 + 1;
	double fract = x - idx0;
	double median = (1.0 - fract) * nanoseconds[idx0] + fract * nanoseconds[idx1];
	return median;
}

enum array_type {
	ARRAY_RANDOM,
	ARRAY_SORTED,
	ARRAY_REVERSE,
	ARRAY_SORTED_PARTITIONS,
	ARRAY_UP_DOWN,
	ARRAY_MOSTLY_SORTED,
	ARRAY_ZERO_ONE,
	ARRAY_MOD_8,

	__ARRAY_TYPE_COUNT
};

static const char *array_type_string(enum array_type type)
{
	switch (type) {
	case ARRAY_SORTED:            return "sorted";
	case ARRAY_RANDOM:            return "random";
	case ARRAY_REVERSE:           return "reverse";
	case ARRAY_SORTED_PARTITIONS: return "partitions";
	case ARRAY_UP_DOWN:           return "updown";
	case ARRAY_MOSTLY_SORTED:     return "mostly sorted";
	case ARRAY_ZERO_ONE:          return "01    ";
	case ARRAY_MOD_8:             return "mod8  ";
	case __ARRAY_TYPE_COUNT:      break;
	}
	assert(false);
	return "";
}

static void print_header(const char *name)
{
	printf(" %-9.9s", name);
	for (enum array_type type = 0; type < __ARRAY_TYPE_COUNT; type++) {
		printf(" \u2502 %-9.9s", array_type_string(type));
	}
	putchar('\n');
	for (unsigned int i = 0; i < (__ARRAY_TYPE_COUNT + 1) * 12 - 1; i++) {
		if (i % 12 == 11) {
			fputs("\u253c", stdout);
		} else {
			fputs("\u2500", stdout);
		}
	}
	putchar('\n');
}

static array_t(int) create_int_array(size_t n, enum array_type type)
{
	random_state_init(&global_rng, 0xdeadbeef);

	array_t(int) arr = NULL;
	array_reserve(arr, n);
	switch (type) {
	case ARRAY_ZERO_ONE:
		for (size_t i = 0; i < n; i++) {
			array_add(arr, i % 2);
		}
		break;
	case ARRAY_MOD_8:
		for (size_t i = 0; i < n; i++) {
			array_add(arr, i % 8);
		}
		break;
	default:
		for (size_t i = 0; i < n; i++) {
			array_add(arr, (int)i);
		}
		break;
	}

	switch (type) {
	case ARRAY_SORTED:
		array_sort(arr, int_cmp);
		break;
	case ARRAY_RANDOM:
	case ARRAY_ZERO_ONE:
	case ARRAY_MOD_8:
		array_shuffle(arr, random_size_t);
		break;
	case ARRAY_REVERSE:
		array_sort(arr, int_cmp_rev);
		break;
	case ARRAY_SORTED_PARTITIONS: {
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
		break;
	}
	case ARRAY_UP_DOWN: {
		size_t l = array_length(arr);
		size_t h = l / 2;
		qsort(arr, h, sizeof(arr[0]), int_cmp);
		qsort(&arr[h], l - h, sizeof(arr[0]), int_cmp_rev);
		break;
	}
	case ARRAY_MOSTLY_SORTED: {
		array_sort(arr, int_cmp);
		size_t len = array_length(arr);
		size_t n = len / 10;
		for (size_t i = 0; i < n; i++) {
			size_t idx1 = random_size_t() % len;
			size_t idx2 = random_size_t() % len;
			array_swap(arr, idx1, idx2);
		}
		break;
	}
	case __ARRAY_TYPE_COUNT:
	default:
		assert(false);
	}
	return arr;
}

static void int_sort(int *arr, size_t size)
{
	integer_sort(arr, size);
	// qsort(arr, size, sizeof(arr[0]), int_cmp);
	// intheap_heapify(arr, size);
	// intheap_sort(arr, size);
}

static void int_benchmark(void)
{
	print_header("int");
	const unsigned int max_order = 22;
	for (unsigned int order = 4; order <= max_order; order += 2) {
		size_t size = (size_t)1 << order;
		unsigned int d_order = max_order - order;
		const size_t iterations = ((size_t)1 << d_order) / (d_order + 1);
		printf(" 2^%2u     ", order);
		for (enum array_type array_type = 0; array_type < __ARRAY_TYPE_COUNT; array_type++) {
			double times[N];
			array_t(int) arr_proto = create_int_array(size, array_type);
			array_t(int) arr = NULL;
			for (unsigned int n = 0; n < N; n++) {
				double avg = 0;
				for (size_t i = 0; i < iterations; i++) {
					struct timespec start, end;

					array_clear(arr);
					array_add_array(arr, arr_proto);

					clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start);

					int_sort(arr, size);

					clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end);
					avg = (avg * i + ns_elapsed(&start, &end)) / (i + 1);
				}
				times[n] = avg;
			}
			assert(int_is_sorted(arr, size));
			array_free(arr);
			array_free(arr_proto);
			double t = get_median_time(times);
			const char *unit = "ns";
			if (t > 1000000000) {
				t *= 1.0 / 1000000000;
				unit = " s";
			} else if (t > 1000000) {
				t *= 1.0 / 1000000;
				unit = "ms";
			} else if (t > 1000) {
				t *= 1.0 / 1000;
				unit = "us";
			}
			printf(" \u2502 %6.2f %s", t, unit);
		}
		putchar('\n');
	}
}

int main (void)
{
	int_benchmark();
}
