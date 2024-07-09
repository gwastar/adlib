#ifndef __SORT_INCLUDE__
#define __SORT_INCLUDE__

// TODO implement 3-way partitioning? (with dual pivot?)
// TODO user data pointer for comparison
// TODO test whether recursion would be faster than the explicit stack
// TODO test whether copying the pivot to the stack would be faster
// TODO detect when data is already correctly partitioned and attempt insertion sort
// TODO detect when data is probably in reverse order and reverse it

// 'name' is the identifier of the sort function.
// 'type' is the element type of the array to sort.
// 'threshold' is the minimum number of elements for which quicksort will be used,
// below this number insertion sort will be used.
// The last argument should be a code expression that compares two elements:
// The expression receives two pointers to array elements called 'a' and 'b' and
// "must return an integer less than, equal to, or greater than zero if the first argument (a)
// is considered to be respectively less than, equal to, or greater than the second (b)" (see qsort manpage).
// Examples:
//   DEFINE_SORTFUNC(integer_sort, int, 16, *a - *b)
//   DEFINE_SORTFUNC(string_sort, char *, 16, strcmp(*a, *b))
// To sort an array with the defined functions do:
//   int array[N] = {1, 3, 2, ...};
//   integer_sort(array, N);
// or
//   char *array[N] = {"abc", "ghi", "def", ...}
//   string_sort(array, N);

#include "heap.h"

