#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "array.h"
#include "dstring.h"
#include "hashtable.h"
#include "macros.h"
#include "testing.h"

// TODO put some hash functions into the library
// https://github.com/PeterScott/murmur3
static uint32_t string_hash(const void *string)
{
	const void *data = string;
	const size_t nbytes = strlen(string);
	if (data == NULL || nbytes == 0) {
		return 0;
	}

	const uint32_t c1 = 0xcc9e2d51;
	const uint32_t c2 = 0x1b873593;

	const int nblocks = nbytes / 4;
	const uint32_t *blocks = (const uint32_t *)data;
	const uint8_t *tail = (const uint8_t *)data + (nblocks * 4);

	uint32_t h = 0;

	int i;
	uint32_t k;
	for (i = 0; i < nblocks; i++) {
		k = blocks[i];

		k *= c1;
		k = (k << 15) | (k >> (32 - 15));
		k *= c2;

		h ^= k;
		h = (h << 13) | (h >> (32 - 13));
		h = (h * 5) + 0xe6546b64;
	}

	k = 0;
	switch (nbytes & 3) {
	case 3:
		k ^= tail[2] << 16; _attr_fallthrough;
	case 2:
		k ^= tail[1] << 8; _attr_fallthrough;
	case 1:
		k ^= tail[0];
		k *= c1;
		k = (k << 15) | (k >> (32 - 15));
		k *= c2;
		h ^= k;
	};

	h ^= nbytes;

	h ^= h >> 16;
	h *= 0x85ebca6b;
	h ^= h >> 13;
	h *= 0xc2b2ae35;
	h ^= h >> 16;

	return h;
}

#define JSON_KEYWORD_FALSE "false"
#define JSON_KEYWORD_NULL  "null"
#define JSON_KEYWORD_TRUE  "true"
#define JSON_WHITESPACE " \t\n\r"

struct json_object_member {
	char *name;
	struct json_value *value;
};
DEFINE_HASHTABLE(json_object_members, char *, struct json_object_member, 8, strcmp(*key, entry->name) == 0)

	struct json_value {
		enum {
			JSON_OBJECT,
			JSON_ARRAY,
			JSON_NUMBER,
			JSON_STRING,
			JSON_FALSE,
			JSON_NULL,
			JSON_TRUE,
		} kind;
	};

struct json_object {
	struct json_value value;
	struct json_object_members members;
};

struct json_array {
	struct json_value value;
	array_t(struct json_value *) elements;
};

struct json_number {
	struct json_value value;
	double number;
};

struct json_string {
	struct json_value value;
	char *string;
};

static bool json_is_object(const struct json_value *value)
{
	return value->kind == JSON_OBJECT;
}

static bool json_is_array(const struct json_value *value)
{
	return value->kind == JSON_ARRAY;
}

static bool json_is_number(const struct json_value *value)
{
	return value->kind == JSON_NUMBER;
}

static bool json_is_string(const struct json_value *value)
{
	return value->kind == JSON_STRING;
}

static bool json_is_false(const struct json_value *value)
{
	return value->kind == JSON_FALSE;
}

static bool json_is_null(const struct json_value *value)
{
	return value->kind == JSON_NULL;
}

static bool json_is_true(const struct json_value *value)
{
	return value->kind == JSON_TRUE;
}

static struct json_object *json_to_object(const struct json_value *value)
{
	return json_is_object(value) ? container_of(value, struct json_object, value) : NULL;
}

static struct json_array *json_to_array(const struct json_value *value)
{
	return json_is_array(value) ? container_of(value, struct json_array, value) : NULL;
}

static struct json_number *json_to_number(const struct json_value *value)
{
	return json_is_number(value) ? container_of(value, struct json_number, value) : NULL;
}

static struct json_string *json_to_string(const struct json_value *value)
{
	return json_is_string(value) ? container_of(value, struct json_string, value) : NULL;
}

static struct json_object *json_new_object(void)
{
	struct json_object *object = malloc(sizeof(*object));
	object->value.kind = JSON_OBJECT;
	json_object_members_init(&object->members, 0);
	return object;
}

static struct json_array *json_new_array(void)
{
	struct json_array *array = malloc(sizeof(*array));
	array->value.kind = JSON_ARRAY;
	array->elements = NULL;
	return array;
}

static struct json_number *json_new_number(double d)
{
	struct json_number *number = malloc(sizeof(*number));
	number->value.kind = JSON_NUMBER;
	number->number = d;
	return number;
}

static struct json_string *json_new_string(char *str)
{
	struct json_string *string = malloc(sizeof(*string));
	string->value.kind = JSON_STRING;
	string->string = str;
	return string;
}

static struct json_value *json_new_false(void)
{
	struct json_value *value = malloc(sizeof(*value));
	value->kind = JSON_FALSE;
	return value;
}

