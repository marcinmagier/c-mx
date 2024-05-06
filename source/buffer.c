
#include "mx/buffer.h"
#include "mx/memory.h"
#include "mx/misc.h"

#include <string.h>


#define BUFFER_INITIAL_SIZE     128





/**
 * Constructor
 *
 */
struct buffer* buffer_new(void)
{
    return buffer_create(BUFFER_INITIAL_SIZE);
}


/**
 * Constructor with arguments
 *
 * - initial size
 *
 */
struct buffer* buffer_create(size_t size)
{
    struct buffer *self = xmalloc(sizeof(struct buffer));
    buffer_init(self, size);
    return self;
}


/**
 * Destructor
 *
 */
struct buffer* buffer_delete(struct buffer *self)
{
    buffer_clean(self);
    return xfree(self);
}


/**
 * Initialize buffer
 *
 */
void buffer_init(struct buffer *self, size_t size)
{
    self->size = size;
    self->alloc = xmalloc(self->size);
    self->data = self->alloc;
    self->length = 0;
}


/**
 * Clean buffer
 *
 */
void buffer_clean(struct buffer *self)
{
    if (self->alloc)
        self->alloc = xfree(self->alloc);

    self->data = NULL;
    self->size = 0;
    self->length = 0;
}


/**
 * Copy buffer
 *
 */
void buffer_copy(struct buffer *self, struct buffer *other)
{
    buffer_resize(self, other->length);
    memcpy(self->data, other->data, other->length);
    self->length = other->length;
}


/**
 * Bring data pointer to the beginning of the buffer
 *
 */
void buffer_rewind(struct buffer *self)
{
    if (self->alloc != self->data) {
        if (self->length)
            memmove(self->alloc, self->data, self->length);
        self->data = self->alloc;
    }
}


/**
 * Make sure buffer is long enough to store given size of data
 *
 */
size_t buffer_resize(struct buffer *self, size_t size)
{
    buffer_rewind(self);

    size_t expected_size = self->length + size;
    if (self->size < expected_size) {
        // Resizing needed
        self->size = expected_size;
        self->alloc = xrealloc(self->alloc, self->size);
        self->data = self->alloc;
    }
    return self->size;
}


/**
 * Append data to the buffer
 *
 */
void buffer_append(struct buffer *self, const void *data, size_t len)
{
    buffer_resize(self, len);
    memcpy(self->data + self->length, data, len);
    self->length += len;
}


/**
 * Take data from the buffer
 *
 */
size_t buffer_take(struct buffer *self, void *data, size_t len)
{
    size_t chunk_len = MIN(self->length, len);
    if (chunk_len > 0) {
        memcpy(data, self->data, chunk_len);
        buffer_cut(self, chunk_len);
    }
    return chunk_len;
}
