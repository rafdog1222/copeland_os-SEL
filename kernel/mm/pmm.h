#ifndef PMM_H
#define PMM_H

#include <stdint.h>

#define  PAGE_SIZE 4096

void     pmm_init(uint32_t mem_upper);
void    *pmm_alloc_page(void);
void     pmm_free_page(void *addr);
uint32_t pmm_free_pages(void);
uint32_t pmm_total_pages(void);

#endif
