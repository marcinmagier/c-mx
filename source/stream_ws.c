
#include "mx/stream_ws.h"
#include "mx/stream.h"

#include "mx/misc.h"
#include "mx/log.h"
#include "mx/memory.h"
#include "mx/string.h"
#include "mx/misc.h"
#include "mx/base64.h"
#include "mx/websocket.h"
#include "mx/http.h"
#include "mx/buffer.h"
#include "mx/rand.h"
#include "mx/timer.h"

#include "private_stream.h"

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>



#ifdef DEBUG_STREAM_WS
  #define STREAM_WS_LOG_DATA(prefix, data, len)  _LOG_DATA_(LOG_COLOR_BLUE, prefix, data, len)
#else
  #define STREAM_WS_LOG_DATA(...)
#endif



#define WS_KEY_BUFFER_SIZE              128
#define WS_MESSAGE_BUFFER_SIZE          4096

#define WS_KEEP_ALIVE_SERVER_TIMEOUT    100
#define WS_KEEP_ALIVE_CLIENT_TIMEOUT    90



struct ws_frame_slice
{
    struct buffer buffer;

    unsigned char fin;
    unsigned char opcode;

    TAILQ_ENTRY(ws_frame_slice) _entry_;
};





struct ws_frame_slice* ws_frame_slice_new(const void *buffer, size_t length)
{
    struct ws_frame_slice *self = xmalloc(sizeof(struct ws_frame_slice));
    buffer_init(&self->buffer, length);
    buffer_append(&self->buffer, buffer, length);
    return self;
}


struct ws_frame_slice* ws_frame_slice_delete(struct ws_frame_slice *self)
{
    buffer_clean(&self->buffer);
    return xfree(self);
}







static void* stream_ws_destructor_impl(struct stream *stream);
static ssize_t stream_ws_read_impl(struct stream *stream, void *buffer, size_t length);
static ssize_t stream_ws_write_impl(struct stream *stream, const void *buffer, size_t length);
static int     stream_ws_flush_impl(struct stream *stream);
static int     stream_ws_time_impl(struct stream *stream);

static const struct stream_vtable stream_ws_vtable = {
        .destructor_fn = stream_ws_destructor_impl,
        .read_fn = stream_ws_read_impl,
        .write_fn = stream_ws_write_impl,
        .flush_fn = stream_ws_flush_impl,
        .time_fn = stream_ws_time_impl,
};





struct stream_ws
{
    struct stream stream;
    struct buffer buffer;

    bool client_role;

    TAILQ_HEAD(ws_frame_slice_head, ws_frame_slice) cache;

    unsigned char data_type;
    char *key;
    unsigned char *mask;
    unsigned char mask_data[4];


    unsigned int keep_alive;
    bool keep_alive_responded;
    struct timer keep_alive_timer;
};





/**
 * Websocket stream initializer
 *
 */
void stream_ws_init(struct stream_ws *self)
{
    buffer_init(&self->buffer, WS_MESSAGE_BUFFER_SIZE);
    TAILQ_INIT(&self->cache);

    self->client_role = false;
    self->data_type = WS_OPCODE_BINARY;
    self->mask = NULL;      // Needs to be NULL for server role
    self->key = NULL;

    self->keep_alive = 0;   // Disable keep alive
    self->keep_alive_responded = true;
    timer_stop(&self->keep_alive_timer);
}


/**
 * Websocket stream cleaner
 *
 */
void stream_ws_clean(struct stream_ws *self)
{
    buffer_clean(&self->buffer);

    if (self->key)
        self->key = xfree(self->key);

    struct ws_frame_slice *slice, *tmp;
    TAILQ_FOREACH_SAFE(slice, &self->cache, _entry_, tmp) {
        TAILQ_REMOVE(&self->cache, slice, _entry_);
        ws_frame_slice_delete(slice);
    }
}


/**
 * Websocket stream constructor
 *
 */