#define DEFINE_SORTFUNC(name, type, threshold, ...)			\
									\
	static inline int _##name##_cmp(type const *a, type const *b)	\
	{								\
		return (__VA_ARGS__);					\
	}								\
									\
	DEFINE_BINHEAP(_##name##_heap, type, _##name##_cmp(a, b) > 0)	\
									\
	static inline void _##name##_swap(type *a, type *b)		\
	{								\
		type tmp = *a;						\
		*a = *b;						\
		*b = tmp;						\
	}								\
									\
	static size_t _##name##_ilog2(size_t n)				\
	{								\
		size_t ilog2;						\
		for (ilog2 = 0; n >>= 1; ilog2++);			\
		return ilog2;						\
	}								\
									\
	static inline int _##name##_sort2(type *arr, size_t a, size_t b) { \
		if (_##name##_cmp(&arr[b], &arr[a]) < 0) {		\
			_##name##_swap(&arr[a], &arr[b]);		\
			return 1;					\
		}							\
		return 0;						\
	}								\
									\
	static void _##name##_sort3(type *arr, size_t a, size_t b, size_t c) \
	{								\
		_##name##_sort2(arr, a, b);				\
		if (_##name##_sort2(arr, b, c)) {			\
			_##name##_sort2(arr, a, b);			\
		}							\
	}								\
									\
	static void _##name##_insertion_sort(type *arr, size_t left, size_t right) \
	{								\
		for (size_t i = left + 1; i <= right; i++) {		\
			size_t j = i;					\
			if (_##name##_cmp(&arr[j - 1], &arr[j]) < 0) {	\
				continue;				\
			}						\
			type tmp = arr[j];				\
			do {						\
				arr[j] = arr[j - 1];			\
				j--;					\
			} while (j != left && _##name##_cmp(&tmp, &arr[j - 1]) < 0); \
			arr[j] = tmp;					\
		}							\
	}								\
									\
	static void _##name##_unguarded_insertion_sort(type *arr, size_t left, size_t right) \
	{								\
		for (size_t i = left + 1; i <= right; i++) {		\
			size_t j = i;					\
			if (_##name##_cmp(&arr[j - 1], &arr[j]) < 0) {	\
				continue;				\
			}						\
			type tmp = arr[j];				\
			do {						\
				arr[j] = arr[j - 1];			\
				j--;					\
			} while (_##name##_cmp(&tmp, &arr[j - 1]) < 0); \
			arr[j] = tmp;					\
		}							\
	}								\
									\
	static size_t _##name##_select_pivot(type *arr, size_t left, size_t right) \
	{								\
		size_t mid = left + (right - left) / 2;			\
		if (right - left + 1 < 256) {				\
			_##name##_sort3(arr, left, mid, right);		\
		} else {						\
			size_t left_mid = left + (mid - left) / 2;	\
			size_t right_mid = mid + (right - mid) / 2;	\
			_##name##_sort3(arr, left_mid, left, mid - 1);	\
			_##name##_sort3(arr, mid + 1, right, right_mid); \
			_##name##_sort3(arr, left_mid + 1, mid, right_mid - 1); \
			/* ensure left <= mid <= right */		\
			_##name##_sort3(arr, left, mid, right);		\
		}							\
		return mid;						\
	}								\
									\
	static void name(type *arr, size_t n)				\
	{								\
		const int insertion_sort_at_the_end = 0; /* TODO figure out which is better */ \
		switch (n) {						\
		case 1: return;						\
		case 2: _##name##_sort2(arr, 0, 1); return;		\
		case 3: _##name##_sort3(arr, 0, 1, 2); return;		\
		}							\
		const size_t T = (threshold) > 2 ? (threshold) : 2;	\
		if (n >= T) {						\
			struct {					\
				size_t left;				\
				size_t right;				\
			} stack[64];					\
			size_t sp = 0;					\
			size_t left = 0;				\
			size_t right = n - 1;				\
			size_t bad_partition_limit = _##name##_ilog2(n); \
			for (;;) {					\
				size_t size = right - left + 1;		\
				if (size < T) {				\
					if (!insertion_sort_at_the_end) { \
						if (left == 0) {	\
							_##name##_insertion_sort(arr, left, right); \
						} else {		\
							_##name##_unguarded_insertion_sort(arr, left, right); \
						}			\
					}				\
				pop_stack:				\
					if (sp == 0) {			\
						break;			\
					}				\
					sp--;				\
					left = stack[sp].left;		\
					right = stack[sp].right;	\
					continue;			\
				}					\
				size_t mid = _##name##_select_pivot(arr, left, right); \
				size_t i = left + 1;			\
				size_t j = right - 1;			\
				do {					\
					while (_##name##_cmp(&arr[i], &arr[mid]) < 0) { \
						i++;			\
					}				\
									\
					while (_##name##_cmp(&arr[mid], &arr[j]) < 0) { \
						j--;			\
					}				\
									\
					if (i >= j) {			\
						if (i == j) {		\
							i++;		\
							j--;		\
						}			\
						break;			\
					}				\
									\
					_##name##_swap(&arr[i], &arr[j]); \
					if (mid == i) {			\
						mid = j;		\
					} else if (mid == j) {		\
						mid = i;		\
					}				\
				} while (++i <= --j);			\
				size_t left_size = j - left + 1;	\
				size_t right_size = right - i + 1;	\
				if (left_size > right_size) {		\
					if (right_size < size / 8 &&	\
					    --bad_partition_limit == 0) { \
						_##name##_heap_heapify(&arr[left], size); \
						_##name##_heap_sort(&arr[left], size); \
						goto pop_stack;		\
					}				\
					stack[sp].left = left;		\
					stack[sp].right = j;		\
					left = i;			\
				} else {				\
					if (left_size < size / 8 &&	\
					    --bad_partition_limit == 0) { \
						_##name##_heap_heapify(&arr[left], size); \
						_##name##_heap_sort(&arr[left], size); \
						goto pop_stack;		\
					}				\
					stack[sp].left = i;		\
					stack[sp].right = right;	\
					right = j;			\
				}					\
				sp++;					\
			}						\
			if (!insertion_sort_at_the_end) {		\
				return;					\
			}						\
		}							\
		if (T == 2) {						\
			return;						\
		}							\
		size_t m = 0;						\
		for (size_t i = 1; i < T && i < n; i++) {		\
			if (_##name##_cmp(&arr[i], &arr[m]) < 0) {	\
				m = i;					\
			}						\
		}							\
		if (m != 0) {						\
			_##name##_swap(&arr[0], &arr[m]);		\
		}							\
		_##name##_unguarded_insertion_sort(arr, 1, n - 1);	\
	}


#endif
