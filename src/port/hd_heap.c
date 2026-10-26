/*
 * hode's heap: first fit with boundary tags over a few memory regions
 * (RAM_EMU, the LCD pool, DTCM on the console; one block on the PC, the
 * same size, so that the PC build runs out of memory where the console
 * would). Blocks are 8-byte aligned.
 */
#ifdef HOST_BUILD
#define _GNU_SOURCE
#endif
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "hd_heap.h"

typedef struct block {
	size_t size;		/* whole block incl. header, low bit = used */
	size_t prev_size;	/* previous block in the region, 0 = first */
} block_t;

#define HDR sizeof(block_t)
#define USED 1u
#define MIN_BLOCK (HDR + 16)
#define MAX_REGIONS 8

typedef struct {
	uint8_t *start, *end;	/* end = sentinel header (size 0, used) */
} region_t;

static region_t regions[MAX_REGIONS];
static int region_count;
static size_t used_bytes, peak_bytes, total_bytes;

static size_t bsize(const block_t *b) { return b->size & ~(size_t)USED; }
static block_t *next_block(block_t *b) { return (block_t *)((uint8_t *)b + bsize(b)); }

void hd_heap_add(void *start, size_t size)
{
	if (region_count >= MAX_REGIONS) return;
	uintptr_t s = ((uintptr_t)start + 7) & ~(uintptr_t)7;
	uintptr_t e = ((uintptr_t)start + size) & ~(uintptr_t)7;
	if (e <= s + 2 * MIN_BLOCK) return;
	region_t *r = &regions[region_count++];
	r->start = (uint8_t *)s;
	r->end = (uint8_t *)(e - HDR);
	block_t *b = (block_t *)r->start;
	b->size = (size_t)(r->end - r->start);
	b->prev_size = 0;
	block_t *sentinel = (block_t *)r->end;
	sentinel->size = USED;
	sentinel->prev_size = b->size;
	total_bytes += b->size;
}

static void set_size(block_t *b, size_t size, int used)
{
	b->size = size | (used ? USED : 0);
	next_block(b)->prev_size = size;
}

#ifdef HOST_BUILD
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
static void *heap_log_caller;
/* HD_HEAPTRACE=1: every allocation and free on stderr ("A size caller ptr",
   "F ptr"), caller as an offset in the executable (addr2line) */
static int heap_trace = -1;
static unsigned long code_offset(void *caller) {
	Dl_info info;
	uintptr_t off = (uintptr_t)caller;
	if (dladdr(caller, &info) && info.dli_fbase) off -= (uintptr_t)info.dli_fbase;
	return (unsigned long)off;
}
void hd_heap_set_caller(void *caller) { heap_log_caller = caller; }
#endif

#ifdef HOST_BUILD
static void *hd_malloc_raw(size_t n);
#endif

void *hd_malloc(size_t n)
{
#ifdef HOST_BUILD
	/* HD_HEAPLOG=bytes: print the allocations from that size on */
	static long log_min = -1;
	if (log_min < 0) log_min = getenv("HD_HEAPLOG") ? atol(getenv("HD_HEAPLOG")) : 0;
	if (heap_trace < 0) heap_trace = getenv("HD_HEAPTRACE") != 0;
	void *caller = heap_log_caller ? heap_log_caller : __builtin_return_address(0);
	heap_log_caller = 0;
	if (log_min && (long)n >= log_min)
		fprintf(stderr, "heap: %zu bytes from 0x%lx (used %zu)\n", n, code_offset(caller), used_bytes);
	void *p = hd_malloc_raw(n);
	if (heap_trace) fprintf(stderr, "A %zu 0x%lx %p\n", n, code_offset(caller), p);
	return p;
}

