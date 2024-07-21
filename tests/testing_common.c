#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "testing.h"

#define RED "\033[1;31m"
#define GREEN "\033[1;32m"
#define CYAN "\033[1;36m"
#define CLEAR_LINE "\033[2K\r"
#define RESET "\033[0m"

// TODO remove?
#define __GET_VAL(name, type, p) type: (p)->_##name,
#define PRIMITIVE_GET_VALUE(p, type)			\
	_Generic(*(type *)0,				\
		 __TEST_FOREACH_TYPE_WITH_ARGS(__GET_VAL, p)	\
		 struct _test_dummy_type: 0);

#define PRINT_FORMAT_FOR_TYPE(type)		\
	_Generic(*(type *)0,			\
		 bool: "%d",			\
		 char: "%c",			\
		 signed char: "%d",		\
		 unsigned char:	"%u",		\
		 short:	"%d",			\
		 unsigned short: "%u",		\
		 int: "%d",			\
		 unsigned int: "%u",		\
		 long: "%ld",			\
		 unsigned long: "%lu",		\
		 long long: "%lld",		\
		 unsigned long long: "%llu",	\
		 const void *: "%p",		\
		 const char *: "\"%s\""		\
		)

static void print_primitive_value(struct primitive *p)
{
	switch (p->type) {
#define P(name, type, p)						\
		case DATA_TYPE_##name:					\
			test_log(PRINT_FORMAT_FOR_TYPE(type), (p)->_##name); \
			break;

		__TEST_FOREACH_TYPE_WITH_ARGS(P, p);

#undef P
	default:
		assert(false);
	}
}

static void print_primitive_value_buf(char *buf, size_t bufsize, struct primitive *p)
{
	switch (p->type) {
#define P(name, type, p)						\
		case DATA_TYPE_##name:					\
			snprintf(buf, bufsize, PRINT_FORMAT_FOR_TYPE(type), (p)->_##name); \
			break;

		__TEST_FOREACH_TYPE_WITH_ARGS(P, p);

#undef P
	default:
		assert(false);
	}
}

static void print_primitive(struct primitive *var)
{
	test_log("'%s' = ", var->code);
	print_primitive_value(var);
}

static void print_primitives(struct primitive **primitives, size_t num_primitives)
{
	test_log("(");
	for (size_t i = 0; i < num_primitives; i++) {
		print_primitive(primitives[i]);
		if (i != num_primitives - 1) {
			test_log(", ");
		}
	}
	test_log(")\n");
}

static void collect_primitive(struct primitive *primitive, struct primitive **primitives, size_t *num_primitives)
{
	char buf[128];
	print_primitive_value_buf(buf, sizeof(buf), primitive);
	if (strcmp(buf, primitive->code) == 0) {
		// no point in printing the value
		return;
	}
	size_t idx = (*num_primitives)++;
	if (primitives) {
		primitives[idx] = primitive;
	}
}

static void collect_primitives(struct check *check, struct primitive **primitives,
			      size_t *num_primitives)
{
	if (!check->checked || check->passed) {
		return;
	}
	switch (check->check_type) {
	case CHECK_TYPE_AND:
	case CHECK_TYPE_OR:
		collect_primitives(check->check_args[0], primitives, num_primitives);
		collect_primitives(check->check_args[1], primitives, num_primitives);
		break;
	case CHECK_TYPE_COMPARE_EQ:
	case CHECK_TYPE_COMPARE_LT:
		collect_primitive(check->primitive_args[0], primitives, num_primitives);
		collect_primitive(check->primitive_args[1], primitives, num_primitives);
		break;
	case CHECK_TYPE_BOOL:
		// we already printed the value of the boolean
		break;
	case CHECK_TYPE_DEBUG:
		collect_primitive(check->primitive_args[0], primitives, num_primitives);
		break;
	}
}

static int compare_primitives(const void *_a, const void *_b)
{
	const struct primitive *a = *(const struct primitive **)_a;
	const struct primitive *b = *(const struct primitive **)_b;
	return strcmp(a->code, b->code);
}

static void make_unique(struct primitive **primitives, size_t *num_primitives)
{
	size_t write_idx = 1;
	for (size_t read_idx = 1; read_idx < *num_primitives; read_idx++) {
		if (strcmp(primitives[read_idx - 1]->code, primitives[read_idx]->code) != 0) {
			primitives[write_idx++] = primitives[read_idx];
		}
	}
	*num_primitives = write_idx;
}

static void print_failed_checks(struct check *check)
{
	if (!check->checked || check->passed) {
		return;
	}
	bool invert = check->invert;
	switch (check->check_type) {
	case CHECK_TYPE_AND:
		print_failed_checks(check->check_args[0]);
		print_failed_checks(check->check_args[1]);
		break;
	case CHECK_TYPE_OR:
		print_failed_checks(check->check_args[0]);
		print_failed_checks(check->check_args[1]);
		break;
	case CHECK_TYPE_COMPARE_EQ:
	case CHECK_TYPE_COMPARE_LT:
	{
		struct primitive *a = check->primitive_args[0];
		struct primitive *b = check->primitive_args[1];
		const char *cmp, *inv_cmp;
		if (check->check_type == CHECK_TYPE_COMPARE_EQ) {
			cmp = "==";
			inv_cmp = "!=";
		} else {
			assert(check->check_type == CHECK_TYPE_COMPARE_LT);
			cmp = "<";
			inv_cmp = ">=";
		}
		if (invert) {
			const char *tmp = cmp;
			cmp = inv_cmp;
			inv_cmp = tmp;
		}
		test_log("expected: %s %s %s\n", a->code, cmp, b->code);
		test_log("but got:  ");
		print_primitive_value(a);
		test_log(" %s ", inv_cmp);
		print_primitive_value(b);
		test_log("\n");
		break;
	}
	case CHECK_TYPE_BOOL:
	{
		struct primitive *p = check->primitive_args[0];
		test_log("expected '%s' to be %s, but was %s\n", p->code, invert ? "false" : "true",
			 invert ? "true" : "false");
		break;
	}
	case CHECK_TYPE_DEBUG:
		break;
	}
}

void _print_check_failure(struct check *check, const char *func, const char *file, unsigned int line)
{
	test_log("[%s:%u: %s] CHECK failed:\n", file, line, func);
	print_failed_checks(check);
	size_t num_primitives = 0;
	collect_primitives(check, NULL, &num_primitives);
	if (num_primitives == 0) {
		return;
	}
	struct primitive **primitives = malloc(num_primitives * sizeof(primitives[0]));
	num_primitives = 0;
	collect_primitives(check, primitives, &num_primitives);
	qsort(primitives, num_primitives, sizeof(primitives[0]), compare_primitives);
	make_unique(primitives, &num_primitives);
	print_primitives(primitives, num_primitives);
	free(primitives);
}

static bool eval(struct check *check)
{
	check->checked = true;
	bool passed;
	switch (check->check_type) {
	case CHECK_TYPE_AND:
		passed = eval(check->check_args[0]) && eval(check->check_args[1]);
		break;
	case CHECK_TYPE_OR:
		passed = eval(check->check_args[0]) || eval(check->check_args[1]);
		break;
	case CHECK_TYPE_COMPARE_EQ:
	case CHECK_TYPE_COMPARE_LT:
	{
		struct primitive *a = check->primitive_args[0];
		struct primitive *b = check->primitive_args[1];
		assert(a->type == b->type);
		int cmp;
		switch (a->type) {
#define E(name, ...)							\
			case DATA_TYPE_##name:				\
				cmp = a->type == DATA_TYPE_string ? strcmp(a->_string, b->_string) : \
				(a->_##name < b->_##name ? -1 : a->_##name != b->_##name); \
				break;
			__TEST_FOREACH_TYPE(E)
#undef E
		default:
			assert(false);
		}
		if (check->check_type == CHECK_TYPE_COMPARE_EQ) {
			passed = cmp == 0;
		} else {
			assert(check->check_type == CHECK_TYPE_COMPARE_LT);
			passed = cmp < 0;
		}
		break;
	}
	case CHECK_TYPE_BOOL:
	{
		struct primitive *p = check->primitive_args[0];
		switch (p->type) {
#define E(name, ...) case DATA_TYPE_##name: passed = (bool)p->_##name; break;
			__TEST_FOREACH_TYPE(E)
#undef E
		default:
			assert(false);
		}
		break;
	}
	case CHECK_TYPE_DEBUG:
		check->passed = false; // little hack so that we print it
		return true;
	}
	check->passed = passed == !check->invert;
	return check->passed;
}

bool _evaluate_check(struct check *check)
{
	return eval(check);
}
