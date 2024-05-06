


#include "mx/idler.h"
#include "mx/stream.h"

#include "mx/log.h"
#include "mx/memory.h"
#include "mx/queue.h"
#include "mx/misc.h"

#include "private_stream.h"

#include <errno.h>
#include <stdlib.h>
#include <sys/select.h>
#include <time.h>

#include <stdio.h>
#include <unistd.h>
#include <string.h>






struct idler
{
    fd_set read_fds;
    fd_set write_fds;

    LIST_HEAD(stream_head, stream) streams;
};



/**
 * Constructor
 *
 */
struct idler* idler_new(void)
{
    struct idler *self = xmalloc(sizeof(struct idler));

    LIST_INIT(&self->streams);

    return self;
}


/**
 * Destructor
 *
 */
struct idler* idler_delete(struct idler *self)
{
    struct stream *stream, *tmp;
    LIST_FOREACH_SAFE(stream, &self->streams, _entry_, tmp) {
        LIST_REMOVE(stream, _entry_);
    }

    free(self);
    return NULL;
}


void idler_add_stream(struct idler *self, struct stream *stream)
{
    LIST_INSERT_HEAD(&self->streams, stream, _entry_);
}


void idler_remove_stream(struct idler *self, struct stream *stream)
{
    UNUSED(self);
    LIST_REMOVE(stream, _entry_);
}


struct stream* idler_find_stream(struct idler *self, int fd)
{
    struct stream *tmp;
    LIST_FOREACH(tmp, &self->streams, _entry_) {
        if (stream_get_fd(tmp) == fd)
            return tmp;
    }

    return NULL;
}


static int idler_prepare(struct idler *self, fd_set *readfds, fd_set *writefds)
{
    int max_fd = 0;

    FD_ZERO(readfds);
    FD_ZERO(writefds);

    struct stream *tmp;
    LIST_FOREACH(tmp, &self->streams, _entry_) {
        int fd = stream_get_fd(tmp);
        if (fd > max_fd)
            max_fd = fd;

        FD_SET(fd, readfds);

        if (stream_has_outgoing_data(tmp))
            FD_SET(fd, writefds);
    }

    return max_fd;
}


int idler_wait(struct idler *self, unsigned long time_ms)
{
    struct timeval tv;

    tv.tv_sec = time_ms/1000;
    tv.tv_usec = (time_ms%1000)*1000;

    int max_fd = idler_prepare(self, &self->read_fds, &self->write_fds);

    int ret = select(max_fd+1, &self->read_fds, &self->write_fds, NULL, &tv);
    if (ret > 0)
        return IDLER_OPERATION;
    if (ret == 0)
        return IDLER_TIMEOUT;

    if (errno == EINTR)
        return IDLER_INTERRUPT;

    return IDLER_ERROR;
}


unsigned int idler_get_stream_status(struct idler *self, struct stream *stream)
{
    unsigned int status = 0;

    if (FD_ISSET(stream_get_fd(stream), &self->write_fds))
        status |= STREAM_OUTGOING_READY;
    if (FD_ISSET(stream_get_fd(stream), &self->read_fds))
        status |= STREAM_INCOMING_READY;

    return status;
}


/**
 * Stream iterator
 *
 * To start iteration pass NULL as 'prev' argument
 *
 */
struct stream* idler_get_next_stream(struct idler *self, struct stream *prev, unsigned int *status)
{
    unsigned int current_status = 0;
    struct stream *current = prev ? LIST_NEXT(prev, _entry_) : LIST_FIRST(&self->streams);

    while (current) {
        if (FD_ISSET(stream_get_fd(current), &self->write_fds))
            current_status |= STREAM_OUTGOING_READY;
        if (FD_ISSET(stream_get_fd(current), &self->read_fds))
            current_status |= STREAM_INCOMING_READY;

        if (current_status) {
            // Stream needs attention
            *status = current_status;
            break;
        }

        current = LIST_NEXT(current, _entry_);
    }

    return current;
}
