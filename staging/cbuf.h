#ifndef __cbuf_include__
#define __cbuf_include__

#include <string.h>
#include <stdbool.h>
#include <assert.h>

struct cbuf {
	char *buf;
	size_t capacity;
	size_t start;
	size_t end;
};

static void cbuf_init(struct cbuf *cbuf, void *buf, size_t capacity)
{
	assert(capacity != 0 && (capacity & (capacity - 1)) == 0);

	cbuf->buf = buf;
	cbuf->capacity = capacity;
	cbuf->start = 0;
	cbuf->end = 0;
}

static inline void *cbuf_buffer(const struct cbuf *cbuf)
{
	return cbuf->buf;
}

static inline size_t cbuf_capacity(const struct cbuf *cbuf)
{
	return cbuf->capacity;
}

static inline size_t cbuf_size(const struct cbuf *cbuf)
{
	return cbuf->end - cbuf->start;
}

static inline size_t cbuf_avail_size(const struct cbuf *cbuf)
{
	return cbuf->capacity - cbuf_size(cbuf);
}

static inline void cbuf_flush(struct cbuf *cbuf)
{
	cbuf->start = cbuf->end;
}

static bool cbuf_skip(struct cbuf *cbuf, size_t count)
{
	size_t avail = cbuf_size(cbuf);
	if (count > avail) {
		return false;
	}
	cbuf->start += count;
	return true;
}

static bool cbuf_pushb(struct cbuf *cbuf, char byte, bool overwrite)
{
	size_t avail = cbuf_avail_size(cbuf);
	if (!overwrite && avail == 0) {
		return false;
	}
	size_t index = cbuf->end & (cbuf->capacity - 1);
	cbuf->buf[index] = byte;
	cbuf->end++;
	if (cbuf_size(cbuf) > cbuf->capacity) {
		cbuf->start = cbuf->end - cbuf->capacity;
	}
	return true;
}

static bool cbuf_peekb(struct cbuf *cbuf, char *bytep)
{
	size_t avail = cbuf_size(cbuf);
	if (avail == 0) {
		return false;
	}
	size_t index = cbuf->start & (cbuf->capacity - 1);
	*bytep = cbuf->buf[index];
	return true;
}

static bool cbuf_popb(struct cbuf *cbuf, char *bytep)
{
	if (cbuf_peekb(cbuf, bytep)) {
		cbuf->start++;
		return true;
	}
	return false;
}

static void cbuf_write(struct cbuf *cbuf, size_t offset, const void *_buf, size_t count);
static void cbuf_read(const struct cbuf *cbuf, size_t offset, void *_buf, size_t count);

static size_t cbuf_push(struct cbuf *cbuf, const void *buf, size_t count, bool overwrite)
{
	size_t avail = cbuf_avail_size(cbuf);
	if (!overwrite && avail < count) {
		count = avail;
	}
	cbuf_write(cbuf, cbuf->end, buf, count);
	cbuf->end += count;
	if (cbuf_size(cbuf) > cbuf->capacity) {
		cbuf->start = cbuf->end - cbuf->capacity;
	}
	return count;
}

static size_t cbuf_peek(struct cbuf *cbuf, void *buf, size_t count)
{
	size_t avail = cbuf_size(cbuf);
	if (avail < count) {
		count = avail;
	}
	cbuf_read(cbuf, cbuf->start, buf, count);
	return count;
}

static size_t cbuf_pop(struct cbuf *cbuf, void *buf, size_t count)
{
	count = cbuf_peek(cbuf, buf, count);
	cbuf->start += count;
	return count;
}

static void cbuf_write(struct cbuf *cbuf, size_t offset, const void *_buf, size_t count)
{
	size_t skip = count > cbuf->capacity ? count - cbuf->capacity : 0;
	const char *buf = (const char *)_buf + skip;
	offset += skip;
	count -= skip;
	offset &= cbuf->capacity - 1;
	char *start = cbuf->buf + offset;
	if (offset + count > cbuf->capacity) {
		size_t n = cbuf->capacity - offset;
		memcpy(start, buf, n);
		count -= n;
		buf += n;
		start = cbuf->buf;
	}
	memcpy(start, buf, count);
}

static void cbuf_read(const struct cbuf *cbuf, size_t offset, void *_buf, size_t count)
{
	char *buf = (char *)_buf;
	offset &= cbuf->capacity - 1;
	size_t n = cbuf->capacity - offset;
	if (n >= count) {
		memcpy(buf, cbuf->buf + offset, count);
		return;
	}
	memcpy(buf, cbuf->buf + offset, n);
	buf += n;
	count -= n;
	while (count > cbuf->capacity) {
		memcpy(buf, cbuf->buf, cbuf->capacity);
		buf += cbuf->capacity;
		count -= cbuf->capacity;
	}
	memcpy(buf, cbuf->buf, count);
}

#endif
