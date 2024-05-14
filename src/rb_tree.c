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

#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>
#include "rb_tree.h"

/* From Wikipedia:
 * 1) Each node is either red or black.
 * 2) The root is black.
 * 3) All leaves (NIL) are black.
 * 4) If a node is red, then both its children are black.
 * 5) Every path from a given node to any of its descendant NIL nodes contains the same number of
 *    black nodes.
 */

#define RB_RED 0
#define RB_BLACK 1

static inline struct rb_node *_rb__parent(uintptr_t pc)
{
	return (struct rb_node *)(pc & ~1);
}

static inline uintptr_t _rb__color(uintptr_t pc)
{
	return (pc & 1);
}

struct rb_node *rb_parent(const struct rb_node *node)
{
	return _rb__parent(node->_parent_color);
}

static inline uintptr_t _rb_color(const struct rb_node *node)
{
	return _rb__color(node->_parent_color);
}

static inline void _rb_set_parent(struct rb_node *node, const struct rb_node *parent)
{
	/* assert(((uintptr_t)parent & 1) == 0); */
	node->_parent_color = _rb_color(node) | (uintptr_t)parent;
}

static inline void _rb_set_color(struct rb_node *node, uintptr_t color)
{
	/* assert(color == RB_RED || color == RB_BLACK); */
	node->_parent_color = (uintptr_t)rb_parent(node) | color;
}

static inline bool _rb_is_red(const struct rb_node *node)
{
	return _rb_color(node) == RB_RED;
}

static inline bool _rb_is_black(const struct rb_node *node)
{
	return _rb_color(node) == RB_BLACK;
}

static inline bool _rb_is_null_or_black(const struct rb_node *node)
{
	// technically null nodes are leaves and therefore black
	return !node || _rb_is_black(node);
}

static inline struct rb_node *_rb_red_parent(const struct rb_node *node)
{
	// somehow neither gcc nor clang can optimize the additional 'and' away
	// even if it knows the bottom bit is zero (red)
	/* assert(rb_is_red(node)); */
	return (struct rb_node *)node->_parent_color;
}

static inline void _rb_change_child(const struct rb_node *old_child, struct rb_node *new_child,
				    struct rb_node *parent, struct rb_tree *root)
{
	if (parent) {
		if (old_child == parent->children[RB_LEFT]) {
			parent->children[RB_LEFT] = new_child;
		} else {
			parent->children[RB_RIGHT] = new_child;
		}
	} else {
		root->root = new_child;
	}
}

struct rb_node *rb_first(const struct rb_tree *root)
{
	struct rb_node *node = NULL;
	struct rb_node *cur = root->root;
	while (cur) {
		node = cur;
		cur = cur->children[RB_LEFT];
	}
	return node;
}

struct rb_node *rb_next(const struct rb_node *node)
{
	if (node->children[RB_RIGHT]) {
		node = node->children[RB_RIGHT];
		while (node->children[RB_LEFT]) {
			node = node->children[RB_LEFT];
		}
		return (struct rb_node *)node;
	}

	struct rb_node *parent = rb_parent(node);
	while (parent && node == parent->children[RB_RIGHT]) {
		node = parent;
		parent = rb_parent(node);
	}

	return parent;
}

static void _rb_remove_repair(struct rb_tree *root, struct rb_node *parent)
{
	// we only use node at the start to figure out which child node is,
	// since we are always the left child on the first iteration this is fine
	struct rb_node *node = NULL;
	for (;;) {
		enum rb_direction dir = RB_LEFT;
		struct rb_node *sibling = parent->children[RB_RIGHT];
		if (node == sibling) {
			dir = RB_RIGHT;
			sibling = parent->children[RB_LEFT];
		}

		enum rb_direction left_dir = dir;
		enum rb_direction right_dir = 1 - dir;

		if (_rb_is_red(sibling)) {
			// rotate left(/right) at parent
			struct rb_node *tmp = sibling->children[left_dir];
			parent->children[right_dir] = tmp;
			/* assert(rb_color(parent->children[right_dir]) == RB_BLACK); */
			_rb_set_parent(parent->children[right_dir], parent);
			sibling->children[left_dir] = parent;
			_rb_change_child(parent, sibling, rb_parent(parent), root);
			sibling->_parent_color = parent->_parent_color;
			_rb_set_parent(parent, sibling);
			_rb_set_color(parent, RB_RED);
			sibling = tmp;
			_rb_set_color(parent, RB_RED);
		}

		if (_rb_is_null_or_black(sibling->children[RB_RIGHT]) &&
		    _rb_is_null_or_black(sibling->children[RB_LEFT])) {
			/* assert(rb_parent(sibling) == parent); */
			_rb_set_color(sibling, RB_RED);
			if (_rb_is_red(parent)) {
				_rb_set_color(parent, RB_BLACK);
			} else {
				node = parent;
				parent = rb_parent(node);
				if (parent) {
					continue;
				}
			}
			break;
		}

		if (_rb_is_null_or_black(sibling->children[right_dir])) {
			// rotate right at sibling
			struct rb_node *tmp = sibling->children[left_dir];
			sibling->children[left_dir] = tmp->children[right_dir];
			if (sibling->children[left_dir]) {
				/* assert(rb_is_black(sibling->children[left_dir])); */
				_rb_set_parent(sibling->children[left_dir], sibling);
			}
			tmp->children[right_dir] = sibling;
			parent->children[right_dir] = tmp;
			_rb_set_parent(sibling, tmp);
			sibling = tmp;
		}
		// rotate left at parent
		parent->children[right_dir] = sibling->children[left_dir];
		if (parent->children[right_dir]) {
			_rb_set_parent(parent->children[right_dir], parent);
		}
		sibling->children[left_dir] = parent;
		_rb_change_child(parent, sibling, rb_parent(parent), root);
		sibling->_parent_color = parent->_parent_color;
		_rb_set_parent(parent, sibling);
		_rb_set_color(sibling->children[right_dir], RB_BLACK);
		/* assert(rb_parent(sibling->children[right_dir]) == sibling); */
		_rb_set_color(parent, RB_BLACK);

		break;
	}
}

