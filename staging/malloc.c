#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <sys/mman.h>
#include "malloc.h"

/*
 * TODO: Try an approach similar to partition_alloc in chrome:
 * -tiers of slot sizes, e.g. range(start=0, stop=128, step=16), range(128, 512, 32), range(512, 4096, 64)
 * -one bucket per slot size according to the size tiers
 * -bucket = list of spans with the same size
 * -span = memory area that contains n slots of the same size + one metadata header with freelist-pointer and allocation-bitmap
 * -allocate separate range of pages for metadata
 * -maybe use the old implementation for sizes greater than e.g. 512
 */

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define DIV_ROUND_UP(a, b) (((a) + (b) - 1) / (b))
#define ALIGNP2(val, pow2) ((val) & ~((pow2) - 1))

#define HEAP_ALIGN(val) ALIGNP2(val + (HEAP_ALIGNMENT - 1), HEAP_ALIGNMENT)
#define HEAP_ALIGNED(val) (((size_t)val & (HEAP_ALIGNMENT - 1)) == 0)

// use bottom 3 bits because min alignment is 8
static const size_t FLAG_USED   = 0x1;
static const size_t FLAG_LAST   = 0x2;
static const size_t FLAG_ZEROED = 0x4;
static const size_t FLAG_BITS   = HEAP_ALIGNMENT - 1;

struct memblock {
	size_t prev_size;
	size_t size;
	_Alignas(HEAP_ALIGNMENT) unsigned char mem[];
};

struct freeblock {
	unsigned char memblock_bytes[sizeof(struct memblock)];
	struct list_head list_head;
};

#define to_memblock(p) &(container_of(p, struct freeblock, list_head)->memblock)

static const size_t DEFAULT_MAP_SIZE = 4 * 4096;
static const size_t MEMBLOCK_EXTRA_SIZE = sizeof(struct memblock);
static const size_t MIN_BLOCK_SIZE = MAX(HEAP_ALIGNMENT, sizeof(struct list_head));

static inline bool block_size_valid(size_t size)
{
	return HEAP_ALIGNED(size) && size >= MIN_BLOCK_SIZE;
}

static inline size_t fix_block_size(size_t size)
{
	return MAX(HEAP_ALIGN(size), MIN_BLOCK_SIZE);
}

static inline size_t get_flags(struct memblock *block)
{
	return block->prev_size & FLAG_BITS;
}

static inline void set_flags(struct memblock *block, size_t flags)
{
	assert((flags & ~FLAG_BITS) == 0);
	block->prev_size &= ~FLAG_BITS;
	block->prev_size |= flags;
}

static inline size_t get_prev_size(struct memblock *block)
{
	return block->prev_size & ~FLAG_BITS;
}

static inline void set_prev_size(struct memblock *block, size_t prev_size)
{
	// 0 is valid for the first block
	assert(prev_size == 0 || block_size_valid(prev_size));
	block->prev_size &= FLAG_BITS;
	block->prev_size |= prev_size;
}

static inline size_t get_size(struct memblock *block)
{
	return block->size;
}

static inline void set_size(struct memblock *block, size_t size)
{
	assert(block_size_valid(size));
	block->size = size;
}

static inline bool is_free(struct memblock *block)
{
	return !(get_flags(block) & FLAG_USED);
}

static inline void mark_used(struct memblock *block)
{
	set_flags(block, get_flags(block) | FLAG_USED);
}

static inline void mark_free(struct memblock *block)
{
	set_flags(block, get_flags(block) & ~FLAG_USED);
}

static inline void init_memblock(struct memblock *block, size_t size, size_t flags, size_t prev_size)
{
	set_size(block, size);
	set_flags(block, flags);
	set_prev_size(block, prev_size);
}

static inline struct memblock *__get_next_unchecked(struct memblock *block)
{
	return (struct memblock *)(block->mem + get_size(block));
}

