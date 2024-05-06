
#ifndef __MX_PRIVATE_STREAM_H_
#define __MX_PRIVATE_STREAM_H_


#include "mx/stream.h"
#include "mx/buffer.h"
#include "mx/observer.h"
#include "mx/queue.h"
#include "mx/log.h"

#include <stdio.h>



#if (DEBUG_STREAM >= 1)
  #define STREAM_LOG(...)                       _LOG_(LOG_COLOR_CYAN, __VA_ARGS__)
  #if (DEBUG_STREAM >= 2)
    #define STREAM_LOG_DATA(prefix, data, len)  _LOG_DATA_(LOG_COLOR_BLUE, prefix, data, len)
  #else
    #define STREAM_LOG_DATA(...)
  #endif
#else
  #define STREAM_LOG(...)
  #define STREAM_LOG_DATA(...)
#endif



#define STREAM_CLS          0
#define STREAM_SSL_CLS      1
#define STREAM_WS_CLS       2
#define STREAM_MQTT_CLS     3




enum stream_notify_type_en
{
    NOTIFY_DISABLED,
    NOTIFY_ON_STREAM_READY,
    NOTIFY_ON_WS_MSG_RECEIVED,
    NOTIFY_ON_MQTT_MSG_RECEIVED,
};






typedef void* (*stream_destructor_fn)(struct stream *self);
typedef ssize_t (*stream_read_fn)(struct stream *self, void *buffer, size_t length);
typedef ssize_t (*stream_write_fn)(struct stream *self, const void *buffer, size_t length);
typedef int     (*stream_flush_fn)(struct stream *self);
typedef int     (*stream_time_fn)(struct stream *self);

struct stream_vtable
{
    stream_destructor_fn destructor_fn;
    stream_read_fn read_fn;
    stream_write_fn write_fn;
    stream_flush_fn flush_fn;
    stream_time_fn time_fn;
};




struct stream{
    const struct stream_vtable *vtable;
    int rtti;

    int fd;
    int status;

    struct stream *decorated;
    struct observer *observer;

    LIST_ENTRY(stream) _entry_;
    TAILQ_HEAD(stream_buffer_slice_head, buffer_slice) outgoing;

};


void stream_init(struct stream *self, int fd, struct stream *decorated);
void stream_clean(struct stream *self);

struct stream* stream_get_decorated(struct stream *self);


#endif /* __MX_PRIVATE_STREAM_H_ */
