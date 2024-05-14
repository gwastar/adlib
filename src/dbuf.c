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

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dbuf.h"

void dbuf_init(struct dbuf *dbuf)
{
	*dbuf = DBUF_INITIALIZER;
}

void dbuf_destroy(struct dbuf *dbuf)
{
	free(dbuf->_buf);
	dbuf_init(dbuf);
}

void *dbuf_finalize(struct dbuf *dbuf)
{
	void *buf = dbuf->_buf;
	dbuf_init(dbuf);
	return buf;
}

struct dbuf dbuf_copy(const struct dbuf *dbuf)
{
	struct dbuf copy;
	copy._size = dbuf->_size;
	copy._capacity = dbuf->_capacity;
	copy._buf = NULL;
	if (unlikely(dbuf->_capacity == 0)) {
		return copy;
	}
	copy._buf = malloc(dbuf->_capacity);
	if (unlikely(!copy._buf)) {
		abort();
	}
	memcpy(copy._buf, dbuf->_buf, dbuf->_size);
	return copy;
}

void *dbuf_buffer(const struct dbuf *dbuf)
{
	return dbuf->_buf;
}

size_t dbuf_size(const struct dbuf *dbuf)
{
	return dbuf->_size;
}

size_t dbuf_capacity(const struct dbuf *dbuf)
{
	return dbuf->_capacity;
}

size_t dbuf_available_size(const struct dbuf *dbuf)
{
	return dbuf->_capacity - dbuf->_size;
}

void dbuf_truncate(struct dbuf *dbuf, size_t new_size)
{
	if (likely(new_size < dbuf->_size)) {
		dbuf->_size = new_size;
	}
}

void dbuf_clear(struct dbuf *dbuf)
{
	dbuf_truncate(dbuf, 0);
}

void dbuf_resize(struct dbuf *dbuf, size_t capacity)
{
	if (unlikely(capacity == dbuf->_capacity)) {
		return;
	}
	dbuf->_buf = realloc(dbuf->_buf, capacity);
	if (unlikely(!dbuf->_buf && capacity != 0)) {
		abort();
	}
	if (unlikely(dbuf->_size > capacity)) {
		dbuf->_size = capacity;
	}
	dbuf->_capacity = capacity;
}

void dbuf_shrink_to_fit(struct dbuf *dbuf)
{
	dbuf_resize(dbuf, dbuf->_size);
}

void dbuf_grow(struct dbuf *dbuf, size_t n)
{
	if (unlikely(n == 0)) {
		return;
	}
	unsigned int capacity = dbuf_capacity(dbuf);
	unsigned int new_capacity = n < capacity ? 2 * capacity : capacity + n;
	if (unlikely(new_capacity < DBUF_INITIAL_SIZE)) {
		new_capacity = DBUF_INITIAL_SIZE;
	}
	dbuf_resize(dbuf, new_capacity);
}

void dbuf_reserve(struct dbuf *dbuf, size_t n)
{
	size_t avail = dbuf_available_size(dbuf);
	if (n > avail) {
		dbuf_grow(dbuf, n - avail);
	}
}

void dbuf_add_byte(struct dbuf *dbuf, unsigned char byte)
{
	dbuf_reserve(dbuf, 1);
	dbuf->_buf[dbuf->_size] = byte;
	dbuf->_size++;
}

void *dbuf_add_uninitialized(struct dbuf *dbuf, size_t count)
{
	dbuf_reserve(dbuf, count);
	void *p = dbuf->_buf + dbuf->_size;
	dbuf->_size += count;
	return p;
}

void (dbuf_add_buf)(struct dbuf *dbuf, const void *buf, size_t count)
{
	if (likely(count != 0)) {
		void *p = dbuf_add_uninitialized(dbuf, count);
		memcpy(p, buf, count);
	}
}

void dbuf_add_dbuf(struct dbuf *dbuf, const struct dbuf *other)
{
	dbuf_add_buf(dbuf, dbuf_buffer(other), dbuf_size(other));
}

void dbuf_add_str(struct dbuf *dbuf, const char *str)
{
	dbuf_add_buf(dbuf, str, strlen(str));
}

void dbuf_add_fmt(struct dbuf *dbuf, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	size_t n = vsnprintf(NULL, 0, fmt, args);
	va_end(args);

	dbuf_reserve(dbuf, n + 1);

	va_start(args, fmt);
	vsnprintf(dbuf->_buf + dbuf->_size, n + 1, fmt, args);
	va_end(args);

	dbuf->_size += n;
}
