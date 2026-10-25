/* C++ runtime pieces: new / delete on the port's heap (hd_heap.c). */
#include <stddef.h>
#include <new>

#include "hd_heap.h"
#include "hd_plat.h"

void *operator new(size_t n) {
#ifdef HOST_BUILD
	hd_heap_set_caller(__builtin_return_address(0));
#endif
	void *p = hd_malloc(n);
	if (!p) hd_plat_fatal("out of memory (new)");
	return p;
}
void *operator new[](size_t n) {
#ifdef HOST_BUILD
	hd_heap_set_caller(__builtin_return_address(0));
#endif
	void *p = hd_malloc(n);
	if (!p) hd_plat_fatal("out of memory (new[])");
	return p;
}
void operator delete(void *p) noexcept { hd_free(p); }
void operator delete[](void *p) noexcept { hd_free(p); }
void operator delete(void *p, size_t) noexcept { hd_free(p); }
void operator delete[](void *p, size_t) noexcept { hd_free(p); }

#ifndef HOST_BUILD
extern "C" void __cxa_pure_virtual() { hd_plat_fatal("pure virtual call"); }
extern "C" int __cxa_atexit(void (*)(void *), void *, void *) { return 0; }
void *__dso_handle = 0;
#endif
