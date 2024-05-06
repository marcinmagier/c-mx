
#ifndef __MX_STREAM_WS_H_
#define __MX_STREAM_WS_H_


#include "mx/stream.h"
#include "mx/websocket.h"



struct stream_ws* stream_ws_new(struct stream *decorated);
struct stream_ws* stream_ws_delete(struct stream_ws *self);

struct stream* stream_ws_to_stream(struct stream_ws *self);
struct stream_ws* stream_ws_from_stream(struct stream *self);


static inline int stream_ws_get_fd(struct stream_ws *self) {
    return stream_get_fd(stream_ws_to_stream(self));
}

static inline void stream_ws_set_status(struct stream_ws *self, int status) {
    stream_set_status(stream_ws_to_stream(self), status);
}
static inline int stream_ws_get_status(struct stream_ws *self) {
    return stream_get_status(stream_ws_to_stream(self));
}

static inline ssize_t stream_ws_read(struct stream_ws *self, void *buffer, size_t length) {
    return stream_read(stream_ws_to_stream(self), buffer, length);
}

static inline ssize_t stream_ws_write(struct stream_ws *self, const void *buffer, size_t length) {
    return stream_write(stream_ws_to_stream(self), buffer, length);
}

// Non-virtual functions
ssize_t stream_ws_do_read(struct stream_ws *self, void *buffer, size_t length);
ssize_t stream_ws_do_write(struct stream_ws *self, const void *buffer, size_t length);


void stream_ws_connect(struct stream_ws *self, const char *uri, const char *key, const char *header);
void stream_ws_disconnect(struct stream_ws *self, unsigned short code);


ssize_t stream_ws_peek_frame(struct stream_ws *self);
ssize_t stream_ws_read_frame(struct stream_ws *self, unsigned char *fin, unsigned char *opcode,
                                                      void *buffer, size_t length);
ssize_t stream_ws_write_frame(struct stream_ws *self, unsigned char fin, unsigned char opcode,
                                                      const unsigned char *mask,
                                                      const void *buffer, size_t length);

#endif /* __MX_STREAM_SSL_H_ */