static void *hd_malloc_raw(size_t n)
{
#endif
	if (n == 0) n = 1;
	size_t need = (n + HDR + 7) & ~(size_t)7;
	if (need < MIN_BLOCK) need = MIN_BLOCK;
	for (int i = 0; i < region_count; i++) {
		block_t *b = (block_t *)regions[i].start;
		while ((uint8_t *)b < regions[i].end) {
			size_t s = bsize(b);
			if (!(b->size & USED) && s >= need) {
				if (s - need >= MIN_BLOCK) {
					block_t *rest = (block_t *)((uint8_t *)b + need);
					rest->prev_size = need;
					set_size(rest, s - need, 0);
					set_size(b, need, 1);
				} else {
					set_size(b, s, 1);
				}
				used_bytes += bsize(b);
				if (used_bytes > peak_bytes) peak_bytes = used_bytes;
				return (uint8_t *)b + HDR;
			}
			b = next_block(b);
		}
	}
	return 0;
}

void hd_free(void *p)
{
	if (!p) return;
#ifdef HOST_BUILD
	if (heap_trace > 0) fprintf(stderr, "F %p\n", p);
#endif
	block_t *b = (block_t *)((uint8_t *)p - HDR);
	used_bytes -= bsize(b);
	size_t s = bsize(b);
	block_t *n = next_block(b);
	if (!(n->size & USED)) s += bsize(n);
	if (b->prev_size) {
		block_t *prev = (block_t *)((uint8_t *)b - b->prev_size);
		if (!(prev->size & USED)) {
			s += bsize(prev);
			b = prev;
		}
	}
	set_size(b, s, 0);
}

void *hd_calloc(size_t count, size_t size)
{
#ifdef HOST_BUILD
	heap_log_caller = __builtin_return_address(0);
#endif
	size_t n = count * size;
	void *p = hd_malloc(n);
	if (p) memset(p, 0, n);
	return p;
}

void *hd_realloc(void *p, size_t n)
{
	if (!p) return hd_malloc(n);
	if (n == 0) {
		hd_free(p);
		return 0;
	}
	block_t *b = (block_t *)((uint8_t *)p - HDR);
	size_t have = bsize(b) - HDR;
	if (have >= n) return p;
#ifdef HOST_BUILD
	heap_log_caller = __builtin_return_address(0);
#endif
	void *q = hd_malloc(n);
	if (!q) return 0;
	memcpy(q, p, have);
	hd_free(p);
	return q;
}

char *hd_strdup(const char *s)
{
	size_t n = strlen(s) + 1;
	char *p = hd_malloc(n);
	if (p) memcpy(p, s, n);
	return p;
}

size_t hd_heap_used(void) { return used_bytes; }
size_t hd_heap_peak(void) { return peak_bytes; }
size_t hd_heap_total(void) { return total_bytes; }

size_t hd_heap_largest_free(void)
{
	size_t best = 0;
	for (int i = 0; i < region_count; i++)
		for (block_t *b = (block_t *)regions[i].start; (uint8_t *)b < regions[i].end; b = next_block(b))
			if (!(b->size & USED) && bsize(b) > best) best = bsize(b);
	return best > HDR ? best - HDR : 0;
}

/* The engine's malloc & co (hd_glue.h): hode rarely checks for NULL, so
   running out of memory stops here with a message instead of a crash. */
#include "hd_plat.h"

static void out_of_memory(size_t n)
{
	char msg[96];
	snprintf(msg, sizeof msg, "out of memory (%lu bytes, largest free %lu)", (unsigned long)n,
	         (unsigned long)hd_heap_largest_free());
	hd_plat_fatal(msg);
}

void *hd_engine_malloc(size_t n)
{
#ifdef HOST_BUILD
	if (!heap_log_caller) heap_log_caller = __builtin_return_address(0);
#endif
	void *p = hd_malloc(n);
	if (!p) out_of_memory(n);
	return p;
}

void *hd_engine_calloc(size_t count, size_t size)
{
#ifdef HOST_BUILD
	heap_log_caller = __builtin_return_address(0);
#endif
	void *p = hd_calloc(count, size);
	if (!p && count && size) out_of_memory(count * size);
	return p;
}

void *hd_engine_realloc(void *p, size_t n)
{
	void *q = hd_realloc(p, n);
	if (!q && n) out_of_memory(n);
	return q;
}
