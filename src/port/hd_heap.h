/* hode's heap (hd_heap.c). */
#ifndef HD_HEAP_H
#define HD_HEAP_H

#include <stddef.h>

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
#ifdef HOST_BUILD
void hd_heap_set_caller(void *caller);
#endif

#ifdef __cplusplus
}
#endif

#endif
