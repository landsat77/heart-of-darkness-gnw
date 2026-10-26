/* hode's heap (hd_heap.c). */
#ifndef HD_HEAP_H
#define HD_HEAP_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void hd_heap_add(void *start, size_t size);
void *hd_malloc(size_t n);
void hd_free(void *p);
void *hd_calloc(size_t count, size_t size);
void *hd_realloc(void *p, size_t n);
char *hd_strdup(const char *s);
size_t hd_heap_used(void);
size_t hd_heap_peak(void);
size_t hd_heap_total(void);
size_t hd_heap_largest_free(void);
/* Save states: the region table's signature (same on load), the blocks */
uint32_t hd_heap_signature(void);
int hd_heap_save(int (*write)(const void *, size_t, void *), void *ctx);
int hd_heap_load(int (*read)(void *, size_t, void *), void *ctx);
/* malloc & co for the engine: fatal error when out of memory */
void *hd_engine_malloc(size_t n);
void *hd_engine_calloc(size_t count, size_t size);
void *hd_engine_realloc(void *p, size_t n);
#ifdef HOST_BUILD
void hd_heap_set_caller(void *caller);
#endif

#ifdef __cplusplus
}
#endif

#endif