struct stream_ws* stream_ws_new(struct stream *decorated)
{
    struct stream_ws *self = xmalloc(sizeof(struct stream_ws));
    self->stream.rtti = STREAM_WS_CLS;
    self->stream.vtable = &stream_ws_vtable;

    stream_init(&self->stream, -1, decorated);
    stream_ws_init(self);
    return self;
}


/**
 * Websocket stream destructor
 *
 */
struct stream_ws* stream_ws_delete(struct stream_ws *self)
{
    // Call virtual destructor
    return self->stream.vtable->destructor_fn(&self->stream);
}





/**
 * Cast websocket stream to base class
 *
 */
struct stream* stream_ws_to_stream(struct stream_ws *self)
{
    return &self->stream;
}


/**
 * Cast websocket stream from base class
 *
 */
struct stream_ws* stream_ws_from_stream(struct stream *self)
{
    if (self->rtti == STREAM_WS_CLS)
        return (struct stream_ws*)self;

    return NULL;
}


/**
 * Return decorated stream
 *
 */
struct stream* stream_ws_get_decorated(struct stream_ws *self)
{
    return stream_get_decorated(&self->stream);
}


/**
 * Generate new mask
 *
 */
void stream_ws_generate_mask(struct stream_ws *self)
{
    if (self->client_role) {
        self->mask = self->mask_data;
        rand_data(self->mask_data, sizeof(self->mask_data));
    }
    else {
        self->mask = NULL;
    }
}


/**
 * Handle handshake request received from client
 *
 */
int stream_ws_handle_handshake_request(struct stream_ws *self)
{
    struct http_msg_view http_view;
    if (http_msg_view_parse_request(&http_view, (const char*)self->buffer.data)) {

        char *key = NULL;
        if (!http_header_find(http_view.headers, "Sec-WebSocket-Key", (const char**)&key)) {
            WARN("Sec-WebSocket-Key header not found");
            buffer_reset(&self->buffer);
            return 0;   // Disconnect
        }

        size_t key_len = xstr_word_len(key, NULL);
        key[key_len] = '\0';
        INFO("WebSocket request with %s", key);

        char accept_key[WS_KEY_BUFFER_SIZE];
        if (!ws_calculate_accept_key(key, key_len, accept_key, sizeof(accept_key))) {
            ERROR("Calculating Sec-WebSocket-Accept failed");
            buffer_reset(&self->buffer);
            return 0;   // Disconnect
        }

        char response[WS_MESSAGE_BUFFER_SIZE];
        snprintf(response, sizeof(response), "HTTP/1.1 101 Switching Protocols\r\n"
                                             "Upgrade: websocket\r\n"
                                             "Connection: Upgrade\r\n"
                                             "Sec-WebSocket-Accept: %s\r\n\r\n", accept_key);
        stream_do_write(stream_ws_to_stream(self), response, strlen(response));

        // Client connected
        stream_set_status(&self->stream, STREAM_ST_READY);
        buffer_reset(&self->buffer);

        self->keep_alive = WS_KEEP_ALIVE_SERVER_TIMEOUT;
        timer_start(&self->keep_alive_timer, TIMER_SEC, self->keep_alive);
    }

    // No message yet
    errno = EAGAIN;
    return -1;
}


/**
 * Handle handshake response received from server
 *
 */
