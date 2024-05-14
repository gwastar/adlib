#ifndef __SPINLOCK_INCLUDE__
#define __SPINLOCK_INCLUDE__

#include <stdbool.h>
#include <stdatomic.h>
#include <threads.h>

struct ticketlock {
	atomic_uint cur;
	atomic_uint next;
};

#define TICKETLOCK_INITIALIZER {0, 0}

static void ticketlock_init(struct ticketlock *lock)
{
	lock->cur = 0;
	lock->next = 0;
}

static void ticketlock_lock(struct ticketlock *lock)
{
	unsigned int ticket = atomic_fetch_add_explicit(&lock->next, 1, memory_order_acq_rel);
	while (atomic_load_explicit(&lock->cur, memory_order_acquire) != ticket) {
		__builtin_ia32_pause();
	}
}

static void ticketlock_unlock(struct ticketlock *lock)
{
	atomic_store_explicit(&lock->cur, lock->cur + 1, memory_order_release);
}

static bool ticketlock_try_lock(struct ticketlock *lock)
{
	unsigned int expected = atomic_load_explicit(&lock->cur, memory_order_acquire);
	return atomic_compare_exchange_strong_explicit(&lock->next, &expected, lock->next + 1,
						       memory_order_acq_rel, memory_order_relaxed);
}

typedef struct _qnode * _Atomic qspinlock_t;

struct _qnode {
	struct _qnode * _Atomic next;
	atomic_bool locked;
	qspinlock_t *lock;
};

#define QSPINLOCK_INITIALIZER NULL

#define MAX_NESTED_QSPINLOCKS 16
static thread_local struct _qnode qnodes[MAX_NESTED_QSPINLOCKS];

static void qspinlock_init(qspinlock_t *lock)
{
	*lock = NULL;
}

static struct _qnode *_qspinlock_alloc_node(void)
{
	for (unsigned int i = 0; i < MAX_NESTED_QSPINLOCKS; i++) {
		if (!qnodes[i].lock) {
			return &qnodes[i];
		}
	}
	return NULL;
}

static void _qspinlock_free_node(struct _qnode *node)
{
	node->lock = NULL;
}

static void _qspinlock_lock(qspinlock_t *lock, struct _qnode *node)
{
	struct _qnode *predecessor = atomic_exchange_explicit(lock, node, memory_order_acquire);
	if (predecessor) {
		atomic_store_explicit(&node->locked, true, memory_order_relaxed);
		atomic_store_explicit(&predecessor->next, node, memory_order_release);
		while (atomic_load_explicit(&node->locked, memory_order_acquire)) {
			__builtin_ia32_pause();
		}
	}
}

static void qspinlock_lock(qspinlock_t *lock)
{
	struct _qnode *node = _qspinlock_alloc_node();
	_qspinlock_lock(lock, node);
	node->lock = lock;
}

static void _qspinlock_unlock(qspinlock_t *lock, struct _qnode *node)
{
	struct _qnode *next = atomic_load_explicit(&node->next, memory_order_acquire);
	if (!next) {
		struct _qnode *expected = node;
		if (atomic_compare_exchange_strong_explicit(lock, &expected, NULL,
							    memory_order_release, memory_order_relaxed)) {
			return;
		}
		while (!(next = atomic_load_explicit(&node->next, memory_order_acquire))) {
			__builtin_ia32_pause();
		}
	}
	atomic_store_explicit(&next->locked, false, memory_order_release);
	atomic_store_explicit(&node->next, NULL, memory_order_relaxed);
}

static void qspinlock_unlock(qspinlock_t *lock)
{
	struct _qnode *node = NULL;
	for (unsigned int i = 0; i < MAX_NESTED_QSPINLOCKS; i++) {
		if (qnodes[i].lock == lock) {
			node = &qnodes[i];
			break;
		}
	}
	_qspinlock_free_node(node);
	_qspinlock_unlock(lock, node);
}

static bool qspinlock_try_lock(qspinlock_t *lock)
{
	struct _qnode *node = _qspinlock_alloc_node();
	struct _qnode *expected = NULL;
	bool success = atomic_compare_exchange_strong_explicit(lock, &expected, node,
							       memory_order_acquire, memory_order_relaxed);
	if (!success) {
		_qspinlock_free_node(node);
	}
	return success;
}

#endif
