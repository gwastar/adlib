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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "array.h"
#include "macros.h"

void *_arr_resize_internal(void *arr, size_t elem_size, size_t capacity)
{
	if (unlikely(capacity == 0)) {
		if (arr) {
			free(_arrhead(arr));
		}
		return NULL;
	}
	size_t new_size = sizeof(_arr) + (capacity * elem_size);
	// TODO should this check always be enabled even without fortify?
	_fortify_check(((capacity * elem_size) / elem_size == capacity) && new_size > sizeof(_arr));
	_arr *head;
	if (unlikely(!arr)) {
		head = malloc(new_size);
		if (unlikely(!head)) {
			abort();
		}
		head->length = 0;
#ifdef __FORTIFY_ENABLED
		head->magic1 = ARRAY_MAGIC1;
		head->magic2 = ARRAY_MAGIC2;
#endif
	} else {
		head = _arrhead(arr);
		if (unlikely(capacity == head->capacity)) {
			return arr;
		}
		head = realloc(head, new_size);
		if (unlikely(!head)) {
			abort();
		}
		if (unlikely(head->length > capacity)) {
			head->length = capacity;
		}
	}
	head->capacity = capacity;
	return head + 1;
}

void *_arr_copy(const void *arr, size_t elem_size)
{
	void *new_arr = _arr_resize_internal(NULL, elem_size, _arr_capacity(arr));
	if (unlikely(!new_arr)) {
		return NULL;
	}
	_arrhead(new_arr)->length = _arr_length(arr);
	memcpy(new_arr, arr, elem_size * _arr_length(arr));
	return new_arr;
}

bool _arr_equal(const void *arr1, size_t elem_size, const void *arr2)
{
	if (unlikely(arr1 == arr2)) {
		// the code below does not cover the case where both are NULL!
		return true;
	}
	size_t len = _arr_length(arr1);
	if (len != _arr_length(arr2)) {
		return false;
	}
	return memcmp(arr1, arr2, len * elem_size) == 0;
}

void _arr_grow(void **arrp, size_t elem_size, size_t n)
{
	if (unlikely(n == 0)) {
		return;
	}
	size_t capacity = _arr_capacity(*arrp);
	// TODO should this check always be enabled even without fortify?
	_fortify_check(SIZE_MAX - n >= capacity);
	const size_t numerator = ARRAY_GROWTH_FACTOR_NUMERATOR;
	const size_t denominator = ARRAY_GROWTH_FACTOR_DENOMINATOR;
	size_t new_capacity = (capacity + denominator - 1) / denominator * numerator;
	if (unlikely(new_capacity < capacity + n)) {
		new_capacity = capacity + n;
	}
	if (unlikely(new_capacity < ARRAY_INITIAL_SIZE)) {
		new_capacity = ARRAY_INITIAL_SIZE;
	}
	*arrp = _arr_resize_internal(*arrp, elem_size, new_capacity);
}

void _arr_make_valid(void **arrp, size_t elem_size, size_t i)
{
	size_t capacity = _arr_capacity(*arrp);
	if (i >= capacity) {
		_arr_grow(arrp, elem_size, i - capacity + 1);
	}
	if (i >= _arr_length(*arrp)) {
		// *arrp cannot be null here
		_arrhead(*arrp)->length = i + 1;
	}
}

void *_arr_addn(void **arrp, size_t elem_size, size_t n)
{
	if (unlikely(n == 0)) {
		return NULL;
	}
	if (unlikely(!(*arrp))) {
		_arr_grow(arrp, elem_size, n);
		_arrhead(*arrp)->length = n;
		return *arrp;
	}
	_arr *head = _arrhead(*arrp);
	size_t old_len = head->length;
	head->length += n;
	if (unlikely(head->length > head->capacity)) {
		_arr_grow(arrp, elem_size, head->length - head->capacity);
	}
	return (char *)(*arrp) + (old_len * elem_size);
}

void *_arr_insertn(void **arrp, size_t elem_size, size_t i, size_t n)
{
	void *arr = *arrp;
	size_t len = _arr_length(arr);
	_fortify_check(i <= len);
	if (unlikely(n == 0)) {
		return NULL;
	}
	if (i == len) {
		return _arr_addn(arrp, elem_size, n);
	}
	_arr_addn(&arr, elem_size, n);
	char *src = (char *)arr + i * elem_size;
	char *dst = (char *)arr + (i + n) * elem_size;
	memmove(dst, src, (len - i) * elem_size);
	*arrp = arr;
	return src;
}

void _arr_ordered_deleten(void *arr, size_t elem_size, size_t i, size_t n)
{
	size_t len = _arr_length(arr);
	_fortify_check(i < len && n <= len && (i + n) <= len);
	char *src = (char *)arr + (i + n) * elem_size;
	char *dst = (char *)arr + i * elem_size;
	memmove(dst, src, (len - (i + n)) * elem_size);
	_arrhead(arr)->length -= n;
}

void _arr_fast_deleten(void *arr, size_t elem_size, size_t i, size_t n)
{
	size_t len = _arr_length(arr);
	_fortify_check(i < len && n <= len && (i + n) <= len);
	size_t k = len - (i + n);
	if (k > n) {
		k = n;
	}
	char *src = (char *)arr + (len - k) * elem_size;
	char *dst = (char *)arr + i * elem_size;
	memmove(dst, src, k * elem_size);
	_arrhead(arr)->length -= n;
}

void _arr_sort(void *arr, size_t elem_size, int (*compare)(const void *, const void *))
{
	size_t len = _arr_length(arr);
	if (likely(len != 0)) {
		qsort(arr, len, elem_size, compare);
	}
}

bool _arr_bsearch_index(const void *arr, size_t elem_size, const void *key,
			int (*compare)(const void *, const void *), size_t *ret_index)
{
	size_t start = 0;
	size_t end = _arr_length(arr);
	while (end > start) {
		size_t idx = start + (end - start) / 2;
		int cmp = compare(key, (char *)arr + idx * elem_size);
		if (cmp < 0) {
			end = idx;
		} else if (cmp > 0) {
			start = idx + 1;
		} else {
			*ret_index = idx;
			return true;
		}
	}
	*ret_index = start;
	return false;
}

void *_arr_bsearch(const void *arr, size_t elem_size, const void *key,
		   int (*compare)(const void *, const void *))
{
	size_t idx;
	if (!_arr_bsearch_index(arr, elem_size, key, compare, &idx)) {
		return NULL;
	}
	return (char *)arr + idx * elem_size;
}