int stream_ws_handle_handshake_accept(struct stream_ws *self)
{
    struct http_msg_view http_view;
    if (http_msg_view_parse_response(&http_view, (const char*)self->buffer.data)) {

        if (!self->key) {
            WARN("WebSocket handshake not requested");
            buffer_reset(&self->buffer);
            return 0;   // Disconnect
        }

        char *key = NULL;
        if (!http_header_find(http_view.headers, "Sec-WebSocket-Accept", (const char**)&key)) {
            WARN("Sec-WebSocket-Accept header not found");
            buffer_reset(&self->buffer);
            return 0;   // Disconnect
        }

        size_t key_len = xstr_word_len(key, NULL);
        key[key_len] = '\0';
        INFO("WebSocket response with %s", key);

        char accept_key[WS_KEY_BUFFER_SIZE];
        if (!ws_calculate_accept_key(self->key, strlen(self->key), accept_key, sizeof(accept_key))) {
            ERROR("Calculating WebSocket key failed");
            buffer_reset(&self->buffer);
            return 0;   // Disconnect
        }
        if (strncmp(accept_key, key, key_len)) {
            WARN("WebSocket key verification failed");
            buffer_reset(&self->buffer);
            return 0;   // Disconnect
        }

        // Server connected
        stream_set_status(&self->stream, STREAM_ST_READY);
        buffer_reset(&self->buffer);
        self->key = xfree(self->key);   // Not needed anymore

        self->keep_alive = WS_KEEP_ALIVE_CLIENT_TIMEOUT;
        timer_start(&self->keep_alive_timer, TIMER_SEC, self->keep_alive);
    }

    // No message yet
    errno = EAGAIN;
    return -1;
}


/**
 * Handle websocket frame
 *
 */
void stream_ws_handle_received_frame(struct stream_ws *self, struct ws_frame *frame, unsigned char *data)
{
    unsigned char *payload = NULL;
    if (frame->payload_offset > 0)
        payload = data + frame->payload_offset;

    // Unmask payload if necessary
    if (payload && frame->mask_flag) {
        if (self->client_role) {
            // Client MUST close connection if received masked frame
            stream_ws_disconnect(self, WS_CLOSE_PROTOCOL_ERROR);
        }
        else {
            unsigned char *mask = data + frame->mask_offset;
            ws_apply_mask(payload, frame->payload_length, mask);
        }
    }

    switch (frame->opcode) {
        case WS_OPCODE_TEXT:
        case WS_OPCODE_BINARY: {
            // Save received data type as default
            self->data_type = frame->opcode;

            struct ws_frame_slice *slice = ws_frame_slice_new(payload, frame->payload_length);
            slice->fin = frame->fin_flag;
            slice->opcode = frame->opcode;
            TAILQ_INSERT_TAIL(&self->cache, slice, _entry_);
        }   break;

        case WS_OPCODE_CONTINUE: {
            // Append received data to last slice
            struct ws_frame_slice *slice = TAILQ_LAST(&self->cache, ws_frame_slice_head);
            if (slice) {
                buffer_append(&slice->buffer, payload, frame->payload_length);
                slice->fin = frame->fin_flag;
            }
        }   break;

        case WS_OPCODE_CLOSE:
            break;  // Client should disconnect eventually

        case WS_OPCODE_PING:
            stream_ws_generate_mask(self);
            stream_ws_write_frame(self, WS_FIN_FLAG, WS_OPCODE_PONG, self->mask, payload, frame->payload_length);
            timer_start(&self->keep_alive_timer, TIMER_SEC, self->keep_alive);
            break;

        case WS_OPCODE_PONG:
            self->keep_alive_responded = true;
            break;
    }
}


/**
 * Peek for websocket frame
 *
 */