static struct json_value *json_new_null(void)
{
	struct json_value *value = malloc(sizeof(*value));
	value->kind = JSON_NULL;
	return value;
}

static struct json_value *json_new_true(void)
{
	struct json_value *value = malloc(sizeof(*value));
	value->kind = JSON_TRUE;
	return value;
}

static bool json_value_equal(struct json_value *value1, struct json_value *value2);

static bool json_object_equal(struct json_object *object1, struct json_object *object2)
{
	if (json_object_members_num_entries(&object1->members) !=
	    json_object_members_num_entries(&object2->members)) {
		return false;
	}
	for (json_object_members_iter_t iter = json_object_members_iter_start(&object1->members);
	     !json_object_members_iter_finished(&iter);
	     json_object_members_iter_advance(&iter)) {
		// TODO shouldn't have to recompute hash here...
		struct json_object_member *found = json_object_members_lookup(&object1->members,
									      iter.entry->name,
									      string_hash(iter.entry->name));
		if (!found) {
			return false;
		}
		if (!json_value_equal(iter.entry->value, found->value)) {
			return false;
		}
	}
	return true;
}

static bool json_array_equal(struct json_array *array1, struct json_array *array2)
{
	if (array_length(array1->elements) != array_length(array2->elements)) {
		return false;
	}
	array_fori(array1->elements, i) {
		if (!json_value_equal(array1->elements[i], array2->elements[i])) {
			return false;
		}
	}
	return true;
}

static bool json_number_equal(struct json_number *number1, struct json_number *number2)
{
	return number1->number == number2->number;
}

static bool json_string_equal(struct json_string *string1, struct json_string *string2)
{
	return strcmp(string1->string, string2->string) == 0;
}

static bool json_value_equal(struct json_value *value1, struct json_value *value2)
{
	if (value1->kind != value2->kind) {
		return false;
	}
	if (json_is_object(value1)) {
		return json_object_equal(json_to_object(value1), json_to_object(value2));
	}
	if (json_is_array(value1)) {
		return json_array_equal(json_to_array(value1), json_to_array(value2));
	}
	if (json_is_number(value1)) {
		return json_number_equal(json_to_number(value1), json_to_number(value2));
	}
	if (json_is_string(value1)) {
		return json_string_equal(json_to_string(value1), json_to_string(value2));
	}
	return true;
}

static void json_value_delete(struct json_value *value);

static void json_object_delete(struct json_object *object)
{
	for (json_object_members_iter_t iter = json_object_members_iter_start(&object->members);
	     !json_object_members_iter_finished(&iter);
	     json_object_members_iter_advance(&iter)) {
		free(iter.entry->name);
		json_value_delete(iter.entry->value);
	}
	json_object_members_destroy(&object->members);
	free(object);
}

static void json_array_delete(struct json_array *array)
{
	array_foreach_value(array->elements, v) {
		json_value_delete(v);
	}
	array_free(array->elements);
	free(array);
}

static void json_number_delete(struct json_number *number)
{
	free(number);
}

static void json_string_delete(struct json_string *string)
{
	free(string->string);
	free(string);
}

static void json_value_delete(struct json_value *value)
{
	if (json_is_false(value) || json_is_null(value) || json_is_true(value)) {
		free(value);
		return;
	}
	if (json_is_number(value)) {
		json_number_delete(json_to_number(value));
		return;
	}
	if (json_is_string(value)) {
		json_string_delete(json_to_string(value));
		return;
	}
	if (json_is_array(value)) {
		json_array_delete(json_to_array(value));
		return;
	}
	json_object_delete(json_to_object(value));
}

static struct json_value *json_parse_value(struct strview *inputp);
static char *_json_parse_string(struct strview *inputp);

static struct json_object *json_parse_object(struct strview *inputp)
{
	struct strview input = strview_lstrip(*inputp, JSON_WHITESPACE);
	if (!strview_startswith_cstr(input, "{")) {
		return NULL;
	}
	input = strview_narrow(input, 1, 0);
	struct json_object *object = json_new_object();
	for (bool first = true;; first = false) {
		input = strview_lstrip(input, JSON_WHITESPACE);
		if (strview_startswith_cstr(input, "}")) {
			input = strview_narrow(input, 1, 0);
			break;
		}
		if (!first) {
			if (!strview_startswith_cstr(input, ",")) {
				goto error;
			}
			input = strview_narrow(input, 1, 0);
		}
		char *name = _json_parse_string(&input);
		if (!name) {
			goto error;
		}
		input = strview_lstrip(input, JSON_WHITESPACE);
		if (!strview_startswith_cstr(input, ":")) {
			free(name);
			goto error;
		}
		input = strview_narrow(input, 1, 0);
		struct json_value *value = json_parse_value(&input);
		if (!value) {
			free(name);
			goto error;
		}
		uint32_t name_hash = string_hash(name);
		struct json_object_member *member = json_object_members_lookup(&object->members, name, name_hash);
		if (member) {
			json_value_delete(member->value);
			member->value = value;
		} else {
			member = json_object_members_insert(&object->members, name, name_hash);
			member->name = name;
			member->value = value;
		}
	}
	*inputp = input;
	return object;

error:
	json_object_delete(object);
	return NULL;
}

