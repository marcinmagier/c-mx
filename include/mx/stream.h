
#ifndef __MX_STREAM_H_
#define __MX_STREAM_H_



#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>
#include <errno.h>



typedef long esize_t;


enum stream_status_e
{
    STREAM_ST_INIT,     // Prepering stream, e.g. ssl handshake pending
    STREAM_ST_READY,
    STREAM_ST_CLOSING,  // Stream is going to be closed soon
    STREAM_ST_CLOSED,
};


struct stream;
struct observer;


typedef int (*stream_on_ready_clbk)(void *object, struct stream *stream);



struct stream* stream_new(int fd);
struct stream* stream_delete(struct stream *self);

int stream_get_fd(struct stream *self);

void stream_set_status(struct stream *self, int status);
int stream_get_status(struct stream *self);

void stream_set_observer(struct stream *self, void *object, stream_on_ready_clbk handler);
void stream_remove_observer(struct stream *self);

bool stream_has_outgoing_data(struct stream *self);
void stream_reset_outgoing_data(struct stream *self);
int stream_handle_outgoing_data(struct stream *self);
int stream_handle_incoming_data(struct stream *self);


// Virtual functions
ssize_t stream_read(struct stream *self, void *buffer, size_t length);
ssize_t stream_write(struct stream *self, const void *buffer, size_t length);
int     stream_flush(struct stream *self);
int     stream_time(struct stream *self);

// Non-virtual functions
ssize_t stream_do_read(struct stream *self, void *buffer, size_t length);
ssize_t stream_do_write(struct stream *self, const void *buffer, size_t length);
int     stream_do_flush(struct stream *self);
int     stream_do_time(struct stream *self);




static inline bool stream_try_again(ssize_t status) {
    return ((status < 0) && (errno == EAGAIN || errno == EWOULDBLOCK)) ? true : false;
}



#endif /* __MX_STREAM_H_ */
