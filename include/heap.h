#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "fortify.h"

// TODO document public API

// 'name' is used as a prefix for the function names
// 'type' is the element type (the heap functions operate on 'type' arrays)
// The last argument should be a code expression that compares two elements:
// The expression receives two pointers to array elements called 'a' and 'b' and
// must return true if *a is less than *b or false otherwise (i.e. it must be
// equivalent to *a < *b for integer types)
#define DEFINE_BINHEAP(name, type, ...)					\
									\
	static bool _##name##_less_than(type const *a, type const *b)	\
	{								\
		return (__VA_ARGS__);					\
	}								\
									\
	static void _##name##_swap(type *arr, size_t i, size_t j)	\
	{								\
		type tmp = arr[i];					\
		arr[i] = arr[j];					\
		arr[j] = tmp;						\
	}								\
									\
	static void _##name##_sift_up(type *arr, size_t start, size_t i) \
	{								\
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
		_##name##_sift_up(arr, start, i);			\
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
	static _attr_unused void name##_heapify(type *arr, size_t n)	\
	{								\
		for (size_t i = n / 2; i-- > 0;) {			\
			_##name##_sift_down_bottom_up(arr, n, i);	\
		}							\
	}								\
									\
	static _attr_unused void name##_insert(type *arr, size_t i)	\
	{								\
		_##name##_sift_up(arr, 0, i);				\
	}								\
									\
	static _attr_unused void name##_delete(type *arr, size_t n, size_t i) \
	{								\
		_fortify_check(i < n);					\
		if (i == n - 1) {					\
			return;						\
		}							\
		arr[i] = arr[n - 1];					\
		_##name##_sift_down_bottom_up(arr, n, i);		\
		_##name##_sift_up(arr, 0, i);				\
	}								\
									\
	static _attr_unused void name##_delete_first(type *arr, size_t n) \
	{								\
		_fortify_check(n != 0);					\
		arr[0] = arr[n - 1];					\
		_##name##_sift_down_bottom_up(arr, n - 1, 0);		\
	}								\
									\
	static _attr_unused type name##_extract_first(type *arr, size_t n) \
	{								\
		_fortify_check(n != 0);					\
		type result = arr[0];					\
		name##_delete_first(arr, n);				\
		return result;						\
	}								\
									\
	static _attr_unused void name##_sift_up(type *arr, size_t n, size_t i) \
	{								\
		(void)n;						\
		_fortify_check(i < n);					\
		_##name##_sift_up(arr, 0, i);				\
	}								\
									\
	static _attr_unused void name##_sift_down(type *arr, size_t n, size_t i) \
	{								\
		_fortify_check(i < n);					\
		_##name##_sift_down(arr, n, i);				\
	}								\
									\
	static _attr_unused size_t name##_is_heap_until(type const *arr, size_t n) \
	{								\
		for (size_t i = 1; i < n; i++) {			\
			if (_##name##_less_than(&arr[i], &arr[_heap_get_parent(i)])) { \
				return i;				\
			}						\
		}							\
		return n;						\
	}								\
									\
	static _attr_unused bool name##_is_heap(type const *arr, size_t n) \
	{								\
		return name##_is_heap_until(arr, n) == n;		\
	}								\
									\
	static _attr_unused void name##_sort(type *arr, size_t n)	\
	{								\
		for (size_t i = 0; i < n; i++) {			\
			arr[n - 1 - i] = name##_extract_first(arr, n - i); \
		}							\
	}


// TODO put _Static_assert(1, "") at the end of this macro (and other macros like this) to allow/force a
// semicolon after macro instantiation?

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
