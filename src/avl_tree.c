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
#include "avl_tree.h"

static inline int _avl_d2b(enum avl_direction dir)
{
	/* assert(dir == AVL_LEFT || dir == AVL_RIGHT); */
	return 2 * dir - 1;
}

static inline enum avl_direction _avl_b2d(int balance)
{
	/* assert(balance == -1 || balance == 1); */
	return (balance + 1) / 2;
}

inline struct avl_node *avl_parent(const struct avl_node *node)
{
	return (struct avl_node *)(node->_parent_balance & ~0x3);
}

static inline int _avl_balance(const struct avl_node *node)
{
	return (int)(node->_parent_balance & 0x3) - 1;
}

static inline void _avl_set_parent(struct avl_node *node, const struct avl_node *parent)
{
	/* assert(((uintptr_t)parent & 0x3) == 0); */
	node->_parent_balance = (node->_parent_balance & 0x3) | (uintptr_t)parent;
}

static inline void _avl_set_balance(struct avl_node *node, int balance)
{
	/* assert(-1 <= balance && balance <= 1); */
	node->_parent_balance = (node->_parent_balance & ~0x3) | (balance + 1);
}

static inline void _avl_change_child(const struct avl_node *old_child, struct avl_node *new_child,
				     struct avl_node *parent, struct avl_tree *root)
{
	if (parent) {
		if (old_child == parent->children[AVL_LEFT]) {
			parent->children[AVL_LEFT] = new_child;
		} else {
			/* assert(old_child == parent->children[AVL_RIGHT]); */
			parent->children[AVL_RIGHT] = new_child;
		}
	} else {
		root->root = new_child;
	}
}

static inline enum avl_direction _avl_dir_of_child(const struct avl_node *child, const struct avl_node *parent)
{
	if (child == parent->children[AVL_LEFT]) {
		return AVL_LEFT;
	} else {
		/* assert(child == parent->children[AVL_RIGHT]); */
		return AVL_RIGHT;
	}
}

static struct avl_node *_avl_single_rotate(struct avl_node *node, enum avl_direction dir)
{
	enum avl_direction left_dir = dir;
	enum avl_direction right_dir = 1 - dir;
	struct avl_node *child = node->children[right_dir];
	node->children[right_dir] = child->children[left_dir];
	if (node->children[right_dir]) {
		_avl_set_parent(node->children[right_dir], node);
	}
	child->children[left_dir] = node;
	_avl_set_parent(node, child);

#if 0
	if (avl_balance(child) == 0) {
		avl_set_balance(node, avl_d2b(right_dir));
		avl_set_balance(child, avl_d2b(left_dir));
	} else {
		avl_set_balance(node, 0);
		avl_set_balance(child, 0);
	}
#else
	int balance = (~_avl_balance(child) & 1) * _avl_d2b(right_dir);
	_avl_set_balance(node, balance);
	_avl_set_balance(child, -balance);
#endif
	return child;
}

static struct avl_node *_avl_double_rotate(struct avl_node *node, enum avl_direction dir)
{
	enum avl_direction left_dir = dir;
	enum avl_direction right_dir = 1 - dir;
	struct avl_node *child = node->children[right_dir];
	struct avl_node *left = child->children[left_dir];
	node->children[right_dir] = left->children[left_dir];
	if (node->children[right_dir]) {
		_avl_set_parent(node->children[right_dir], node);
	}
	child->children[left_dir] = left->children[right_dir];
	if (child->children[left_dir]) {
		_avl_set_parent(child->children[left_dir], child);
	}
	left->children[left_dir] = node;
	_avl_set_parent(node, left);
	left->children[right_dir] = child;
	_avl_set_parent(child, left);

#if 1
	int d = _avl_balance(left) - _avl_d2b(right_dir);
	d = (d | (d >> 1)) & 1;
	int x = _avl_d2b(d * right_dir + (1 - d) * left_dir);
	int b = _avl_balance(left) & 1;
	int node_balance = b * ((1 - d) * x);
	int child_balance = b * (d * x);
	_avl_set_balance(node, node_balance);
	_avl_set_balance(child, child_balance);
#else
	if (avl_balance(left) == 0) {
		avl_set_balance(node, 0);
		avl_set_balance(child, 0);
	} else if (avl_balance(left) == avl_d2b(right_dir)) {
		avl_set_balance(node, avl_d2b(left_dir));
		avl_set_balance(child, 0);
	} else {
		avl_set_balance(node, 0);
		avl_set_balance(child, avl_d2b(right_dir));
	}
#endif

	_avl_set_balance(left, 0);
	return left;
}

static struct avl_node *_avl_rotate(struct avl_node *node, enum avl_direction dir)
{
	/* assert(dir == AVL_LEFT || dir == AVL_RIGHT); */
	enum avl_direction left_dir = dir;
	enum avl_direction right_dir = 1 - dir;
	struct avl_node *child = node->children[right_dir];
	if (_avl_balance(child) == _avl_d2b(left_dir)) {
		node = _avl_double_rotate(node, left_dir);
	} else {
		node = _avl_single_rotate(node, left_dir);
	}
	return node;
}