static inline struct memblock *get_next(struct memblock *block)
{
	if (get_flags(block) & FLAG_LAST) {
		return NULL;
	}

	size_t block_size = get_size(block);
	struct memblock *next = __get_next_unchecked(block);
	if (block_size != get_prev_size(next)) {
		assert(!"Heap corruption detected!!!");
		abort();
	}

	return next;
}

static inline struct memblock *get_previous(struct memblock *block)
{
	size_t prev_size = get_prev_size(block);
	if (prev_size == 0) {
		return NULL;
	}

	struct memblock *prev = (struct memblock *)((unsigned char *)block - prev_size) - 1;

	if (get_size(prev) != prev_size) {
		assert(!"Heap corruption detected!!!");
		abort();
	}

	return prev;
}

static inline size_t size_to_bucket_index(size_t size)
{
	assert(size != 0);
	size_t idx = DIV_ROUND_UP(size, MIN_BLOCK_SIZE);
	idx = __builtin_ctz(idx); // log2
	return MIN(idx, HEAP_NBUCKETS - 1);
}

static inline void free_list_insert(struct heap *heap, struct memblock *memblock)
{
	struct freeblock *freeblock = (struct freeblock *)memblock;
	size_t idx = size_to_bucket_index(get_size(memblock));
	list_insert_after(&heap->free_list[idx], &freeblock->list_head);
}

static inline void free_list_remove(struct heap *heap, struct memblock *memblock)
{
	(void)heap;
	struct freeblock *freeblock = (struct freeblock *)memblock;
	list_remove_item(&freeblock->list_head);
}

// returns a pointer to the merged block (can only be different from block when merging with prev)
static struct memblock *try_merge(struct heap *heap, struct memblock *block, bool merge_prev)
{
	struct memblock *prev = merge_prev ? get_previous(block) : NULL;
	struct memblock *next = get_next(block);
	size_t orig_size = get_size(block);
	size_t new_size = orig_size;
	size_t prev_size = get_prev_size(block);
	size_t flags = get_flags(block);

	if (prev && is_free(prev)) {
		free_list_remove(heap, prev);
		block = prev; // if we merge with prev the address changes
		prev_size = get_prev_size(prev);
		new_size += get_size(prev) + MEMBLOCK_EXTRA_SIZE;
	}

	if (next && is_free(next)) {
		free_list_remove(heap, next);
		new_size += get_size(next) + MEMBLOCK_EXTRA_SIZE;
		flags |= get_flags(next) & FLAG_LAST;
		next = get_next(next);
	}

	if (new_size != orig_size) {
		assert(HEAP_ALIGNED(new_size));
		// we only merge blocks we get back from the user so this should not be set
		assert(!(flags & FLAG_ZEROED));
		if (next) {
			set_prev_size(next, new_size);
		}
		init_memblock(block, new_size, flags, prev_size);
	}
	return block;
}

// block will have atleast 'size' free bytes afterwards
static void maybe_split_block(struct heap *heap, struct memblock *block, size_t size)
{
	assert(block_size_valid(size));
	if (get_size(block) >= size + MEMBLOCK_EXTRA_SIZE + MIN_BLOCK_SIZE) {
		// splitting will produce a new block with atleast minimal size

		// note that FLAG_LAST and FLAG_ZEROED are properly carried over here
		size_t flags = get_flags(block);
		size_t new_size = get_size(block) - size - MEMBLOCK_EXTRA_SIZE;
		assert(new_size >= MIN_BLOCK_SIZE);

		set_size(block, size);
		set_flags(block, flags & (FLAG_USED | FLAG_ZEROED));

		struct memblock *next = __get_next_unchecked(block);
		init_memblock(next, new_size, flags & (FLAG_ZEROED | FLAG_LAST), size);
		if (!(flags & FLAG_LAST)) {
			set_prev_size(__get_next_unchecked(next), new_size);
		}
		free_list_insert(heap, next);
	}
}

static void alloc_memblock(struct heap *heap, struct memblock *block)
{
	assert(is_free(block));
	mark_used(block);
	free_list_remove(heap, block);
}

