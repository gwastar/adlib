#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>
#include "lfqueue.h"
#include "random.h"

// doesn't work yet
#define USE_NEXT_IS_NULL_AS_QUEUED_CHECK 0

void lfqueue_init(struct lfqueue *queue)
{
	atomic_init(&queue->head.next, NULL);
	atomic_init(&queue->tail, &queue->head);
}

void lfq_node_init(struct lfq_node *node)
{
	atomic_init(&node->next, NULL);
}

bool lfq_enqueue(struct lfqueue *queue, struct lfq_node *node)
{
	// self-reference: hook
#if USE_NEXT_IS_NULL_AS_QUEUED_CHECK
	struct lfq_node *expected = NULL;
	if (!atomic_compare_exchange_strong_explicit(&node->next,
						     &expected,
						     node,
						     memory_order_acq_rel,
						     memory_order_relaxed)) {
		return false;
	}
#else
	atomic_store_explicit(&node->next, node, memory_order_release);
#endif

	struct lfq_node *last, *hook;
	last = atomic_load_explicit(&queue->tail, memory_order_acquire);
	do {
		hook = atomic_load_explicit(&last->next, memory_order_acquire);
	} while (!atomic_compare_exchange_weak_explicit(&queue->tail, &last, node, memory_order_acq_rel,
							memory_order_acquire));

	// endpiece?
	if (!atomic_compare_exchange_strong_explicit(&last->next, &hook, node, memory_order_acq_rel,
						     memory_order_acquire)) {
		atomic_store_explicit(&queue->head.next, node, memory_order_release); // no longer!
	}
	return true;
}

struct lfq_node *lfq_dequeue(struct lfqueue *queue)
{
	struct lfq_node *node, *next;

	node = atomic_load_explicit(&queue->head.next, memory_order_acquire);
	do {
		if (!node) {
			return NULL;
		}
		next = atomic_load_explicit(&node->next, memory_order_acquire);
	} while (!atomic_compare_exchange_weak_explicit(&queue->head.next,
							&node,
							(next == node ? NULL : next),
							memory_order_acq_rel, memory_order_acquire));

	if (next == node) { // self-reference, is last
		struct lfq_node *n = node;
		if (!atomic_compare_exchange_strong_explicit(&node->next, &n, NULL, memory_order_acq_rel,
							     memory_order_acquire)) {
			// n is now the new node->next
			atomic_store_explicit(&queue->head.next, n, memory_order_release);
#if USE_NEXT_IS_NULL_AS_QUEUED_CHECK
			atomic_store_explicit(&node->next, NULL, memory_order_release); // mark as unqueued
#endif
		} else {
			// n is still the old node->next which is node/next
			atomic_compare_exchange_strong_explicit(&queue->tail, &n, &queue->head,
								memory_order_acq_rel, memory_order_acquire);
		}
	} else {
#if USE_NEXT_IS_NULL_AS_QUEUED_CHECK
		atomic_store_explicit(&node->next, NULL, memory_order_release); // mark as unqueued
#endif
	}

	return node;
}

bool lfq_empty(struct lfqueue *queue)
{
	return !atomic_load_explicit(&queue->head.next, memory_order_acquire);
}


#define NPTHREADS 1
#define NCTHREADS 1
#define ITERATIONS 100

struct msg {
	struct lfq_node node __attribute__((aligned(64)));
	atomic_size_t counter __attribute__((aligned(64)));
};

struct thread_data {
	struct lfqueue queue;
	pthread_barrier_t barrier;
};

static void *produce(void *arg)
{
	struct thread_data *data = arg;

	{
		int rv = pthread_barrier_wait(&data->barrier);
		assert(rv == 0 || rv == PTHREAD_BARRIER_SERIAL_THREAD);
	}

	struct msg msg;
	msg.counter = 0;
	lfq_node_init(&msg.node);

	for (size_t i = 0; i < ITERATIONS;) {
#if !USE_NEXT_IS_NULL_AS_QUEUED_CHECK
		size_t old_counter = msg.counter;
#endif
		bool success = lfq_enqueue(&data->queue, &msg.node);
#if USE_NEXT_IS_NULL_AS_QUEUED_CHECK
		if (!success) {
			continue;
		}
#else
		assert(success);
		while (atomic_load_explicit(&msg.counter, memory_order_relaxed) == old_counter);
#endif
		i++;
	}

	{
		int rv = pthread_barrier_wait(&data->barrier);
		assert(rv == 0 || rv == PTHREAD_BARRIER_SERIAL_THREAD);
	}

	assert(msg.counter == ITERATIONS);
	return NULL;
}

static void *consume(void *arg)
{
	struct thread_data *data = arg;

	{
		int rv = pthread_barrier_wait(&data->barrier);
		assert(rv == 0 || rv == PTHREAD_BARRIER_SERIAL_THREAD);
	}

	for (size_t i = 0; i < ITERATIONS * NPTHREADS;) {
		struct lfq_node *node = lfq_dequeue(&data->queue);
		if (!node) {
			continue;
		}
		struct msg *msg = (struct msg *)node;
		atomic_fetch_add_explicit(&msg->counter, 1, memory_order_relaxed);
		i++;
	}

	{
		int rv = pthread_barrier_wait(&data->barrier);
		assert(rv == 0 || rv == PTHREAD_BARRIER_SERIAL_THREAD);
	}

	return NULL;
}

int main(void)
{
	struct thread_data data;
	lfqueue_init(&data.queue);
	{
		int rv = pthread_barrier_init(&data.barrier, NULL, NPTHREADS + NCTHREADS);
		assert(rv == 0);
	}

	pthread_t producers[NPTHREADS];
	pthread_t consumers[NCTHREADS];
	for (size_t i = 0; i < sizeof(producers) / sizeof(producers[0]); i++) {
		int rv = pthread_create(&producers[i], NULL, produce, &data);
		assert(rv == 0);
	}

	for (size_t i = 0; i < sizeof(consumers) / sizeof(consumers[0]); i++) {
		int rv = pthread_create(&consumers[i], NULL, consume, &data);
		assert(rv == 0);
	}

	for (size_t i = 0; i < sizeof(producers) / sizeof(producers[0]); i++) {
		int rv = pthread_join(producers[i], NULL);
		assert(rv == 0);
	}

	for (size_t i = 0; i < sizeof(consumers) / sizeof(consumers[0]); i++) {
		int rv = pthread_join(consumers[i], NULL);
		assert(rv == 0);
	}

	puts("pass");

	return 0;
}