static struct json_array *json_parse_array(struct strview *inputp)
{
	struct strview input = strview_lstrip(*inputp, JSON_WHITESPACE);
	if (!strview_startswith_cstr(input, "[")) {
		return NULL;
	}
	input = strview_narrow(input, 1, 0);
	struct json_array *array = json_new_array();
	for (bool first = true;; first = false) {
		input = strview_lstrip(input, JSON_WHITESPACE);
		if (strview_startswith_cstr(input, "]")) {
			input = strview_narrow(input, 1, 0);
			break;
		}
		if (!first) {
			if (!strview_startswith_cstr(input, ",")) {
				goto error;
			}
			input = strview_narrow(input, 1, 0);
		}
		struct json_value *value = json_parse_value(&input);
		if (!value) {
			goto error;
		}
		array_add(array->elements, value);
	}
	*inputp = input;
	return array;

error:
	json_array_delete(array);
	return NULL;
}

static struct json_number *json_parse_number(struct strview *inputp)
{
	struct strview input = strview_lstrip(*inputp, JSON_WHITESPACE);
	struct strview rest = input;
	if (strview_startswith_cstr(rest, "-")) {
		rest = strview_narrow(rest, 1, 0);
	}
	if (rest.length == 0 || !isdigit(rest.characters[0])) {
		return NULL;
	}
	if (rest.characters[0] == '0') {
		rest = strview_narrow(rest, 1, 0);
	} else {
		do {
			rest = strview_narrow(rest, 1, 0);
		} while (rest.length != 0 && isdigit(rest.characters[0]));
	}
	if (strview_startswith_cstr(rest, ".")) {
		rest = strview_narrow(rest, 1, 0);
		if (rest.length == 0 || !isdigit(rest.characters[0])) {
			return NULL;
		}
		do {
			rest = strview_narrow(rest, 1, 0);
		} while (rest.length != 0 && isdigit(rest.characters[0]));
	}
	if (strview_startswith_cstr(rest, "e") || strview_startswith_cstr(rest, "E")) {
		rest = strview_narrow(rest, 1, 0);
		if (strview_startswith_cstr(rest, "+") || strview_startswith_cstr(rest, "-")) {
			rest = strview_narrow(rest, 1, 0);
		}
		if (rest.length == 0 || !isdigit(rest.characters[0])) {
			return NULL;
		}
		do {
			rest = strview_narrow(rest, 1, 0);
		} while (rest.length != 0 && isdigit(rest.characters[0]));
	}
	struct strview number_string = strview_substring(input, 0, input.length - rest.length);
	char buf[128];
	if (number_string.length >= sizeof(buf)) {
		return NULL;
	}
	memcpy(buf, number_string.characters, number_string.length);
	buf[number_string.length] = '\0';
	int saved_errno = errno;
	errno = 0;
	double d = strtod(buf, NULL);
	if (errno == ERANGE) {
		return NULL;
	}
	errno = saved_errno;
	*inputp = rest;
	return json_new_number(d);
}

static char *_json_parse_string(struct strview *inputp)
{
	struct strview input = strview_lstrip(*inputp, JSON_WHITESPACE);
	if (!strview_startswith_cstr(input, "\"")) {
		return NULL;
	}
	input = strview_narrow(input, 1, 0);
	size_t end = strview_find_first_of(input, "\"", 0);
	if (end == STRVIEW_NPOS) {
		return NULL;
	}
	*inputp = strview_narrow(input, end + 1, 0);
	return strview_to_cstr(strview_substring(input, 0, end));
}

static struct json_string *json_parse_string(struct strview *inputp)
{
	char *str = _json_parse_string(inputp);
	return str ? json_new_string(str) : NULL;
}