static bool free_pages(void *ptr, size_t size)
{
#if GUARD_PAGES
	ptr = (char *)ptr - 4096;
	size += 2 * 4096;
#endif
	return munmap(ptr, size) == 0;
}

static void free_memblock(struct heap *heap, struct memblock *block)
{
	assert(!is_free(block));

	if (!get_previous(block) && !get_next(block)) {
		// TODO maybe don't free if this is our last free block
#if USE_MALLOC
		free(block);
		return;
#else
		if (free_pages(block, get_size(block) + MEMBLOCK_EXTRA_SIZE)) {
			return;
		}
		// freeing failed so we just keep using the memory
#endif
	}

	mark_free(block);
	free_list_insert(heap, block);
}

static struct memblock *find_free_block(struct heap *heap, size_t size)
{
	for (size_t i = size_to_bucket_index(size); i < HEAP_NBUCKETS; i++) {
		// first fit
		list_foreach_elem(&heap->free_list[i], cur, struct freeblock, list_head) {
			struct memblock *block = (struct memblock *)cur;
			if (get_size(block) >= size) {
				return block;
			}
		}
	}
	return NULL;
}

static void *alloc_pages(size_t size)
{
	size_t total = size;
	int prot = PROT_READ | PROT_WRITE;

#if GUARD_PAGES
	total += 2 * 4096;
	prot = PROT_NONE;
#endif

	char *ptr = mmap(NULL, total, prot, MAP_PRIVATE | MAP_ANONYMOUS /* | MAP_POPULATE */, -1, 0);
	if (ptr == MAP_FAILED) {
		return NULL;
	}

#if GUARD_PAGES
	ptr += 4096;
	// TODO is it faster to call mprotect twice for single pages?
	if (mprotect(ptr, size, PROT_READ | PROT_WRITE) != 0) {
		munmap(ptr - 4096, total);
		return NULL;
	}
#endif

	return ptr;
}

static struct memblock *__heap_malloc(struct heap *heap, size_t size)
{
	if (size == 0) {
		// could comment this out to return
		// "a unique pointer value that can later be successfully passed to free()"
		return NULL;
	}
	size = fix_block_size(size);
	struct memblock *block = find_free_block(heap, size);
	if (block) {
		alloc_memblock(heap, block);
	} else {
		size_t map_size = MAX(DEFAULT_MAP_SIZE,
		                      ALIGNP2(size + MEMBLOCK_EXTRA_SIZE + 4095, 4096));
		size_t flags = FLAG_USED | FLAG_LAST;
#if USE_MALLOC
		void *ptr = aligned_alloc(HEAP_ALIGNMENT, map_size);
#else
		flags |= FLAG_ZEROED;
		void *ptr = alloc_pages(map_size);
#endif
		if (!ptr) {
			assert(errno == ENOMEM);
			return NULL;
		}
		block = (struct memblock *)ptr;
		init_memblock(block, map_size - MEMBLOCK_EXTRA_SIZE, flags, 0);
	}

	maybe_split_block(heap, block, size);
	assert(!is_free(block));
	assert(HEAP_ALIGNED(block->mem));
	assert(get_size(block) >= size);
	return block;
}

void *heap_malloc(struct heap *heap, size_t size)
{
	struct memblock *block = __heap_malloc(heap, size);
	if (!block) {
		return NULL;
	}
	set_flags(block, get_flags(block) & ~FLAG_ZEROED);
	return block->mem;
}

void *heap_calloc(struct heap *heap, size_t nmemb, size_t size)
{
	size_t total = size * nmemb;
	if (size != 0 && total / size != nmemb) {
		assert(!"Overflow");
		abort();
	}
	struct memblock *block = __heap_malloc(heap, total);
	if (!block) {
		return NULL;
	}
	size_t flags = get_flags(block);
	if (flags & FLAG_ZEROED) {
		set_flags(block, flags & ~FLAG_ZEROED);
		// this block may have been in the freelist
		memset(block->mem, 0, sizeof(struct list_head));
	} else {
		memset(block->mem, 0, get_size(block));
	}
	return block->mem;
}

