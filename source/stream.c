
#include "mx/stream.h"
#include "mx/memory.h"
#include "mx/queue.h"
#include "mx/misc.h"

#include "private_stream.h"

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>







struct buffer_slice
{
    struct buffer buffer;

    TAILQ_ENTRY(buffer_slice) _entry_;
};


struct buffer_slice* buffer_slice_new(const void *buffer, size_t length)
{
    struct buffer_slice *self = xmalloc(sizeof(struct buffer_slice));
    buffer_init(&self->buffer, length);
    buffer_append(&self->buffer, buffer, length);
    return self;
}


struct buffer_slice* buffer_slice_delete(struct buffer_slice *self)
{
    buffer_clean(&self->buffer);
    return xfree(self);
}







static void* stream_destructor_impl(struct stream*);
static ssize_t stream_read_impl(struct stream *self, void *buffer, size_t length);
static ssize_t stream_write_impl(struct stream *self, const void *buffer, size_t length);
static int     stream_flush_impl(struct stream *self);
static int     stream_time_impl(struct stream *self);

static const struct stream_vtable stream_vtable = {
        .destructor_fn = stream_destructor_impl,
        .read_fn = stream_read_impl,
        .write_fn = stream_write_impl,
        .flush_fn = stream_flush_impl,
        .time_fn = stream_time_impl,
};



/**
 * Stream initializer
 *
 */
void stream_init(struct stream *self, int fd, struct stream *decorated)
{
    self->fd = fd;
    self->decorated = decorated;
    self->observer = NULL;

    // Stream is ready if file descriptor is defined
    self->status = (fd >= 0) ? STREAM_ST_READY : STREAM_ST_INIT;

    TAILQ_INIT(&self->outgoing);
}


/**
 * Stream cleaner
 *
 */
void stream_clean(struct stream *self)
{
    stream_reset_outgoing_data(self);
    stream_remove_observer(self);

    self->fd = -1;

    if (self->decorated)
        self->decorated = stream_delete(self->decorated);
}


/**
 * Stream constructor
 *
 */
struct stream* stream_new(int fd)
{
    struct stream *self = xmalloc(sizeof(struct stream));
    self->rtti = STREAM_CLS;
    self->vtable = &stream_vtable;

    stream_init(self, fd, NULL);
    return self;
}


/**
 * Stream destructor
 *
 */
struct stream* stream_delete(struct stream *self)
{
    // Call virtual destructor
    return self->vtable->destructor_fn(self);
}





/**
 * Return file descriptor
 *
 */
int stream_get_fd(struct stream *self)
{
    if (self->decorated)
        return stream_get_fd(self->decorated);

    return self->fd;
}


/**
 * Set new status for the stream
 *
 */
void stream_set_status(struct stream *self, int status)
{
    self->status = status;
}


/**
 * Get current status of the stream
 *
 */
int stream_get_status(struct stream *self)
{
    return self->status;
}


/**
 * Return decorated stream
 *
 */
struct stream* stream_get_decorated(struct stream *self)
{
    return self->decorated;
}


/**
 * Add stream observer
 *
 */
void stream_set_observer(struct stream *self, void *object, stream_on_ready_clbk handler)
{
    if (!self->observer)
        self->observer = observer_create(object, 0, (observer_notify_fn)handler);
}


/**
 * Remove stream observer
 *
 */
void stream_remove_observer(struct stream *self)
{
    if (self->observer)
        self->observer = observer_delete(self->observer);
}


/**
 * Read function
 *
 */
ssize_t stream_real_read(struct stream *self, void *buffer, size_t length)
{
    if (self->decorated)
        return stream_read(self->decorated, buffer, length);

    ssize_t ret = read(self->fd, buffer, length);

    STREAM_LOG("- s:read %ld", ret);
    STREAM_LOG_DATA("s:rd ", buffer, ret);
    return ret;
}


/**
 * Write function
 *
 */
ssize_t stream_real_write(struct stream *self, const void *buffer, size_t length)
{
    if (self->decorated)
        return stream_write(self->decorated, buffer, length);

    ssize_t ret = write(self->fd, buffer, length);

    STREAM_LOG("- s:write %ld", ret);
    STREAM_LOG_DATA("s:wr ", buffer, ret);
    return ret;
}


/**
 * Queue outgoing data
 *
 */
static size_t stream_queue_outgoing_data(struct stream *self, const void *buffer, size_t length)
{
    struct buffer_slice *slice = buffer_slice_new(buffer, length);
    TAILQ_INSERT_TAIL(&self->outgoing, slice, _entry_);

    STREAM_LOG("- s:queue %lu", length);
    return length;
}


/**
 * Check if outgoing data is available
 *
 */
