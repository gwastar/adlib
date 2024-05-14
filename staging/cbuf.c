#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "cbuf.h"

int main(void)
{
	struct cbuf cbuf;
	size_t k, n = 16;
	bool b;
	cbuf_init(&cbuf, malloc(n), n);

	puts("---push/pop---");
	const char *str = "ABCDEFGHIHKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
	n = cbuf_push(&cbuf, str, strlen(str), false);
	printf("%lu\n", n);
	n = cbuf_push(&cbuf, str, strlen(str), false);
	printf("%lu\n", n);

	char buf[128];
	n = cbuf_pop(&cbuf, buf, 20);
	printf("%lu\n", n);
	buf[n] = 0;
	puts(buf);
	k = cbuf_pop(&cbuf, buf + n, sizeof(buf) - 1 - n);
	printf("%lu\n", k);
	buf[n + k] = 0;
	puts(buf);

	cbuf_flush(&cbuf);

	puts("---push/peek/pop---");
	n = cbuf_push(&cbuf, str, strlen(str), false);
	printf("%lu\n", n);
	k = cbuf_peek(&cbuf, buf, n);
	assert(k == n);
	buf[k] = 0;
	puts(buf);
	k = cbuf_pop(&cbuf, buf, n);
	assert(k == n);
	buf[k] = 0;
	puts(buf);

	cbuf_flush(&cbuf);

	puts("---push/skip/peek/pop---");
	n = cbuf_push(&cbuf, str, strlen(str), false);
	printf("%lu\n", n);
	b = cbuf_skip(&cbuf, n - 1);
	assert(b);
	n = cbuf_peek(&cbuf, buf, sizeof(buf));
	assert(n == 1);
	buf[1] = 0;
	puts(buf);
	b = cbuf_skip(&cbuf, 2);
	assert(!b);
	b = cbuf_skip(&cbuf, 1);
	assert(b);
	n = cbuf_pop(&cbuf, buf, sizeof(buf));
	assert(n == 0);

	cbuf_flush(&cbuf);

	puts("---pushb/peekb/popb---");
	n = 0;
	for (size_t i = 0; i < 80; i++) {
		b = cbuf_pushb(&cbuf, '0' + i, true);
		if (b) {
			n++;
		}
	}

	printf("%lu\n", n);
	if (n > cbuf.capacity) {
		n = cbuf.capacity;
	}

	for (size_t i = 0; i < n; i++) {
		char c, cc;
		bool b = cbuf_peekb(&cbuf, &c);
		assert(b);
		b = cbuf_popb(&cbuf, &cc);
		assert(b);
		assert(c == cc);
		putchar(c);
	}
	putchar('\n');

	free(cbuf_buffer(&cbuf));
	return 0;
}