ssize_t stream_ws_peek_frame(struct stream_ws *self)
{
    char inbuffer[WS_MESSAGE_BUFFER_SIZE];

    STREAM_LOG("--- ws:peek");

    ssize_t ret;
    do {
        if (!TAILQ_EMPTY(&self->cache)) {
            struct ws_frame_slice *slice = TAILQ_FIRST(&self->cache);
            if (slice->fin)
                break;  // Something is already waiting
        }

        ret = stream_read(stream_ws_get_decorated(self), inbuffer, sizeof(inbuffer));
        if (ret <= 0) {
            if ((ret == 0) || (errno != EAGAIN && errno != EWOULDBLOCK))
                return ret;     // Error

            // All received data was read
            break;
        }
        else {
            // Received some data
            buffer_append(&self->buffer, inbuffer, ret);

            if (stream_get_status(&self->stream) != STREAM_ST_READY) {
                if (self->client_role)
                    return stream_ws_handle_handshake_accept(self);
                else
                    return stream_ws_handle_handshake_request(self);
            }

            while (!buffer_is_empty(&self->buffer)) {
                struct ws_frame frame;
                size_t frame_length;
                if (!ws_parse_frame(&frame, self->buffer.data, self->buffer.length, &frame_length))
                    break;  // More data is needed

                stream_ws_handle_received_frame(self, &frame, self->buffer.data);
                buffer_cut(&self->buffer, frame_length);
            }
        }
    } while (ret > 0);

    if (!TAILQ_EMPTY(&self->cache)) {
        struct ws_frame_slice *slice = TAILQ_FIRST(&self->cache);
        if (slice->fin)
            return slice->buffer.length;
    }

    errno = EAGAIN;
    return -1;
}


/**
 * Read websocket frame
 *
 */
ssize_t stream_ws_read_frame(struct stream_ws *self, unsigned char *fin, unsigned char *opcode,
                                                      void *buffer, size_t length)
{
     if (TAILQ_EMPTY(&self->cache)) {
         errno = EAGAIN;
         return -1;     // No data received
     }
     struct ws_frame_slice *slice = TAILQ_FIRST(&self->cache);
     if (!slice->fin) {
         errno = EAGAIN;
         return -1;     // Frame is not fully received
     }
     if (slice->buffer.length > length) {
         errno = ENOMEM;
         return -1;     // Bigger buffer needed
     }

     ssize_t ret = buffer_take(&slice->buffer, buffer, length);
     if (fin)
         *fin = slice->fin;
     if (opcode)
         *opcode = slice->opcode;

     TAILQ_REMOVE(&self->cache, slice, _entry_);
     ws_frame_slice_delete(slice);
     return ret;
}


/**
 * Write websocket frame
 *
 */
ssize_t stream_ws_write_frame(struct stream_ws *self, unsigned char fin, unsigned char opcode,
                                                      const unsigned char *mask,
                                                      const void *buffer, size_t length)
{
    fin = fin ? WS_FIN_FLAG : 0;

    unsigned char *outbuffer = xmalloc(length + WS_MAX_HEADER_SIZE);
    size_t frame_len = ws_format_frame(outbuffer, fin | opcode, mask, buffer, length);
    ssize_t ret = stream_do_write(stream_ws_to_stream(self), outbuffer, frame_len);

    STREAM_LOG("--- ws:write %ld", ret);
    STREAM_WS_LOG_DATA("ws:wr ", outbuffer, ret);

    xfree(outbuffer);
    return ret;
}





/**
 * Websocket stream class read operation
 *
 */
ssize_t stream_ws_do_read(struct stream_ws *self, void *buffer, size_t length)
{
    ssize_t ret = stream_ws_peek_frame(self);
    if (ret <= 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK)
            return ret;     // Error
    }

    if (TAILQ_EMPTY(&self->cache)) {
        errno = EAGAIN;
        return -1;
    }

    struct ws_frame_slice *slice = TAILQ_FIRST(&self->cache);
    ret = buffer_take(&slice->buffer, buffer, length);
    if (slice->buffer.length == 0) {
        TAILQ_REMOVE(&self->cache, slice, _entry_);
        ws_frame_slice_delete(slice);
    }

    STREAM_LOG("--- ws:read %ld", ret);
    STREAM_WS_LOG_DATA("ws:rd ", buffer, ret);

    return ret;
}


/**
 * Websocket stream class write operation
 *
 */
ssize_t stream_ws_do_write(struct stream_ws *self, const void *buffer, size_t length)
{
    stream_ws_generate_mask(self);
    return stream_ws_write_frame(self, WS_FIN_FLAG, self->data_type, self->mask, buffer, length);
}


