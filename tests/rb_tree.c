#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <time.h>
#include "random.h"
#include "rb_tree.h"
#include "macros.h"
#include "testing.h"

static inline bool rb_is_red(const struct rb_node *node)
{
	return (node->_parent_color & 1) == 0;
}

static inline bool rb_is_black(const struct rb_node *node)
{
	return (node->_parent_color & 1) == 1;
}

struct thing {
	int key;
	struct rb_node rb_node;
};

#define to_thing(ptr) container_of(ptr, struct thing, rb_node)

static struct rb_node *rb_find_helper(struct rb_root *root, int key, struct rb_node **ret_parent, int *ret_dir)
{
	struct rb_node *parent = NULL;
	struct rb_node *cur = root->node;
	int dir = 0;
	while (cur) {
		if (key == to_thing(cur)->key) {
			break;
		}
		dir = RB_RIGHT;
		if (key < to_thing(cur)->key) {
			dir = RB_LEFT;
		}
		parent = cur;
		cur = cur->children[dir];
	}

	if (ret_parent) {
		*ret_parent = parent;
	}

	if (ret_dir) {
		*ret_dir = dir;
	}

	return cur;
}

static inline struct rb_node *rb_find(struct rb_root *root, int key)
{
	return rb_find_helper(root, key, NULL, NULL);
}

static struct rb_node *rb_remove_key(struct rb_root *root, int key)
{
	struct rb_node *node = rb_find_helper(root, key, NULL, NULL);
	if (node) {
		rb_remove_node(root, node);
	}
	return node;
}

static bool rb_insert_key(struct rb_root *root, int key)
{
	int dir;
	struct rb_node *parent;
	if (rb_find_helper(root, key, &parent, &dir)) {
		return false;
	}

	struct thing *thing = malloc(sizeof(*thing));
	thing->key = key;
	rb_insert_node(root, &thing->rb_node, parent, dir);
	return true;
}

static void _rb_destroy(struct rb_node *node)
{
	if (!node) {
		return;
	}
	_rb_destroy(node->children[RB_LEFT]);
	_rb_destroy(node->children[RB_RIGHT]);
	free(to_thing(node));
}

static void rb_destroy_tree(struct rb_root *root)
{
	_rb_destroy(root->node);
	root->node = NULL;
}

static bool check_tree_recursive(struct rb_node *node, int cur_black_depth, int *black_depth)
{
	if (!node) {
		if (*black_depth == -1) {
			*black_depth = cur_black_depth;
		}
		CHECK(*black_depth == cur_black_depth);
		return true;
	}
	if (rb_is_black(node)) {
		cur_black_depth++;
	}
	if (node->children[RB_LEFT]) {
		CHECK(rb_parent(node->children[RB_LEFT]) == node);
		CHECK(!(rb_is_red(node) && rb_is_red(node->children[RB_LEFT])));
		CHECK(to_thing(node->children[RB_LEFT])->key < to_thing(node)->key);
	}
	if (node->children[RB_RIGHT]) {
		CHECK(rb_parent(node->children[RB_RIGHT]) == node);
		CHECK(!(rb_is_red(node) && rb_is_red(node->children[RB_RIGHT])));
		CHECK(to_thing(node->children[RB_RIGHT])->key > to_thing(node)->key);
	}
	CHECK(check_tree_recursive(node->children[RB_LEFT], cur_black_depth, black_depth));
	CHECK(check_tree_recursive(node->children[RB_RIGHT], cur_black_depth, black_depth));
	return true;
}

static bool check_tree(struct rb_root *root)
{
	if (!root->node) {
		return true;
	}
	int black_depth = -1;
	CHECK(rb_is_black(root->node));
	CHECK(!rb_parent(root->node));
	return check_tree_recursive(root->node, 0, &black_depth);
}

RANDOM_TEST(insert_find_remove, 2, 0, UINT64_MAX)
{
	const unsigned int N = 200000;
	struct rb_root root = RB_EMPTY_ROOT;
	struct random_state rng;
	random_state_init(&rng, random);
	for (unsigned int i = 0; i < N; i++) {
		int key = random_next_u32(&rng);
		rb_insert_key(&root, key);
	}
	CHECK(check_tree(&root));
	random_state_init(&rng, random);
	for (unsigned int i = 0; i < N; i++) {
		int key = random_next_u32(&rng);
		struct rb_node *node = rb_find(&root, key);
		CHECK(node && to_thing(node)->key == key);
	}
	random_state_init(&rng, random);
	for (unsigned int i = 0; i < N; i++) {
		int key = random_next_u32(&rng);
		struct rb_node *node = rb_remove_key(&root, key);
		if (node) {
			CHECK(to_thing(node)->key == key);
			free(to_thing(node));
		}
		if (i % 1024 == 0) {
			CHECK(check_tree(&root));
		}
	}
	CHECK(root.node == NULL);
	return true;
}

RANDOM_TEST(foreach, 2, 0, UINT64_MAX)
{
	struct rb_root root = RB_EMPTY_ROOT;
	struct random_state rng;
	random_state_init(&rng, random);
	for (unsigned int i = 0; i < 200000; i++) {
		int key = random_next_u32(&rng);
		rb_insert_key(&root, key);
	}
	CHECK(check_tree(&root));
	struct thing *prev = NULL;
	rb_foreach(&root, cur) {
		struct thing *thing = to_thing(cur);
		if (prev) {
			CHECK(prev->key < thing->key);
		}
		prev = thing;
	}
	rb_destroy_tree(&root);
	return true;
}

RANDOM_TEST(random_insert_find_remove, 2, 0, UINT64_MAX)
{
	struct rb_root root = RB_EMPTY_ROOT;
	struct random_state rng;
	random_state_init(&rng, random);
	for (unsigned int i = 0; i < 200000; i++) {
		const int max_key = 1024;
		{
			int key = random_next_u32(&rng) % max_key;
			bool success = rb_insert_key(&root, key);
			if (!success) {
				struct rb_node *node = rb_remove_key(&root, key);
				CHECK(node);
				free(to_thing(node));
				success = rb_insert_key(&root, key);
				CHECK(success);
			}
			CHECK(to_thing(rb_find(&root, key))->key == key);
		}
		{
			int key = random_next_u32(&rng) % max_key;
			struct rb_node *node = rb_find(&root, key);
			if (node) {
				rb_remove_node(&root, node);
				CHECK(!rb_find(&root, key));
				free(to_thing(node));
			}
		}

		if (i % 1024 == 0) {
			CHECK(check_tree(&root));
		}
	}
	CHECK(check_tree(&root));
	rb_destroy_tree(&root);
	return true;
}

#if 0
	char buf[128];
	while (fgets(buf, sizeof(buf), stdin)) {
		int key = atoi(buf + 1);
		if (buf[0] == 'f') {
			struct rb_node *node = rb_find(&root, key);
			if (node) {
				struct thing *thing = to_thing(node);
				printf("%d\n", thing->key);
			}
		} else if (buf[0] == 'i') {
			rb_insert_key(&root, key);
			struct rb_node *node = rb_find(&root, key);
			assert(to_thing(node)->key == key);
		} else if (buf[0] == 'r') {
			struct rb_node *node = rb_remove_key(&root, key);
			if (node) {
				struct thing *thing = to_thing(node);
				printf("%d\n", thing->key);
				assert(rb_find(&root, key) == NULL);
			}
		}
		debug_check_tree(&root);
	}
#endif
