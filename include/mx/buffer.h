
#ifndef __MX_BUFFER_H_
#define __MX_BUFFER_H_


#include <stddef.h>
#include <stdbool.h>



struct buffer
{
    unsigned char *alloc;       // Pointer to allocated memory
    unsigned char *data;        // Pointer to data stored in buffer
    size_t length;              // Length of data stored in buffer
    size_t size;                // Allocated memory size
};



struct buffer* buffer_new(void);
struct buffer* buffer_create(size_t size);
struct buffer* buffer_delete(struct buffer *self);

void buffer_init(struct buffer *self, size_t size);
void buffer_clean(struct buffer *self);
void buffer_copy(struct buffer *self, struct buffer *other);

void buffer_rewind(struct buffer *self);
size_t buffer_resize(struct buffer *self, size_t size);

void buffer_append(struct buffer *self, const void *data, size_t len);
size_t buffer_take(struct buffer *self, void *data, size_t len);



/**
 * Reset buffer
 *
 */
static inline void buffer_reset(struct buffer *self)
{
    self->data = self->alloc;
    self->length = 0;
}


/**
 * Cut buffer
 *
 */
static inline size_t buffer_cut(struct buffer *self, size_t len)
{
    self->data += len;
    self->length -= len;

    return self->length;
}


/**
 * Check if buffer is empty
 *
 */
static inline bool buffer_is_empty(struct buffer *self)
{
    return self->length == 0 ? true : false;
}



#endif /* __MX_BUFFER_H_ */