void rb_remove_node(struct rb_tree *root, struct rb_node *node)
{
	struct rb_node *child = node->children[RB_RIGHT];
	struct rb_node *tmp = node->children[RB_LEFT];
	struct rb_node *rebalance;
	uintptr_t pc;

	// removal + trivial repairs
	if (!tmp) {
		pc = node->_parent_color;
		struct rb_node *parent = _rb__parent(pc);
		_rb_change_child(node, child, parent, root);
		if (child) {
			child->_parent_color = pc;
			rebalance = NULL;
		} else {
			rebalance = (_rb__color(pc) == RB_BLACK) ? parent : NULL;
		}
	} else if (!child) {
		pc = node->_parent_color;
		tmp->_parent_color = pc;
		struct rb_node *parent = _rb__parent(pc);
		_rb_change_child(node, tmp, parent, root);
		rebalance = NULL;
	} else {
		struct rb_node *successor = child, *child2, *parent;

		tmp = child->children[RB_LEFT];
		if (!tmp) {
			parent = successor;
			child2 = successor->children[RB_RIGHT];
		} else {
			do {
				parent = successor;
				successor = tmp;
				tmp = tmp->children[RB_LEFT];
			} while (tmp);
			child2 = successor->children[RB_RIGHT];
			parent->children[RB_LEFT] = child2;
			successor->children[RB_RIGHT] = child;
			_rb_set_parent(child, successor);
		}

		tmp = node->children[RB_LEFT];
		successor->children[RB_LEFT] = tmp;
		_rb_set_parent(tmp, successor);

		pc = node->_parent_color;
		tmp = _rb__parent(pc);
		_rb_change_child(node, successor, tmp, root);

		if (child2) {
			_rb_set_color(child2, RB_BLACK);
			_rb_set_parent(child2, parent);
			rebalance = NULL;
		} else {
			rebalance = _rb_is_black(successor) ? parent : NULL;
		}
		successor->_parent_color = pc;
	}

	if (rebalance) {
		_rb_remove_repair(root, rebalance);
	}
}

void rb_insert_node(struct rb_tree *root, struct rb_node *node, struct rb_node *parent, enum rb_direction dir)
{
	assert(((uintptr_t)node & 1) == 0);
	node->children[RB_LEFT] = NULL;
	node->children[RB_RIGHT] = NULL;
	node->_parent_color = 0; // silence warning
	_rb_set_parent(node, parent);
	if (!parent) {
		_rb_set_color(node, RB_BLACK);
		root->root = node;
		return;
	}
	_rb_set_color(node, RB_RED);
	parent->children[dir] = node;

	// repair (generic)
	for (;;) {
		if (_rb_is_black(parent)) {
			break;
		}

		struct rb_node *grandparent = _rb_red_parent(parent);
		dir = RB_RIGHT;
		struct rb_node *uncle = grandparent->children[RB_LEFT];
		if (parent == uncle) {
			dir = RB_LEFT;
			uncle = grandparent->children[RB_RIGHT];
		}

		if (_rb_is_null_or_black(uncle)) {
			enum rb_direction left_dir = dir;
			enum rb_direction right_dir = 1 - dir;
			if (node == parent->children[right_dir]) {
				// rotate left(/right) at parent
				parent->children[right_dir] = node->children[left_dir];
				if (parent->children[right_dir]) {
					_rb_set_parent(parent->children[right_dir], parent);
				}
				node->children[left_dir] = parent;
				grandparent->children[left_dir] = node;
				// rb_set_parent(node, grandparent); we overwrite this later anyway
				_rb_set_parent(parent, node);
				parent = node;
			}

			// rotate right(/left) at grandparent
			grandparent->children[left_dir] = parent->children[right_dir];
			if (grandparent->children[left_dir]) {
				_rb_set_parent(grandparent->children[left_dir], grandparent);
			}
			parent->children[right_dir] = grandparent;

			struct rb_node *greatgrandparent = rb_parent(grandparent);
			_rb_change_child(grandparent, parent, greatgrandparent, root);

			_rb_set_parent(parent, greatgrandparent);
			_rb_set_color(parent, RB_BLACK);

			_rb_set_parent(grandparent, parent);
			_rb_set_color(grandparent, RB_RED);

			break;
		}

		_rb_set_color(parent, RB_BLACK);
		_rb_set_color(uncle, RB_BLACK);
		_rb_set_color(grandparent, RB_RED);
		node = grandparent;
		parent = _rb_red_parent(node);

		if (!parent) {
			_rb_set_color(node, RB_BLACK);
			break;
		}
	}
}
