#include <stdio.h>
#include "mem_arena.h"

int main(void)
{
	struct mem_arena arena;
	mem_arena_init(&arena);
	void *p;
	p = mem_arena_alloc(&arena, 1);
	printf("%p\n", p);
	p = mem_arena_alloc(&arena, 2);
	printf("%p\n", p);
	p = mem_arena_alloc(&arena, 3);
	printf("%p\n", p);
	p = mem_arena_alloc(&arena, 4096);
	printf("%p\n", p);
	p = mem_arena_alloc(&arena, 12345);
	printf("%p\n", p);
	p = mem_arena_alloc(&arena, 123456);
	printf("%p\n", p);
	p = mem_arena_alloc(&arena, 1);
	printf("%p\n", p);
	mem_arena_destroy(&arena);
}
