
#ifndef __MX_STREAM_SSL_H_
#define __MX_STREAM_SSL_H_


#include "mx/stream.h"



struct ssl;

struct stream_ssl* stream_ssl_new(struct ssl *ssl, struct stream *decorated);
struct stream_ssl* stream_ssl_delete(struct stream_ssl *self);

struct stream* stream_ssl_to_stream(struct stream_ssl *self);
struct stream_ssl* stream_ssl_from_stream(struct stream *self);

static inline int stream_ssl_get_fd(struct stream_ssl *self) {
    return stream_get_fd(stream_ssl_to_stream(self));
}


static inline ssize_t stream_ssl_read(struct stream_ssl *self, void *buffer, size_t length) {
    return stream_read(stream_ssl_to_stream(self), buffer, length);
}

static inline ssize_t stream_ssl_write(struct stream_ssl *self, const void *buffer, size_t length) {
    return stream_write(stream_ssl_to_stream(self), buffer, length);
}

// Non-virtual functions
ssize_t stream_ssl_do_read(struct stream_ssl *self, void *buffer, size_t length);
ssize_t stream_ssl_do_write(struct stream_ssl *self, const void *buffer, size_t length);


void stream_ssl_accept(struct stream_ssl *self);
void stream_ssl_connect(struct stream_ssl *self);
void stream_ssl_renegotiate(struct stream_ssl *self);


#endif /* __MX_STREAM_SSL_H_ */