bool stream_has_outgoing_data(struct stream *self)
{
    if (!self->decorated || (stream_get_status(self->decorated) == STREAM_ST_READY) ) {
        // In case of decoration we may push our data only if decorated stream is ready
        if (!TAILQ_EMPTY(&self->outgoing))
            return true;
    }

    return self->decorated ? stream_has_outgoing_data(self->decorated) : false;
}


/**
 * Reset outgoing data
 *
 */
void stream_reset_outgoing_data(struct stream *self)
{
    if (self->decorated)
        stream_reset_outgoing_data(self->decorated);

    struct buffer_slice *slice, *tmp;
    TAILQ_FOREACH_SAFE(slice, &self->outgoing, _entry_, tmp) {
        TAILQ_REMOVE(&self->outgoing, slice, _entry_);
        buffer_slice_delete(slice);
    }
}


/**
 * Handle outgoing data
 *
 */
int stream_handle_outgoing_data(struct stream *self)
{
    int written = 0;

    if (self->decorated)
        written = stream_handle_outgoing_data(self->decorated);

    while (!TAILQ_EMPTY(&self->outgoing)) {
        struct buffer_slice *slice = TAILQ_FIRST(&self->outgoing);

        ssize_t ret = stream_real_write(self, slice->buffer.data, slice->buffer.length);
        if (ret <= 0) {
            return ret;
        }

        written += ret;

        if (buffer_cut(&slice->buffer, ret) > 0)
            break;      // Something is still in queue

        // Entire slice was used
        TAILQ_REMOVE(&self->outgoing, slice, _entry_);
        buffer_slice_delete(slice);
    }

    return written;
}


/**
 * Handle incomming data
 *
 * Currently just notify observers
 *
 */
int stream_handle_incoming_data(struct stream *self)
{
    int ret = 0;

    if (observer_is_available(self->observer)) {
        stream_on_ready_clbk handler = (stream_on_ready_clbk)self->observer->notify_handler;
        ret = handler(self->observer->object, self);
    }

    return ret;
}



/**
 * Stream non-virtual read
 */
ssize_t stream_do_read(struct stream *self, void *buffer, size_t length)
{
    return stream_real_read(self, buffer, length);
}


/**
 * Stream non-virtual write
 *
 */
ssize_t stream_do_write(struct stream *self, const void *buffer, size_t length)
{
    if (!TAILQ_EMPTY(&self->outgoing))
        return stream_queue_outgoing_data(self, buffer, length);

    int ret = stream_real_write(self, buffer, length);
    if (ret < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            stream_queue_outgoing_data(self, buffer, length);
            return length;
        }

        return ret;
    }

    if ((unsigned long)ret < length)
        stream_queue_outgoing_data(self, (unsigned char *)buffer+ret, length-ret);

    return length;
}


/**
 * Stream non-virtual flush
 *
 * Flush stream outgoing data
 *
 */
int stream_do_flush(struct stream *self)
{
    while (!TAILQ_EMPTY(&self->outgoing)) {
        int ret = stream_handle_outgoing_data(self);
        if (ret == 0)
            return ret;
        if ((ret < 0) && (errno != EAGAIN && errno != EWOULDBLOCK))
            return ret;
    }

    if (self->decorated) {
        return stream_flush(self->decorated);
    }

    return 1;
}


/**
 * Stream non-virtual time handler
 *
 */
int stream_do_time(struct stream *self)
{
    if (self->decorated) {
        return stream_time(self->decorated);
    }

    return 1;
}


/**
 * Stream virtual read
 *
 */
ssize_t stream_read(struct stream *self, void *buffer, size_t length)
{
    return self->vtable->read_fn(self, buffer, length);
}


/**
 * Stream virtual write
 *
 */
ssize_t stream_write(struct stream *self, const void *buffer, size_t length)
{
    return self->vtable->write_fn(self, buffer, length);
}


/**
 * Stream virtual flush
 *
 */
int stream_flush(struct stream *self)
{
    return self->vtable->flush_fn(self);
}


/**
 * Stream virtual time handler
 *
 */
int stream_time(struct stream *self)
{
    return self->vtable->time_fn(self);
}







////// Virtual function definitions for stream class

/**
 * Stream virtual destructor implementation
 *
 */
void* stream_destructor_impl(struct stream *self)
{
    stream_clean(self);
    return xfree(self);
}


/**
 * Stream virtual read implementation
 *
 */
ssize_t stream_read_impl(struct stream *self, void *buffer, size_t length)
{
    return stream_do_read(self, buffer, length);
}


/**
 * Stream virtual write implementation
 */
ssize_t stream_write_impl(struct stream *self, const void *buffer, size_t length)
{
    return stream_do_write(self, buffer, length);
}


/**
 * Stream virtual flush implementation
 */
int stream_flush_impl(struct stream *self)
{
    return stream_do_flush(self);
}


/**
 * Stream virtual time handler implementation
 */
int stream_time_impl(struct stream *self)
{
    return stream_do_time(self);
}
