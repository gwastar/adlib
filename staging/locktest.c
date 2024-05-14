#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <stdint.h>
#include <limits.h>
#include <threads.h>
#include <pthread.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>

#include "spinlock.h"
#include "semaphore.h"
// #include "mutex2.h"
#include "mutex.h"

#define NUM_THREADS 8
#define N 1000000
#define N2 10000

static qspinlock_t spinlock = QSPINLOCK_INITIALIZER;
static struct ticketlock ticketlock = TICKETLOCK_INITIALIZER;
static struct mutex mutex = MUTEX_INITIALIZER;
static struct semaphore semaphore = SEMAPHORE_INITIALIZER(0);
static pthread_mutex_t pthread_mutex = PTHREAD_MUTEX_INITIALIZER;

struct elem {
	unsigned int tid;
	unsigned int idx;
	struct elem *next;
};

static struct elem *head = NULL;

#include "array.h"

struct string {
	char s[32];
};

static struct semaphore insem = SEMAPHORE_INITIALIZER(0);
static struct semaphore outsem = SEMAPHORE_INITIALIZER(0);
static struct mutex inlock = MUTEX_INITIALIZER;
static struct mutex outlock = MUTEX_INITIALIZER;
static int *input;
static struct string *output;
static unsigned long counter = 0;

struct cbuffer {
	struct semaphore insem;
	struct semaphore outsem;
	atomic_uint in;
	atomic_uint out;
	int buffer[256];
};

static void cbuffer_init(struct cbuffer *buf)
{
	semaphore_init(&buf->insem, 256);
	semaphore_init(&buf->outsem, 0);
	buf->in = 0;
	buf->out = 0;
}

static void cbuffer_put(struct cbuffer *buf, int x)
{
	semaphore_wait(&buf->insem);

	unsigned int in = atomic_load_explicit(&buf->in, memory_order_relaxed);
	buf->buffer[in] = x;
	in = (in + 1) % 256;
	atomic_store_explicit(&buf->in, in, memory_order_relaxed);

	semaphore_post(&buf->outsem);
}

static int cbuffer_get(struct cbuffer *buf)
{
	semaphore_wait(&buf->outsem);

	unsigned int out = atomic_load_explicit(&buf->out, memory_order_relaxed);
	int ret = buf->buffer[out];
	out = (out + 1) % 256;
	atomic_store_explicit(&buf->out, out, memory_order_relaxed);

	semaphore_post(&buf->insem);

	return ret;
}

struct cbuffer buffers[NUM_THREADS + 1];

static int thread_main(void *arg)
{
	unsigned int tid = (uintptr_t)arg;


	for (unsigned int i = 0; i < N; i++) {
		// struct elem *e = malloc(sizeof(*e));
		// e->tid = tid;
		// e->idx = i;

		// static struct mutex mutex2 = MUTEX_INITIALIZER;
		// mutex_lock(&mutex);
		// mutex_lock(&mutex2);
		// int err = pthread_mutex_lock(&pthread_mutex);
		// err |= pthread_mutex_lock(&pthread_mutex);

		static qspinlock_t spinlock2 = QSPINLOCK_INITIALIZER;
		qspinlock_lock(&spinlock);
		qspinlock_lock(&spinlock2);
		// ticketlock_lock(&ticketlock);
		counter++;
		// ticketlock_unlock(&ticketlock);
		qspinlock_unlock(&spinlock2);
		qspinlock_unlock(&spinlock);

		// ssize_t n = write(2, ".", 1);
		// n += write(2, "\n", 1);
		// assert(n == 2);

		// e->next = head;
		// head = e;

		// mutex_unlock(&mutex2);
		// mutex_unlock(&mutex);
		// err |= pthread_mutex_unlock(&pthread_mutex);
		// err |= pthread_mutex_unlock(&pthread_mutex);
		// assert(!err);
	}


	// for (unsigned int i = 0; i < 1000; i++) {
	// 	semaphore_wait(&insem);

	// 	mutex_lock(&inlock);
	// 	int x = array_pop(input);
	// 	mutex_unlock(&inlock);

	// 	mutex_lock(&outlock);
	// 	struct string *s = array_addn(output, 1);
	// 	sprintf(s->s, "%i", x);
	// 	mutex_unlock(&outlock);

	// 	semaphore_post(&outsem);
	// }

	// for (unsigned int i = 0; i < N2 * 256; i++) {
	// 	int x = cbuffer_get(&buffers[tid]);
	// 	cbuffer_put(&buffers[tid + 1], x + 1);
	// }

	return 0;
}


int main(void)
{
	// {
	// 	mutex_init(&mutex, true, true);

	// 	pthread_mutexattr_t attr;
	// 	pthread_mutexattr_init(&attr);
	// 	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	// 	pthread_mutex_init(&pthread_mutex, &attr);
	// 	pthread_mutexattr_destroy(&attr);
	// }

	// array_reserve(input, 1000 * NUM_THREADS);
	// array_reserve(output, 1000 * NUM_THREADS);

	// for (unsigned int i = 0; i < NUM_THREADS + 1; i++) {
	// 	cbuffer_init(&buffers[i]);
	// }

	thrd_t threads[NUM_THREADS];
	for (unsigned int i = 0; i < NUM_THREADS; i++) {
		int err = thrd_create(&threads[i], thread_main, (void *)(uintptr_t)i);
		assert(err == thrd_success);
	}

	// for (unsigned int i = 0; i < 1000 * NUM_THREADS; i++) {
	// 	mutex_lock(&inlock);
	// 	array_add(input, i);
	// 	mutex_unlock(&inlock);
	// 	semaphore_post(&insem);
	// }

	// for (unsigned int i = 0; i < 1000 * NUM_THREADS; i++) {
	// 	semaphore_wait(&outsem);
	// 	mutex_lock(&outlock);
	// 	struct string s = array_pop(output);
	// 	puts(s.s);
	// 	mutex_unlock(&outlock);
	// }

	// for (unsigned int n = 0; n < N2; n++) {
	// 	for (unsigned int i = 0; i < 256; i++) {
	// 		cbuffer_put(&buffers[0], 0);
	// 	}

	// 	for (unsigned int i = 0; i < 256; i++) {
	// 		int x = cbuffer_get(&buffers[NUM_THREADS]);
	// 		assert(x == NUM_THREADS);
	// 	}
	// }

	for (unsigned int i = 0; i < NUM_THREADS; i++) {
		int err = thrd_join(threads[i], NULL);
		assert(err == thrd_success);
		// printf("thread %u finished\n", i);
	}

	assert(counter == N * NUM_THREADS);

	// unsigned int idx[NUM_THREADS] = {0};
	// for (unsigned int i = 0; i < NUM_THREADS; i++) {
	// 	idx[i] = N;
	// }
	// struct elem *e = head;
	// while (e) {
	// 	idx[e->tid]--;
	// 	assert(e->idx == idx[e->tid]);
	// 	e = e->next;
	// }

	// for (unsigned int i = 0; i < NUM_THREADS; i++) {
	// 	assert(idx[i] == 0);
	// }
}
