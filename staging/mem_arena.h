#ifndef __mem_arena_include__
#define __mem_arena_include__

#include <stdlib.h>

#ifndef MEM_ARENA_DEFAULT_SIZE
#define MEM_ARENA_DEFAULT_SIZE 4096
#endif

struct __membucket {
	struct __membucket *next;
	size_t used;
	size_t capacity;
	char mem[];
};

struct mem_arena {
	struct __membucket *first;
	struct __membucket *last;
};

static void mem_arena_init(struct mem_arena *arena)
{
	arena->first = NULL;
	arena->last = NULL;
}

static void mem_arena_destroy(struct mem_arena *arena)
{
	struct __membucket *cur = arena->first;
	while (cur) {
		struct __membucket *next = cur->next;
		free(cur);
		cur = next;
	}
	mem_arena_init(arena);
}

static void *mem_arena_alloc(struct mem_arena *arena, size_t size)
{
	struct __membucket *bucket = arena->last;
	if (bucket && (bucket->capacity - bucket->used >= size)) {
		void *result = &bucket->mem[bucket->used];
		bucket->used += size;
		return result;
	}

	// if size is greater than MEM_ARENA_DEFAULT_SIZE the bucket is immediatly full...
	size_t capacity = size > MEM_ARENA_DEFAULT_SIZE ? size : MEM_ARENA_DEFAULT_SIZE;
	bucket = malloc(sizeof(*bucket) + capacity);
	bucket->next = NULL;
	bucket->used = size;
	bucket->capacity = capacity;

	if (arena->last) {
		arena->last->next = bucket;
	}
	arena->last = bucket;

	return bucket->mem;
}

#endif