static struct json_value *json_parse_value(struct strview *inputp)
{
	struct strview input = strview_lstrip(*inputp, JSON_WHITESPACE);
	if (strview_startswith_cstr(input, JSON_KEYWORD_FALSE)) {
		*inputp = strview_narrow(input, strlen(JSON_KEYWORD_FALSE), 0);
		return json_new_false();
	}
	if (strview_startswith_cstr(input, JSON_KEYWORD_NULL)) {
		*inputp = strview_narrow(input, strlen(JSON_KEYWORD_NULL), 0);
		return json_new_null();
	}
	if (strview_startswith_cstr(input, JSON_KEYWORD_TRUE)) {
		*inputp = strview_narrow(input, strlen(JSON_KEYWORD_TRUE), 0);
		return json_new_true();
	}
	struct json_object *object = json_parse_object(&input);
	if (object) {
		*inputp = input;
		return &object->value;
	}
	struct json_array *array = json_parse_array(&input);
	if (array) {
		*inputp = input;
		return &array->value;
	}
	struct json_number *number = json_parse_number(&input);
	if (number) {
		*inputp = input;
		return &number->value;
	}
	struct json_string *string = json_parse_string(&input);
	if (string) {
		*inputp = input;
		return &string->value;
	}
	return NULL;
}

static void json_value_print(struct json_value *value, dstr_t *dstr);

static void json_object_print(struct json_object *object, dstr_t *dstr)
{
	dstr_append_char(dstr, '{');
	bool first = true;
	for (json_object_members_iter_t iter = json_object_members_iter_start(&object->members);
	     !json_object_members_iter_finished(&iter);
	     json_object_members_iter_advance(&iter)) {
		if (!first) {
			dstr_append_char(dstr, ',');
		}
		dstr_append_fmt(dstr, "\"%s\":", iter.entry->name);
		json_value_print(iter.entry->value, dstr);
		first = false;
	}
	dstr_append_char(dstr, '}');
}

static void json_array_print(struct json_array *array, dstr_t *dstr)
{
	dstr_append_char(dstr, '[');
	array_fori(array->elements, i) {
		json_value_print(array->elements[i], dstr);
		if (i != array_lasti(array->elements)) {
			dstr_append_char(dstr, ',');
		}
	}
	dstr_append_char(dstr, ']');
}

static void json_number_print(struct json_number *number, dstr_t *dstr)
{
	dstr_append_fmt(dstr, "%g", number->number);
}

static void json_string_print(struct json_string *string, dstr_t *dstr)
{
	dstr_append_char(dstr, '"');
	dstr_append_cstr(dstr, string->string);
	dstr_append_char(dstr, '"');
}

static void json_value_print(struct json_value *value, dstr_t *dstr)
{
	if (json_is_false(value)) {
		dstr_append_cstr(dstr, JSON_KEYWORD_FALSE);
		return;
	}
	if (json_is_null(value)) {
		dstr_append_cstr(dstr, JSON_KEYWORD_NULL);
		return;
	}
	if (json_is_true(value)) {
		dstr_append_cstr(dstr, JSON_KEYWORD_TRUE);
		return;
	}
	if (json_is_object(value)) {
		json_object_print(json_to_object(value), dstr);
	}
	if (json_is_array(value)) {
		json_array_print(json_to_array(value), dstr);
	}
	if (json_is_number(value)) {
		json_number_print(json_to_number(value), dstr);
	}
	if (json_is_string(value)) {
		json_string_print(json_to_string(value), dstr);
	}
}

// static dstr_t json_object_to_string(struct json_object *object)
// {
// 	dstr_t dstr = dstr_make_empty();
// 	json_object_print(object, &dstr);
// 	return dstr;
// }

// static dstr_t json_array_to_string(struct json_array *array)
// {
// 	dstr_t dstr = dstr_make_empty();
// 	json_array_print(array, &dstr);
// 	return dstr;
// }

// static dstr_t json_number_to_string(struct json_number *number)
// {
// 	dstr_t dstr = dstr_make_empty();
// 	json_number_print(number, &dstr);
// 	return dstr;
// }

// static dstr_t json_string_to_string(struct json_string *string)
// {
// 	dstr_t dstr = dstr_make_empty();
// 	json_string_print(string, &dstr);
// 	return dstr;
// }

static dstr_t json_value_to_string(struct json_value *value)
{
	dstr_t dstr = dstr_new();
	json_value_print(value, &dstr);
	return dstr;
}

static bool test(struct strview json_string)
{
	// fwrite(json_string.characters, 1, json_string.length, stdout);
	struct json_value *value = json_parse_value(&json_string);
	CHECK(value);
	dstr_t str = json_value_to_string(value);
	// putchar('\n');
	// puts(str);
	struct strview json_string2 = dstr_view(str);
	struct json_value *value2 = json_parse_value(&json_string2);
	CHECK(value2);
	CHECK(json_value_equal(value, value2));
	dstr_free(&str);
	json_value_delete(value);
	json_value_delete(value2);
	return true;
}

SIMPLE_TEST(json)
{
	return test(strview_from_cstr("[\n\t{\n\t\t\"a\": [false, \"a\", -1234.5678e-09],\n\t\t\"b\": null,\n\t\t\"c\": true\n\t}\n]\n"));
}
