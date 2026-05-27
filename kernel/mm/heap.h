#ifndef HEAP_H
#define HEAP_H

#include <stdint.h>

void heap_init(void);
void *kmalloc(uint32_t size);
void kfree(void *ptr);
void heap_stats(uint32_t *out_used, uint32_t *out_free);

#endif
