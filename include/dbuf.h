/*
 * Copyright (C) 2020-2022 Fabian Hügel
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

#ifndef __DBUF_INCLUDE__
#define __DBUF_INCLUDE__

#include "config.h"
#include "compiler.h"

// TODO rename dbuf_add* to dbuf_append_* ?
// TODO fortify some functions

struct dbuf {
	// do not access these fields directly
	char *_buf;
	size_t _size; // size of the valid contents of _buf
	size_t _capacity; // total size of _buf
};

#define DBUF_INITIALIZER ((struct dbuf){0})

// initialize a dynamic buffer (you can also use DBUF_INITIALIZER, this does not allocate any memory)
__AD_LINKAGE _attr_unused void dbuf_init(struct dbuf *dbuf);
// release all resources associated with dbuf (you should reinitialize dbuf before using it again)
__AD_LINKAGE _attr_unused void dbuf_destroy(struct dbuf *dbuf);
// return the internal buffer (which you need to free() eventually) and reinitialize dbuf
__AD_LINKAGE _attr_unused _attr_nodiscard void *dbuf_finalize(struct dbuf *dbuf);
// make an exact copy of dbuf (same content, same capacity, different memory)
__AD_LINKAGE _attr_unused _attr_nodiscard struct dbuf dbuf_copy(const struct dbuf *dbuf);
// return the internal buffer (which is still owned by dbuf after calling this function)
// Warning: modifications to dbuf may invalidate the returned pointer (due to realloc)
__AD_LINKAGE _attr_unused _attr_pure void *dbuf_buffer(const struct dbuf *dbuf);
// return the size of the contents of dbuf in bytes
__AD_LINKAGE _attr_unused _attr_pure size_t dbuf_size(const struct dbuf *dbuf);
// return the capacity of the internal buffer of dbuf in bytes
__AD_LINKAGE _attr_unused _attr_pure size_t dbuf_capacity(const struct dbuf *dbuf);
// return the number of bytes that can be added to dbuf without causing the internal buffer to grow
__AD_LINKAGE _attr_unused _attr_pure size_t dbuf_available_size(const struct dbuf *dbuf);
// set the size of dbuf's contents to min(new_size, dbuf_size(dbuf)) (does not change capacity)
__AD_LINKAGE _attr_unused void dbuf_truncate(struct dbuf *dbuf, size_t new_size);
// equivalent to dbuf_truncate(dbuf, 0)
__AD_LINKAGE _attr_unused void dbuf_clear(struct dbuf *dbuf);
// set the (minimum) capacity of dbuf's internal buffer
// (truncates the size to the new capacity if needed, may allocate more memory than requested)
__AD_LINKAGE _attr_unused void dbuf_resize(struct dbuf *dbuf, size_t capacity);
// set the capacity of dbuf's internal buffer to its size
__AD_LINKAGE _attr_unused void dbuf_shrink_to_fit(struct dbuf *dbuf);
// increase capacity by atleast n bytes
__AD_LINKAGE _attr_unused void dbuf_grow(struct dbuf *dbuf, size_t n);
// ensure that the internal buffer has enough capacity for at least n more bytes
// (does not change length, only capacity)
__AD_LINKAGE _attr_unused void dbuf_reserve(struct dbuf *dbuf, size_t n);
// append a byte
__AD_LINKAGE _attr_unused void dbuf_add_byte(struct dbuf *dbuf, unsigned char byte);
// append 'count' bytes of uninitialized memory and return a pointer to that memory
// Warning: modifications to dbuf may invalidate the returned pointer (due to realloc)
__AD_LINKAGE _attr_unused _attr_nodiscard void *dbuf_add_uninitialized(struct dbuf *dbuf, size_t count);
// append 'count' bytes from 'buf' ('buf' can only be NULL if 'count' is zero)
__AD_LINKAGE _attr_unused void dbuf_add_buf(struct dbuf *dbuf, const void *buf, size_t count);
// append the contents of 'other'
__AD_LINKAGE _attr_unused _attr_nonnull(2) void dbuf_add_dbuf(struct dbuf *dbuf, const struct dbuf *other);
// append a null-terminated string without the null terminator
__AD_LINKAGE _attr_unused _attr_nonnull(2) void dbuf_add_str(struct dbuf *dbuf, const char *str);
// append a formatted string (see printf for details)
__AD_LINKAGE _attr_unused _attr_format_printf(2, 3) void dbuf_add_fmt(struct dbuf *dbuf, const char *fmt, ...);

#endif
