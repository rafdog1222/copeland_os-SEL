/*
    importent stuff,  
    Physical Memory Manager, bitmap allocator~
   
    dividing all ram into 4kb pages,
    each bit in our bitmap = one page.
    0 = free, 1 = used.
   
    grub tells us how much ram exists via multiboot,
    we mark everything used, then free what's actually
    available. Kernel itself stays marked used forever.

    hope this works out, lol
*/

#include "pmm.h"
#include "../kernel.h"


/* bitmap 
   32768 bits = 32768 pages = 128MB addressable
   stored as 1024 x uint32_t (each uint32 = 32 bits = 32 pages)
   this covers way more than my 32MB qemu machine

*/
#define BITMAP_SIZE 1024
static uint32_t bitmap[BITMAP_SIZE];
static uint32_t total_pages = 0;
static uint32_t free_pages = 0;


/* bitmap helpers, */
static void bitmap_set(uint32_t page) {
    bitmap[page / 32] |= (1 << (page % 32));
}

static void bitmap_clear(uint32_t page) {
    bitmap[page / 32] &= ~(1 << (page % 32));
}

static int bitmap_test(uint32_t page) {
    return bitmap[page / 32] & (1 << (page % 32));
}

/* kernel boundaries (in linker) */
extern uint32_t kernel_start;
extern uint32_t kernel_end;

/* init */
void pmm_init(uint32_t mem_upper) {
    /*
       mem_upper comes from multiboot, it's kb of ram
       above 1mb, total ram = 1MB + mem_upper kb,
       grub gives this for free
    */
    uint32_t total_ram   = (1024 + mem_upper) * 1024;
    total_pages          = total_ram / PAGE_SIZE;
    free_pages           = 0;
    /* start, marking everything used */
    for (int i = 0; i < BITMAP_SIZE; i++)
        bitmap[i] = 0xFFFFFFFF;

    /* free pages above 1mb up to total ram
    (below 1mb is going to be BIOS/VGA territory, leaving it alone lol) */
    uint32_t start_page = (1024 * 1024) / PAGE_SIZE;  /* page 256 = 1MB */
    for (uint32_t p = start_page; p < total_pages && p < BITMAP_SIZE * 32; p++) {
        bitmap_clear(p);
        free_pages++;
    }
    /* re-marking kernel pages as used so we don't overwrite anything */
    uint32_t k_start = (uint32_t)&kernel_start / PAGE_SIZE;
    uint32_t k_end   = ((uint32_t)&kernel_end + PAGE_SIZE - 1) / PAGE_SIZE;
    for (uint32_t p = k_start; p <= k_end; p++) {
        if (!bitmap_test(p)) {
            bitmap_set(p);
            free_pages--;
        }
    }
    /* also protect the bitmap itself */
    uint32_t bm_start = (uint32_t)bitmap / PAGE_SIZE;
    uint32_t bm_end   = ((uint32_t)bitmap + sizeof(bitmap)) / PAGE_SIZE;
    for (uint32_t p = bm_start; p <= bm_end; p++) {
        if (!bitmap_test(p)) {
            bitmap_set(p);
            free_pages--;
        }
    }
}

/* alloc */
void *pmm_alloc_page(void) {
    for (uint32_t i = 0; i < BITMAP_SIZE; i++) {
        if (bitmap[i] == 0xFFFFFFFF) continue;  /* if all used, skip it */
        /* finding first free bit */
        for (int bit = 0; bit < 32; bit++) {
            uint32_t page = i * 32 + bit;
            if (!bitmap_test(page)) {
                bitmap_set(page);
                free_pages--;
                return (void *)(page * PAGE_SIZE);
            }
        }
    }
    return 0;  /* out of memory, rip */
}

/* free */
void pmm_free_page(void *addr) {
    uint32_t page = (uint32_t)addr / PAGE_SIZE;
    if (bitmap_test(page)) {
        bitmap_clear(page);
        free_pages++;
    }
}

/* stats */
uint32_t pmm_free_pages(void)  { return free_pages;  }
uint32_t pmm_total_pages(void) { return total_pages; }
