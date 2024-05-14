#ifndef __SEMAPHORE_INCLUDE__
#define __SEMAPHORE_INCLUDE__

#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <limits.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <linux/futex.h>

#ifndef futex
#define futex __futex
static int __futex(int *uaddr, int futex_op, int val,
		   const struct timespec *timeout, int *uaddr2, int val3)
{
	return syscall(SYS_futex, uaddr, futex_op, val, timeout, uaddr2, val3);
}
#endif

struct semaphore {
	union {
		atomic_ullong count_waiting;
		struct {
			atomic_uint count;
			atomic_uint waiting;
		};
	};
};

static_assert(sizeof(unsigned long long) == 2 * sizeof(unsigned int), "");

#define SEMAPHORE_INITIALIZER(initial_count) {.count = initial_count, .waiting = 0}

static void semaphore_init(struct semaphore *sem, unsigned int initial_count)
{
	sem->count = initial_count;
	sem->waiting = 0;
}

static bool semaphore_trywait(struct semaphore *sem)
{
	unsigned int count = atomic_load_explicit(&sem->count, memory_order_relaxed);
	while (count != 0) {
		if (atomic_compare_exchange_strong_explicit(&sem->count, &count, count - 1,
							    memory_order_acq_rel, memory_order_relaxed)) {
			return true;
		}
		__builtin_ia32_pause();
	}
	return false;
}

static void semaphore_wait(struct semaphore *sem)
{
	unsigned int i = 0;
retry_lock:
	for (;;) {
		if (semaphore_trywait(sem)) {
			return;
		}

		unsigned long long val = atomic_load_explicit(&sem->count_waiting, memory_order_relaxed);
		for (;;) {
			unsigned long long count = (unsigned int)val;
			if (count != 0) {
				goto retry_lock;
			} else if (i < 50) {
				i++;
				__builtin_ia32_pause();
				val = atomic_load_explicit(&sem->count_waiting, memory_order_relaxed);
				continue;
			}
			unsigned long long waiting = (unsigned int)(val >> (8 * sizeof(int)));
			waiting++;
			unsigned long long newval = 0 | (waiting << (8 * sizeof(int)));

			if (atomic_compare_exchange_strong_explicit(&sem->count_waiting, &val, newval, memory_order_seq_cst, memory_order_relaxed)) {
				break;
			}
		}

		int err = futex((int *)&sem->count, FUTEX_WAIT | FUTEX_PRIVATE_FLAG, 0, NULL, NULL, 0);
		if (err != 0 && errno != EAGAIN) {
			perror("futex() failed");
			exit(EXIT_FAILURE);
		}

		atomic_fetch_sub_explicit(&sem->waiting, 1, memory_order_relaxed);
	}
}

static void semaphore_post(struct semaphore *sem)
{
	unsigned int old_count = atomic_fetch_add_explicit(&sem->count, 1, memory_order_acq_rel); // TODO protect against overflow?
#if 1
	if (old_count != 0) {
		return;
	}
	int wake_count = INT_MAX; // TODO don't wake up all threads (maybe call futex_wake in a loop until all threads have been woken up or count is zero again?)
#else
	int wake_count = 1;
#endif

	atomic_thread_fence(memory_order_seq_cst);
	unsigned int waiting = atomic_load_explicit(&sem->waiting, memory_order_relaxed);
	if (waiting != 0) {
		int err = futex((int *)&sem->count, FUTEX_WAKE | FUTEX_PRIVATE_FLAG, wake_count, NULL, NULL, 0);
		if (err < 0) {
			perror("futex() failed");
			exit(EXIT_FAILURE);
		}
	}
}

#endif
