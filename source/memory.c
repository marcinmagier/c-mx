
#include "mx/memory.h"
#include "mx/log.h"

#include <stdlib.h>
#include <string.h>



/**
 * free() wrapper
 *
 * May be used to free and clean pointer simultaneously
 * e.g. ptr = xfree(ptr);
 *
 */
void* xfree(void *ptr)
{
    free(ptr);
    return NULL;
}


/**
 * malloc() wrapper
 *
 */
void* xmalloc(size_t size)
{
    void *ret = malloc(size);
    if (!ret)
        RESET("Out of memory");

    return ret;
}


/**
 * calloc() wrapper
 *
 */
void* xcalloc(size_t count, size_t size)
{
    void *ret = calloc(count, size);
    if (!ret)
        RESET("Out of memory");

    return ret;
}


/**
 * realloc() wrapper
 *
 */
void* xrealloc(void *ptr, size_t size)
{
    void *ret = realloc(ptr, size);
    if (!ret)
        RESET("Out of memory");

    return ret;
}


/**
 * xmalloc() that allocates size+1 bytes and zeroes the last byte
 *
 */
void* xmallocz(size_t size)
{
    size_t total = size+1;
    void *ret = xmalloc(total);
    ((char*)ret)[size] = 0;

    return ret;
}


/**
 * Duplicate a chunk of memory using xmalloc
 *
 */
void* xmemdup(const void *ptr, size_t size)
{
    return memcpy(xmalloc(size), ptr, size);
}


/**
 * Duplicate a chunk of memory using xmallocz
 *
 */
void* xmemdupz(const void *ptr, size_t size)
{
    return memcpy(xmallocz(size), ptr, size);
}