/**
 * Websocket stream class time operation
 *
 */
int stream_ws_do_time(struct stream_ws *self)
{
    if (timer_expired(&self->keep_alive_timer)) {
        if (self->client_role) {
            if (self->keep_alive_responded) {
                stream_ws_generate_mask(self);
                stream_ws_write_frame(self, WS_FIN_FLAG, WS_OPCODE_PING, self->mask, NULL, 0);
                self->keep_alive_responded = false;
                timer_restart(&self->keep_alive_timer);
            }
            else {
                WARN("Stream WS %d fd, missing PONG", stream_ws_get_fd(self));
                stream_ws_set_status(self, STREAM_ST_CLOSING);
            }
        }
        else {
            WARN("Stream WS %d fd, missing PING", stream_ws_get_fd(self));
            stream_ws_set_status(self, STREAM_ST_CLOSING);
        }
    }

    return stream_do_time(&self->stream);
}





/**
 * Connect websocket
 *
 */
void stream_ws_connect(struct stream_ws *self, const char *uri, const char *key, const char *header)
{
    self->client_role = true;

    if (key) {
        self->key = xstrdup(key);
    }
    else {
        // Generate key, encode 16 random bytes
        unsigned char random[16];
        rand_data(random, sizeof(random));

        char ws_key[WS_KEY_BUFFER_SIZE];
        if (base64_encode(random, sizeof(random), (unsigned char*)ws_key, sizeof(ws_key), false) == 0)
            return;

        self->key = xstrdup(ws_key);
    }

    char request[WS_MESSAGE_BUFFER_SIZE];
    snprintf(request, sizeof(request), "GET %s HTTP/1.1\r\n"
                                       "%s"
                                       "Upgrade: websocket\r\n"
                                       "Connection: Upgrade\r\n"
                                       "Sec-WebSocket-Version: 13\r\n"
                                       "Sec-WebSocket-Key: %s\r\n\r\n",
                                       uri ? uri : "/",
                                       header ? header : "",
                                       self->key);

    STREAM_LOG("--- ws:connect");
    stream_do_write(stream_ws_to_stream(self), request, strlen(request));
}


/**
 * Disconnect websocket
 *
 */
void stream_ws_disconnect(struct stream_ws *self, unsigned short code)
{
    unsigned short payload[2];
    payload[0] = (unsigned char)((code >> 8) & 0xFF);
    payload[1] = (unsigned char)((code     ) & 0xFF);

    stream_ws_generate_mask(self);
    stream_ws_write_frame(self, WS_FIN_FLAG, WS_OPCODE_CLOSE, self->mask, payload, sizeof(payload));
    stream_set_status(&self->stream, STREAM_ST_CLOSING);
}








////// Virtual function definitions for stream class

/**
 * Websocket stream virtual destructor implementation
 *
 */
void* stream_ws_destructor_impl(struct stream *stream)
{
    stream_ws_clean((struct stream_ws*)stream);
    stream_clean(stream);

    return xfree(stream);
}


/**
 * Websocket stream virtual read implemenation
 *
 */
ssize_t stream_ws_read_impl(struct stream *stream, void *buffer, size_t length)
{
    return stream_ws_do_read((struct stream_ws *)stream, buffer, length);
}


/**
 * Websocket stream virtual write implementation
 *
 */
ssize_t stream_ws_write_impl(struct stream *stream, const void *buffer, size_t length)
{
    return stream_ws_do_write((struct stream_ws*)stream, buffer, length);
}


/**
 * Websocket stream virtual flush implementation
 *
 */
int stream_ws_flush_impl(struct stream *stream)
{
    return stream_do_flush(stream);
}


/**
 * Websocket stream virtual time handler implementation
 *
 */
int stream_ws_time_impl(struct stream *stream)
{
    return stream_ws_do_time((struct stream_ws*)stream);
}
