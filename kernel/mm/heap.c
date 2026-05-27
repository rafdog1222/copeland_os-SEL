/* 
   kernel heap, free list allocator on top of the pmm

   i am grabing pages from pmm and managing them as a linked
   list of blocks, each block has a small header, then
   the usable data follows immediately after

   Layout in memory:
   [block_header][....data....][block_header][....data....]...
   so on so forth, primitive, but works 
*/

#include "heap.h"
#include "pmm.h"
#include "../kernel.h"

/* block header */
typedef struct block_header {
    uint32_t            size;    /* bytes of data (not including header) */
    uint32_t            free;    /* 1 = free, 0 = used */
    struct block_header *next;   /* next block in list */
} block_header_t;

#define HEADER_SIZE sizeof(block_header_t)

/* heap state */
/* starting with 4 pages = 16KB heap, expandable */
#define INITIAL_PAGES 4

static block_header_t *heap_start = 0;
static block_header_t *heap_end   = 0;

/* expanding heap by one page */
static int heap_expand(void) {
    void *page = pmm_alloc_page();
    if (!page) return 0;  /* out of physical memory, rip */

    block_header_t *new_block = (block_header_t *)page;
    new_block->size = PAGE_SIZE - HEADER_SIZE;
    new_block->free = 1;
    new_block->next = 0;
    if (!heap_start) {
        heap_start = new_block;
        heap_end   = new_block;
    } else {
        heap_end->next = new_block;
        heap_end       = new_block;
    }
    return 1;
}

/* i am init lol */
void heap_init(void) {
    for (int i = 0; i < INITIAL_PAGES; i++)
        heap_expand();
}

/* split a block if it's bigger than needed */
static void block_split(block_header_t *block, uint32_t size) {
/* split if remainder is big enough to be useful, this is why we learn math~ */
    if (block->size <= size + HEADER_SIZE + 16) return;
    block_header_t *new_block = (block_header_t *)
        ((uint8_t *)block + HEADER_SIZE + size);
    new_block->size  = block->size - size - HEADER_SIZE;
    new_block->free  = 1;
    new_block->next  = block->next;
    block->size = size;
    block->next = new_block;
}

/* merge adjacent free blocks (coalescing (big word alert)) */
static void heap_coalesce(void) {
    block_header_t *cur = heap_start;
    while (cur && cur->next) {
        if (cur->free && cur->next->free) {
            cur->size += HEADER_SIZE + cur->next->size;
            cur->next  = cur->next->next;
        } else {
            cur = cur->next;
        }
    }
}

/* kmalloc here */
void *kmalloc(uint32_t size) {
    if (size == 0) return 0;
    /* align it to 4 bytes */
    size = (size + 3) & ~3;
    block_header_t *cur = heap_start;
    /* first fit, first free block big enough */
    while (cur) {
        if (cur->free && cur->size >= size) {
            block_split(cur, size);
            cur->free = 0;
            return (void *)((uint8_t *)cur + HEADER_SIZE);
        }
        cur = cur->next;
    }
    /* no block big enough, thn expand heap and try again */
    if (heap_expand()) return kmalloc(size);
    return 0;  /* truly out of memory, holy rip */
}

/*  kfree */
void kfree(void *ptr) {
    if (!ptr) return;
    block_header_t *block = (block_header_t *)
        ((uint8_t *)ptr - HEADER_SIZE);
    block->free = 1;
    heap_coalesce();  /* merge adjacent free blocks */
}

/* stats for mem command */
void heap_stats(uint32_t *out_used, uint32_t *out_free) {
    uint32_t used = 0, free = 0;
    block_header_t *cur = heap_start;
    while (cur) {
        if (cur->free) free += cur->size;
        else           used += cur->size;
        cur = cur->next;
    }
    if (out_used) *out_used = used;
    if (out_free) *out_free = free;
}
