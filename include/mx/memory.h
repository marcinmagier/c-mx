
#ifndef __MX_MEMORY_H_
#define __MX_MEMORY_H_


#include <stddef.h>


void* xfree(void *ptr);

void* xmalloc(size_t size);
void* xcalloc(size_t count, size_t size);
void* xrealloc(void *ptr, size_t size);
void* xmallocz(size_t size);

void* xmemdup(const void *ptr, size_t size);
void* xmemdupz(const void *ptr, size_t size);


#endif /* __MX_MEMORY_H_ */
