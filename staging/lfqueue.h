#ifndef __lfqueue_include__
#define __lfqueue_include__

// lock-free single-producer single-consumer unbounded fifo queue

#include <stdatomic.h>
#include <stdbool.h>

struct lfq_node {
	_Atomic(struct lfq_node *) next;
};

struct lfqueue {
	struct lfq_node head __attribute__((aligned(64)));
	_Atomic(struct lfq_node *) tail __attribute__((aligned(64)));
};

void lfqueue_init(struct lfqueue *queue);

void lfq_node_init(struct lfq_node *node);

// returns false if the node was already queued, otherwise true
bool lfq_enqueue(struct lfqueue *queue, struct lfq_node *node);

// returns NULL if the queue is empty
struct lfq_node *lfq_dequeue(struct lfqueue *queue);

bool lfq_empty(struct lfqueue *queue);

#endif
