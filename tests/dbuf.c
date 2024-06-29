#include <string.h>
#include "config.h"
#include "dbuf.h"
#include "testing.h"

#ifdef HAVE_MALLOC_USABLE_SIZE
#include <malloc.h>
#endif

static bool check_size_and_capacity(const struct dbuf *dbuf)
{
	CHECK(dbuf_capacity(dbuf) >= dbuf_size(dbuf));
	CHECK(dbuf_available_size(dbuf) == dbuf_capacity(dbuf) - dbuf_size(dbuf));
#ifdef HAVE_MALLOC_USABLE_SIZE
	CHECK(malloc_usable_size(dbuf_buffer(dbuf)) >= dbuf_capacity(dbuf));
#endif
	return true;
}

SIMPLE_TEST(dbuf)
{
	struct dbuf dbuf;
	dbuf_init(&dbuf);
	dbuf_add_str(&dbuf, "");
	CHECK(dbuf_size(&dbuf) == 0);
	const char *string = "agdfhgdsio89th4389fcn82fugu";
	dbuf_add_str(&dbuf, string);
	dbuf_add_byte(&dbuf, 0);
	CHECK(strcmp(dbuf_buffer(&dbuf), string) == 0);
	CHECK(check_size_and_capacity(&dbuf));
	dbuf_destroy(&dbuf);
	dbuf = DBUF_INITIALIZER;
	for (const char *s = string; *s; s++) {
		dbuf_add_byte(&dbuf, *s);
	}
	dbuf_add_byte(&dbuf, 0);
	CHECK(strcmp(dbuf_buffer(&dbuf), string) == 0);
	CHECK(check_size_and_capacity(&dbuf));
	dbuf_destroy(&dbuf);

	dbuf_init(&dbuf);
	char *p1 = dbuf_add_uninitialized(&dbuf, 123);
	CHECK(dbuf_size(&dbuf) == 123);
	CHECK(p1 == dbuf_buffer(&dbuf));
	char *p2 = dbuf_add_uninitialized(&dbuf, 123);
	CHECK(dbuf_size(&dbuf) == 2 * 123);
	CHECK(p2 == (char *)dbuf_buffer(&dbuf) + 123);
	char *p3 = dbuf_add_uninitialized(&dbuf, 0);
	CHECK(dbuf_size(&dbuf) == 2 * 123);
	CHECK(p3 == (char *)dbuf_buffer(&dbuf) + 2 * 123);
	CHECK(check_size_and_capacity(&dbuf));
	void *p = dbuf_finalize(&dbuf);
#ifdef HAVE_MALLOC_USABLE_SIZE
	CHECK(malloc_usable_size(p) >= 2 * 123);
#endif
	memset(p, 0, 2 * 123);
	free(p);
	CHECK(dbuf_size(&dbuf) == 0);
	CHECK(dbuf_capacity(&dbuf) == 0);
	CHECK(dbuf_buffer(&dbuf) == NULL);

	int integers[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
	dbuf_add_buf(&dbuf, integers, sizeof(integers));
	CHECK(dbuf_size(&dbuf) == sizeof(integers));
	CHECK(check_size_and_capacity(&dbuf));
	struct dbuf dbuf2 = DBUF_INITIALIZER;
	memcpy(dbuf_add_uninitialized(&dbuf2, sizeof(integers)), integers, sizeof(integers));
	CHECK(dbuf_size(&dbuf2) == dbuf_size(&dbuf));
	CHECK(check_size_and_capacity(&dbuf2));
	struct dbuf dbuf3 = dbuf_copy(&dbuf);
	CHECK(dbuf_size(&dbuf3) == dbuf_size(&dbuf));
	CHECK(dbuf_capacity(&dbuf3) == dbuf_capacity(&dbuf));
	CHECK(dbuf_buffer(&dbuf3) != dbuf_buffer(&dbuf));
	CHECK(check_size_and_capacity(&dbuf3));
	CHECK(memcmp(dbuf_buffer(&dbuf), integers, sizeof(integers)) == 0);
	CHECK(memcmp(dbuf_buffer(&dbuf2), integers, sizeof(integers)) == 0);
	CHECK(memcmp(dbuf_buffer(&dbuf3), integers, sizeof(integers)) == 0);
	dbuf_clear(&dbuf);
	CHECK(dbuf_size(&dbuf) == 0);
	CHECK(dbuf_capacity(&dbuf) == dbuf_capacity(&dbuf3));
	dbuf_add_dbuf(&dbuf, &dbuf2);
	CHECK(dbuf_size(&dbuf) == dbuf_size(&dbuf2));
	CHECK(dbuf_size(&dbuf) == dbuf_size(&dbuf3));
	CHECK(dbuf_capacity(&dbuf) == dbuf_capacity(&dbuf3));
	CHECK(memcmp(dbuf_buffer(&dbuf), integers, sizeof(integers)) == 0);
	CHECK(check_size_and_capacity(&dbuf));
	CHECK(check_size_and_capacity(&dbuf2));
	CHECK(check_size_and_capacity(&dbuf3));
	dbuf_destroy(&dbuf);
	dbuf_destroy(&dbuf2);
	dbuf_destroy(&dbuf3);

	dbuf_init(&dbuf);
	dbuf_add_fmt(&dbuf, "%d %s %c", 123, "abc", '!');
	CHECK(check_size_and_capacity(&dbuf));
	dbuf_add_byte(&dbuf, 0);
	CHECK(check_size_and_capacity(&dbuf));
	CHECK(strcmp(dbuf_buffer(&dbuf), "123 abc !") == 0);
	dbuf_destroy(&dbuf);

	dbuf_init(&dbuf);
	dbuf_reserve(&dbuf, 1000);
	CHECK(dbuf_size(&dbuf) == 0);
	CHECK(dbuf_available_size(&dbuf) >= 1000);
	CHECK(check_size_and_capacity(&dbuf));
	dbuf_shrink_to_fit(&dbuf);
	CHECK(check_size_and_capacity(&dbuf));
	for (size_t i = 0; i < 100; i++) {
		dbuf_reserve(&dbuf, 122);
		CHECK(check_size_and_capacity(&dbuf));
		void *p4 = dbuf_add_uninitialized(&dbuf, 123);
		memset(p4, 0xab, 123);
		CHECK(check_size_and_capacity(&dbuf));
		dbuf_shrink_to_fit(&dbuf);
		CHECK(check_size_and_capacity(&dbuf));
		CHECK(dbuf_size(&dbuf) == (i + 1) * 123);
		for (size_t i = 0; i < dbuf_size(&dbuf); i++) {
			CHECK(((unsigned char *)dbuf_buffer(&dbuf))[i] == 0xab);
		}
	}
	size_t capacity = dbuf_capacity(&dbuf);
	size_t size = dbuf_size(&dbuf);
	dbuf_grow(&dbuf, 0);
	CHECK(dbuf_capacity(&dbuf) == capacity);
	CHECK(dbuf_size(&dbuf) == size);
	CHECK(check_size_and_capacity(&dbuf));
	dbuf_grow(&dbuf, 123);
	CHECK(dbuf_capacity(&dbuf) >= capacity + 123);
	CHECK(dbuf_size(&dbuf) == size);
	CHECK(check_size_and_capacity(&dbuf));
	dbuf_resize(&dbuf, capacity);
	CHECK(dbuf_capacity(&dbuf) >= capacity);
	CHECK(dbuf_size(&dbuf) == size);
	CHECK(check_size_and_capacity(&dbuf));
	dbuf_resize(&dbuf, size - 1);
	CHECK(dbuf_size(&dbuf) == size - 1);
	CHECK(check_size_and_capacity(&dbuf));
	dbuf_truncate(&dbuf, size);
	CHECK(dbuf_size(&dbuf) == size - 1);
	CHECK(check_size_and_capacity(&dbuf));
	dbuf_truncate(&dbuf, 234);
	CHECK(dbuf_size(&dbuf) == 234);
	CHECK(check_size_and_capacity(&dbuf));
	dbuf_shrink_to_fit(&dbuf);
	CHECK(dbuf_size(&dbuf) == 234);
	CHECK(check_size_and_capacity(&dbuf));
	for (size_t i = 0; i < dbuf_size(&dbuf); i++) {
		CHECK(((unsigned char *)dbuf_buffer(&dbuf))[i] == 0xab);
	}
	dbuf_destroy(&dbuf);

	return true;
}