struct avl_node *avl_first(const struct avl_tree *root)
{
	struct avl_node *node = NULL;
	struct avl_node *cur = root->root;
	while (cur) {
		node = cur;
		cur = cur->children[AVL_LEFT];
	}
	return node;
}

struct avl_node *avl_next(const struct avl_node *node)
{
	if (node->children[AVL_RIGHT]) {
		node = node->children[AVL_RIGHT];
		while (node->children[AVL_LEFT]) {
			node = node->children[AVL_LEFT];
		}
		return (struct avl_node *)node;
	}

	struct avl_node *parent = avl_parent(node);
	while (parent && node == parent->children[AVL_RIGHT]) {
		node = parent;
		parent = avl_parent(node);
	}

	return parent;
}

void avl_remove_node(struct avl_tree *root, struct avl_node *node)
{
	struct avl_node *child;
	struct avl_node *parent;
	enum avl_direction dir;

	if (!node->children[AVL_LEFT]) {
		child = node->children[AVL_RIGHT];
	} else if (!node->children[AVL_RIGHT]) {
		child = node->children[AVL_LEFT];
	} else {
		// replace with the leftmost node in the right subtree
		// or the rightmost node in the left subtree depending
		// on which subtree is higher
		int balance = _avl_balance(node);
		dir = balance == 0 ? AVL_RIGHT : _avl_b2d(balance);

		child = node->children[dir];
		parent = node;

		if (child->children[1 - dir]) {
			dir = 1 - dir;
			while (child->children[dir]) {
				parent = child;
				child = child->children[dir];
			}
		}

		_avl_change_child(node, child, avl_parent(node), root);
		child->_parent_balance = node->_parent_balance;

		enum avl_direction left_dir = dir;
		enum avl_direction right_dir = 1 - dir;

		struct avl_node *right = child->children[right_dir];

		child->children[right_dir] = node->children[right_dir];
		_avl_set_parent(child->children[right_dir], child);

		if (node == parent) {
			parent = child;
		} else {
			parent->children[left_dir] = right;
			if (parent->children[left_dir]) {
				_avl_set_parent(parent->children[left_dir], parent);
			}

			child->children[left_dir] = node->children[left_dir];
			_avl_set_parent(child->children[left_dir], child);
		}
		goto rebalance;
	}

	parent = avl_parent(node);
	if (!parent) {
		// we are removing the root with either one or no child
		if (child) {
			_avl_set_parent(child, NULL);
		}
		root->root = child;
		return;
	}
	dir = _avl_dir_of_child(node, parent);
	parent->children[dir] = child;
	if (child) {
		_avl_set_parent(child, parent);
	}

rebalance:
	for (;;) {
		struct avl_node *grandparent = avl_parent(parent);
		enum avl_direction left_dir = dir;
		enum avl_direction right_dir = 1 - dir;
		int balance = _avl_balance(parent);
		if (balance == 0) {
			_avl_set_balance(parent, _avl_d2b(right_dir));
			break;
		}
		if (balance == _avl_d2b(right_dir)) {
			balance = _avl_balance(parent->children[right_dir]);
			node = _avl_rotate(parent, left_dir);

			_avl_change_child(parent, node, grandparent, root);
			_avl_set_parent(node, grandparent);
			if (balance == 0) {
				break;
			}
		} else {
			_avl_set_balance(parent, 0);
			node = parent;
		}

		parent = grandparent;
		if (!parent) {
			break;
		}
		dir = _avl_dir_of_child(node, parent);
	}
}

void avl_insert_node(struct avl_tree *root, struct avl_node *node, struct avl_node *parent, enum avl_direction dir)
{
	assert(((uintptr_t)node & 0x3) == 0);
	node->_parent_balance = 0; // silence warning
	_avl_set_parent(node, parent);
	_avl_set_balance(node, 0);
	node->children[AVL_LEFT] = NULL;
	node->children[AVL_RIGHT] = NULL;

	if (!parent) {
		root->root = node;
		return;
	}
	parent->children[dir] = node;

	for (;;) {
		struct avl_node *grandparent = avl_parent(parent);
		enum avl_direction left_dir = dir;
		enum avl_direction right_dir = 1 - dir;
		int balance = _avl_balance(parent);
		if (balance == _avl_d2b(right_dir)) {
			_avl_set_balance(parent, 0);
			break;
		}
		if (balance == _avl_d2b(left_dir)) {
			node = _avl_rotate(parent, right_dir);
			_avl_change_child(parent, node, grandparent, root);
			_avl_set_parent(node, grandparent);
			break;
		}

		_avl_set_balance(parent, _avl_d2b(left_dir));

		node = parent;
		parent = grandparent;
		if (!parent) {
			break;
		}
		dir = _avl_dir_of_child(node, parent);
	}
}
