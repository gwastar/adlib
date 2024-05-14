#ifndef __heap_include__
#define __heap_include__

#include "list.h"

#define USE_MALLOC 0
#define GUARD_PAGES 1
#define HEAP_ALIGNMENT 16 // must be a power of 2 between 8 and 4096 (both inclusive)
#define HEAP_NBUCKETS 8

struct heap {
	struct list_head free_list[HEAP_NBUCKETS];
};

void *heap_malloc(struct heap *heap, size_t size)
	__attribute__((malloc, assume_aligned(HEAP_ALIGNMENT)));
void heap_free(struct heap *heap, void *mem);
void *heap_realloc(struct heap *heap, void *ptr, size_t size)
	__attribute__((assume_aligned(HEAP_ALIGNMENT)));
void heap_init(struct heap *heap);

#endif