void heap_free(struct heap *heap, void *ptr)
{
	if (!ptr) {
		return;
	}
	struct memblock *block = (struct memblock *)ptr - 1;
	size_t size = get_size(block);
	if (is_free(block) || !block_size_valid(size)) {
		assert(!"Invalid pointer");
		abort();
	}
	block = try_merge(heap, block, true);
	free_memblock(heap, block);
}

void *heap_realloc(struct heap *heap, void *ptr, size_t size)
{
	if (!ptr) {
		return heap_malloc(heap, size);
	}

	if (size == 0) {
		heap_free(heap, ptr);
		return NULL;
	}

	struct memblock *block = (struct memblock *)ptr - 1;
	if (is_free(block) || !block_size_valid(get_size(block))) {
		assert(!"Invalid pointer");
		abort();
	}

	// First try to merge only with the next block, so we don't have to copy.
	// Afterwards, if the block isn't big enough, try merging with the previous block.
	// If the resulting block is big enough, try splitting it again.
	// If not, relocate (malloc + memcpy + free)

	size_t orig_size = get_size(block);
	struct memblock *orig_block = block;
	try_merge(heap, block, false);
	if (get_size(block) < size) {
		block = try_merge(heap, block, true);
	}

	if (get_size(block) >= size) {
		// block might be too big after merging or the user just wanted to shrink the block
		if (block != orig_block) {
			memmove(block->mem, ptr, MIN(size, orig_size));
		}
		maybe_split_block(heap, block, fix_block_size(size));
	} else {
		// block size is too small even after merging
		ptr = heap_malloc(heap, size);
		memcpy(ptr, block->mem, MIN(size, orig_size));

		// block is already fully merged
		free_memblock(heap, block);

		block = (struct memblock *)ptr - 1;
	}

	assert(!is_free(block));
	assert(HEAP_ALIGNED(block->mem));
	assert(get_size(block) >= size);
	return block->mem;
}

void heap_init(struct heap *heap)
{
	for (size_t i = 0; i < HEAP_NBUCKETS; i++) {
		list_head_init(&heap->free_list[i]);
	}
}

int main(void)
{
	struct heap heap;
	heap_init(&heap);
	static unsigned char *p[4096] = {0};
	size_t n = sizeof(p) / sizeof(p[0]);

#if 0
	for (size_t k = 0; k < 50000; k++) {
		for (size_t i = 0; i < n; i++) {
			size_t size = 16 * (rand() % 16);
			p[i] = heap_realloc(&heap, p[i], size);
		}
	}
#else
	for (size_t k = 0; k < 1000; k++) {
		for (size_t i = 0; i < n; i++) {
			p[i] = heap_calloc(&heap, (i % 16), 16);
			for (size_t k = 0; k < (i % 16) * 16; k++) {
				assert(p[i][k] == 0);
				p[i][k] = k;
			}
		}

		for (size_t i = 0; i < n; i++) {
			size_t size = 16 * (i % 16);
			p[i] = heap_realloc(&heap, p[i], size + 17);
			for (size_t k = 0; k < size; k++) {
				assert(p[i][k] == k);
			}
			p[i] = heap_realloc(&heap, p[i], size);
			for (size_t k = 0; k < size; k++) {
				assert(p[i][k] == k);
			}
		}

		for (size_t i = 0; i < n; i++) {
			heap_free(&heap, p[i]);
		}

		for (size_t i = 0; i < n; i++) {
			size_t size = ((i & 1) + 1) * 16;
			p[i] = heap_malloc(&heap, size);
			memset(p[i], 0xcc, size);
		}

		for (size_t i = 0; i < n / 2; i++) {
			heap_free(&heap, p[i]);
		}

		for (size_t i = 0; i < n / 2; i++) {
			p[i] = heap_calloc(&heap, 16, 1);
			for (size_t k = 0; k < 16; k++) {
				assert(p[i][k] == 0);
			}
		}

		for (size_t i = 0; i < n; i++) {
			heap_free(&heap, p[i]);
		}
	}
#endif
}
