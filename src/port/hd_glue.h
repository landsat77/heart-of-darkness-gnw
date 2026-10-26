/* Included first in every hode file (-include): its allocations go to the
   port's heap (hd_heap.c), on the console and on the PC alike. */
#ifndef HD_GLUE_H
#define HD_GLUE_H

#include <stdlib.h>
#include <string.h>
#include "hd_heap.h"

#define malloc(n) hd_engine_malloc(n)
#define calloc(n, s) hd_engine_calloc(n, s)
#define realloc(p, n) hd_engine_realloc(p, n)
#define free(p) hd_free(p)
#define strdup(s) hd_strdup(s)

#endif
