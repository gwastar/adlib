#ifndef __HEAP_INCLUDE__
#define __HEAP_INCLUDE__

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

// TODO add DEFINE_MAXHEAP or generic DEFINE_HEAP?
// TODO document public API

// 'name' is used as a prefix for the function names
// 'type' is the element type (the heap functions operate on 'type' arrays)
// The last argument should be a code expression that compares two elements:
// The expression receives two pointers to array elements called 'a' and 'b' and
// must return true if *a is less than *b or false otherwise (i.e. it must be
// equivalent to *a < *b for integer types)
#define DEFINE_MINHEAP(name, type, ...)					\
									\
	static bool _##name##_less_than(type *a, type *b)		\
	{								\
		return (__VA_ARGS__);					\
	}								\
									\
	static inline void _##name##_swap(type *arr, size_t i, size_t j) \
	{								\
		type tmp = arr[i];					\
		arr[i] = arr[j];					\
		arr[j] = tmp;						\
	}								\
									\
	static void _##name##_sift_down_bottom_up(type *arr, size_t n, size_t i) \
	{								\
		size_t start = i;					\
		for (;;) {						\
			size_t left = _heap_get_left_child(i);		\
			size_t right = _heap_get_right_child(i);	\
			if (right >= n) {				\
				if (left == n - 1 && _##name##_less_than(&arr[left], &arr[i])) { \
					_##name##_swap(arr, left, i);	\
					return;				\
				}					\
				break;					\
			}						\
			size_t smallest = left;				\
			if (_##name##_less_than(&arr[right], &arr[left])) { \
				smallest = right;			\
			}						\
			_##name##_swap(arr, i, smallest);		\
			i = smallest;					\
		}							\
									\
		while (i != start) {					\
			size_t parent = _heap_get_parent(i);		\
			if (_##name##_less_than(&arr[parent], &arr[i])) { \
				break;					\
			}						\
			_##name##_swap(arr, i, parent);			\
			i = parent;					\
		}							\
	}								\
									\
	static void _##name##_sift_down(type *arr, size_t n, size_t i)	\
	{								\
		for (;;) {						\
			size_t left = _heap_get_left_child(i);		\
			size_t right = _heap_get_right_child(i);	\
			size_t smallest = i;				\
			if (left < n && _##name##_less_than(&arr[left], &arr[smallest])) { \
				smallest = left;			\
			}						\
			if (right < n && _##name##_less_than(&arr[right], &arr[smallest])) { \
				smallest = right;			\
			}						\
			if (smallest == i) {				\
				break;					\
			}						\
			_##name##_swap(arr, i, smallest);		\
			i = smallest;					\
		}							\
	}								\
									\
	static void _##name##_sift_up(type *arr, size_t i)		\
	{								\
		while (i != 0) {					\
			size_t parent = _heap_get_parent(i);		\
			if (_##name##_less_than(&arr[parent], &arr[i])) { \
				break;					\
			}						\
			_##name##_swap(arr, i, parent);			\
			i = parent;					\
		}							\
	}								\
									\
	static void name##_heapify(type *arr, size_t n)			\
	{								\
		for (size_t i = n / 2; i-- > 0;) {			\
			/* sift_down_bottom_up does fewer comparisons but more swaps, which seems to be more expensive for a heap of integers */ \
			_##name##_sift_down(arr, n, i);			\
		}							\
	}								\
									\
	static void name##_insert(type *arr, size_t n)			\
	{								\
		assert(n != 0);						\
		_##name##_sift_up(arr, n - 1);				\
	}								\
									\
	static void name##_delete(type *arr, size_t n, size_t i)	\
	{								\
		assert(i < n);						\
		if (i == n - 1) {					\
			return;						\
		}							\
		bool increase = i == 0 || _##name##_less_than(&arr[i], &arr[n - 1]); \
		arr[i] = arr[n - 1];					\
		if (increase) {						\
			_##name##_sift_down_bottom_up(arr, n, i);	\
		} else {						\
			_##name##_sift_up(arr, i);			\
		}							\
	}								\
									\
	static void name##_delete_min(type *arr, size_t n)		\
	{								\
		assert(n != 0);						\
		arr[0] = arr[n - 1];					\
		_##name##_sift_down_bottom_up(arr, n - 1, 0);		\
	}								\
									\
	static type name##_extract_min(type *arr, size_t n)		\
	{								\
		assert(n != 0);						\
		type result = arr[0];					\
		name##_delete_min(arr, n);				\
		return result;						\
	}								\
									\
	static void name##_decrease_key(type *arr, size_t n, size_t i)	\
	{								\
		(void)n;						\
		assert(i < n);						\
		_##name##_sift_up(arr, i);				\
	}								\
									\
	static void name##_increase_key(type *arr, size_t n, size_t i)	\
	{								\
		assert(i < n);						\
		/* TODO which sift_down implementation is better here? */ \
		/* _##name##_sift_down(arr, n, i); */			\
		_##name##_sift_down_bottom_up(arr, n, i);		\
	}								\
									\
	static size_t name##_is_heap_until(type *arr, size_t n)		\
	{								\
		for (size_t i = 1; i < n; i++) {			\
			if (_##name##_less_than(&arr[i], &arr[_heap_get_parent(i)])) { \
				return i;				\
			}						\
		}							\
		return n;						\
	}								\
									\
	static bool name##_is_heap(type *arr, size_t n)			\
	{								\
		return name##_is_heap_until(arr, n) == n;		\
	}								\
									\
	static void name##_sort(type *arr, size_t n)			\
	{								\
		for (size_t i = 0; i < n; i++) {			\
			arr[n - 1 - i] = name##_extract_min(arr, n - i); \
		}							\
	}								\



static inline size_t _heap_get_parent(size_t index)
{
	return (index - 1) / 2;
}

static inline size_t _heap_get_left_child(size_t index)
{
	return 2 * index + 1;
}

static inline size_t _heap_get_right_child(size_t index)
{
	return 2 * index + 2;
}

#endif
