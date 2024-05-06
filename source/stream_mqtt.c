
#include "mx/stream_mqtt.h"
#include "mx/stream.h"

#include "mx/misc.h"
#include "mx/log.h"
#include "mx/memory.h"
#include "mx/string.h"
#include "mx/misc.h"
#include "mx/mqtt.h"
#include "mx/timer.h"

#include "private_stream.h"

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>


#ifdef DEBUG_STREAM_MQTT
  #define STREAM_MQTT_LOG_DATA(prefix, data, len)  _LOG_DATA_(LOG_COLOR_BLUE, prefix, data, len)
#else
  #define STREAM_MQTT_LOG_DATA(...)
#endif



#define MQTT_MESSAGE_BUFFER_SIZE        1024

#define MQTT_KEEP_ALIVE_TIMEOUT         15

#define MQTT_RESEND_ATTEMPTS            3
#define MQTT_RESEND_TIMEOUT             30



struct mqtt_frame_item
{
    struct buffer buffer;

    unsigned char type;
    unsigned char flags;
    unsigned short id;

    TAILQ_ENTRY(mqtt_frame_item) _entry_;
};

// struct mqtt_frame_queue
TAILQ_HEAD(mqtt_frame_queue, mqtt_frame_item);



struct mqtt_frame_item* mqtt_frame_item_new(unsigned char type, unsigned char flags, unsigned short id,
                                              const void *buffer, size_t length)
{
    struct mqtt_frame_item *self = xmalloc(sizeof(struct mqtt_frame_item));
    buffer_init(&self->buffer, length);
    buffer_append(&self->buffer, buffer, length);
    self->type = type;
    self->flags = flags;
    self->id = id;
    return self;
}


struct mqtt_frame_item* mqtt_frame_item_delete(struct mqtt_frame_item *self)
{
    buffer_clean(&self->buffer);
    return xfree(self);
}







static void* stream_mqtt_destructor_impl(struct stream *stream);
static ssize_t stream_mqtt_read_impl(struct stream *stream, void *buffer, size_t length);
static ssize_t stream_mqtt_write_impl(struct stream *stream, const void *buffer, size_t length);
static int     stream_mqtt_flush_impl(struct stream *stream);
static int     stream_mqtt_time_impl(struct stream *stream);


static const struct stream_vtable stream_mqtt_vtable = {
        .destructor_fn = stream_mqtt_destructor_impl,
        .read_fn = stream_mqtt_read_impl,
        .write_fn = stream_mqtt_write_impl,
        .flush_fn = stream_mqtt_flush_impl,
        .time_fn = stream_mqtt_time_impl,
};





struct stream_mqtt
{
    struct stream stream;
    struct buffer buffer;

    bool client_role;

    struct mqtt_frame_queue incoming;       // Incoming messages
    struct mqtt_frame_queue outgoing;       // Outgoing messages require acknowledgement
    struct mqtt_frame_queue inbound;        // Inbound publish qos=2 messages, waiting for delivery confirmation

    struct observer *mqtt_observer;

    unsigned int keep_alive;
    bool keep_alive_responded;
    struct timer keep_alive_timer;

    unsigned int resend_attempts;
    struct timer resend_timer;
};





/**
 * MQTT stream initializer
 *
 */
void stream_mqtt_init(struct stream_mqtt *self)
{
    buffer_init(&self->buffer, MQTT_MESSAGE_BUFFER_SIZE);
    TAILQ_INIT(&self->incoming);
    TAILQ_INIT(&self->outgoing);
    TAILQ_INIT(&self->inbound);

    self->mqtt_observer = NULL;

    self->keep_alive = 0;   // Disable keep alive
    self->keep_alive_responded = true;
    timer_stop(&self->keep_alive_timer);

    self->resend_attempts = 0;
    timer_stop(&self->resend_timer);

    self->client_role = false;
}


/**
 * MQTT stream cleaner
 *
 */
void stream_mqtt_clean(struct stream_mqtt *self)
{
    buffer_clean(&self->buffer);

    struct mqtt_frame_item *item, *tmp;
    TAILQ_FOREACH_SAFE(item, &self->incoming, _entry_, tmp) {
        TAILQ_REMOVE(&self->incoming, item, _entry_);
        mqtt_frame_item_delete(item);
    }
    TAILQ_FOREACH_SAFE(item, &self->outgoing, _entry_, tmp) {
        TAILQ_REMOVE(&self->outgoing, item, _entry_);
        mqtt_frame_item_delete(item);
    }
    TAILQ_FOREACH_SAFE(item, &self->inbound, _entry_, tmp) {
        TAILQ_REMOVE(&self->inbound, item, _entry_);
        mqtt_frame_item_delete(item);
    }

    stream_mqtt_remove_observer(self);
}


/**
 * MQTT stream constructor
 *
 */
struct stream_mqtt* stream_mqtt_new(struct stream *decorated)
{
    struct stream_mqtt *self = xmalloc(sizeof(struct stream_mqtt));
    self->stream.rtti = STREAM_MQTT_CLS;
    self->stream.vtable = &stream_mqtt_vtable;

    stream_init(&self->stream, -1, decorated);
    stream_mqtt_init(self);
    return self;
}


/**
 * MQTT stream destructor
 *
 */
struct stream_mqtt* stream_mqtt_delete(struct stream_mqtt *self)
{
    // Call virtual destructor
    return self->stream.vtable->destructor_fn(&self->stream);
}





/**
 * Cast MQTT stream to base class
 *
 */
struct stream* stream_mqtt_to_stream(struct stream_mqtt *self)
{
    return &self->stream;
}


/**
 * Cast MQTT stream from base class
 *
 */
struct stream_mqtt* stream_mqtt_from_stream(struct stream *self)
{
    if (self->rtti == STREAM_MQTT_CLS)
        return (struct stream_mqtt*)self;

    return NULL;
}


/**
 * Return decorated stream
 *
 */
struct stream* stream_mqtt_get_decorated(struct stream_mqtt *self)
{
    return stream_get_decorated(&self->stream);
}


/**
 * Set MQTT observer
 *
 */
void stream_mqtt_set_observer(struct stream_mqtt *self, void *object, stream_on_mqtt_msg_received_clbk handler)
{
    if (!self->mqtt_observer)
         self->mqtt_observer = observer_create(object, 0, (observer_notify_fn)handler);
}


/**
 * Remove MQTT observer
 *
 */
void stream_mqtt_remove_observer(struct stream_mqtt *self)
{
     if (self->mqtt_observer)
         self->mqtt_observer = observer_delete(self->mqtt_observer);
}


/**
 * Stream MQTT write
 *
 */
ssize_t stream_mqtt_real_write(struct stream_mqtt *self, const void *buffer, size_t length)
{
    ssize_t ret = stream_do_write(stream_mqtt_to_stream(self), buffer, length);
    STREAM_LOG("--- mqtt:write %ld", ret);
    STREAM_MQTT_LOG_DATA("mqtt:wr ", buffer, ret);
    return ret;
}



void stream_mqtt_append_frame(struct mqtt_frame_queue *queue, unsigned char type, unsigned char flags, unsigned short id,
                                                             const unsigned char *body_data, size_t body_length)
{
    struct mqtt_frame_item *item = mqtt_frame_item_new(type, flags, id, body_data, body_length);
    TAILQ_INSERT_TAIL(queue, item, _entry_);
}


void stream_mqtt_insert_frame(struct mqtt_frame_queue *queue, unsigned char type, unsigned char flags, unsigned short id,
                                                             const unsigned char *body_data, size_t body_length)
{
    struct mqtt_frame_item *item = mqtt_frame_item_new(type, flags, id, body_data, body_length);
    TAILQ_INSERT_HEAD(queue, item, _entry_);
}


bool stream_mqtt_remove_frame(struct mqtt_frame_queue *queue, unsigned char type, unsigned short id)
{
    bool found = false;

    struct mqtt_frame_item *item, *tmp;
    TAILQ_FOREACH_SAFE(item, queue, _entry_, tmp) {
        if ((item->type == type) && (item->id == id)) {
            TAILQ_REMOVE(queue, item, _entry_);
            mqtt_frame_item_delete(item);
            found = true;
        }
    }

    return found;
}



bool stream_mqtt_notify_observer(struct stream_mqtt *self, unsigned char type, unsigned char flags, void *msg)
{
    bool handled_by_observer = false;

    if (observer_is_available(self->mqtt_observer)) {
        stream_on_mqtt_msg_received_clbk handler = (stream_on_mqtt_msg_received_clbk)self->mqtt_observer->notify_handler;
        int ret = handler(self->mqtt_observer->object, self, type, flags, msg);
        if (ret > 0) {
            handled_by_observer = true;
        }
    }

    return handled_by_observer;
}


/**
 * Handle MQTT frame
 *
 */
void stream_mqtt_handle_received_frame(struct stream_mqtt *self, struct mqtt_fixed_header *frame, const unsigned char *body)
{
    switch (frame->type) {
        case MQTT_CONNECT: {
            struct mqtt_connect msg;
            mqtt_parse_connect(&msg, body, frame->body_length);
            if (msg.keep_alive > 0) {
                self->keep_alive = msg.keep_alive + MQTT_KEEP_ALIVE_TIMEOUT;
                timer_start(&self->keep_alive_timer, TIMER_SEC, self->keep_alive);
            }
            bool handled_by_observer = stream_mqtt_notify_observer(self, frame->type, frame->flags, &msg);
            if (!handled_by_observer)
                stream_mqtt_append_frame(&self->incoming, frame->type, frame->flags, 0, body, frame->body_length);
        }   break;

        case MQTT_CONNACK: {
            struct mqtt_connack msg;
            mqtt_parse_connack(&msg, body, frame->body_length);
            if ((msg.return_code == MQTT_CONNACK_ACCEPTED) && (self->keep_alive > 0)) {
                timer_start(&self->keep_alive_timer, TIMER_SEC, self->keep_alive);
                stream_mqtt_set_status(self, STREAM_ST_READY);
            }
            bool handled_by_observer = stream_mqtt_notify_observer(self, frame->type, frame->flags, &msg);
            if (!handled_by_observer)
                stream_mqtt_append_frame(&self->incoming, frame->type, frame->flags, 0, body, frame->body_length);
        }   break;

        case MQTT_SUBSCRIBE: {
            bool handled_by_observer = false;
            if (observer_is_available(self->mqtt_observer)) {
                struct mqtt_subscribe msg;
                mqtt_parse_subscribe(&msg, body, frame->body_length);
                handled_by_observer = stream_mqtt_notify_observer(self, frame->type, frame->flags, &msg);
            }
            if (!handled_by_observer)
                stream_mqtt_append_frame(&self->incoming, frame->type, frame->flags, 0, body, frame->body_length);
        }   break;

        case MQTT_UNSUBSCRIBE: {
            bool handled_by_observer = false;
            if (observer_is_available(self->mqtt_observer)) {
                struct mqtt_unsubscribe msg;
                mqtt_parse_unsubscribe(&msg, body, frame->body_length);
                handled_by_observer = stream_mqtt_notify_observer(self, frame->type, frame->flags, &msg);
            }
            if (!handled_by_observer)
                stream_mqtt_append_frame(&self->incoming, frame->type, frame->flags, 0, body, frame->body_length);
        }   break;

        case MQTT_PUBLISH: {
            unsigned char qos = MQTT_QOS_FROM_FLAGS(frame->flags);
            struct mqtt_publish msg;
            mqtt_parse_publish(&msg, body, frame->body_length, qos);

            if (qos == MQTT_QOS_2) {
                stream_mqtt_remove_frame(&self->inbound, frame->type, msg.id);
                stream_mqtt_append_frame(&self->inbound, frame->type, frame->flags, msg.id, body, frame->body_length);
                stream_mqtt_pubrec(self, msg.id);
            }
            else {
                if (qos == MQTT_QOS_1)
                    stream_mqtt_puback(self, msg.id);

                bool handled_by_observer = stream_mqtt_notify_observer(self, frame->type, frame->flags, &msg);
                if (!handled_by_observer)
                    stream_mqtt_append_frame(&self->incoming, frame->type, frame->flags, 0, body, frame->body_length);
            }
        }   break;

        case MQTT_SUBACK:
        case MQTT_UNSUBACK:
        case MQTT_PUBACK:
        case MQTT_PUBREC:
        case MQTT_PUBCOMP: {
            struct mqtt_var_header_id varhdr;
            mqtt_parse_var_header_id(&varhdr, body, frame->body_length);
            if (stream_mqtt_remove_frame(&self->outgoing, mqtt_request_type(frame->type), varhdr.id))
                timer_stop(&self->resend_timer);

            if (frame->type == MQTT_PUBREC) {
                stream_mqtt_pubrel(self, varhdr.id);
                timer_start(&self->resend_timer, TIMER_SEC, MQTT_RESEND_TIMEOUT);
                self->resend_attempts = 0;
            }
            else if (frame->type == MQTT_SUBACK) {
                bool handled_by_observer = false;
                if (observer_is_available(self->mqtt_observer)) {
                    struct mqtt_suback msg;
                    mqtt_parse_suback(&msg, body, frame->body_length);
                    handled_by_observer = stream_mqtt_notify_observer(self, frame->type, frame->flags, &msg);
                }
                if (!handled_by_observer)
                    stream_mqtt_append_frame(&self->incoming, frame->type, frame->flags, 0, body, frame->body_length);
            }
            else if (frame->type == MQTT_UNSUBACK) {
                bool handled_by_observer = false;
                if (observer_is_available(self->mqtt_observer)) {
                    struct mqtt_unsuback msg;
                    mqtt_parse_unsuback(&msg, body, frame->body_length);
                    handled_by_observer = stream_mqtt_notify_observer(self, frame->type, frame->flags, &msg);
                }
                if (!handled_by_observer)
                    stream_mqtt_append_frame(&self->incoming, frame->type, frame->flags, 0, body, frame->body_length);
            }
        }   break;

        case MQTT_PUBREL: {
            struct mqtt_var_header_id msg;
            mqtt_parse_var_header_id(&msg, body, frame->body_length);
            stream_mqtt_pubcomp(self, msg.id); // Always replay to PUBREL

            struct mqtt_frame_item *item, *tmp;
            TAILQ_FOREACH_SAFE(item, &self->inbound, _entry_, tmp) {
                if (item->id == msg.id) {
                    // Delivery qos=2 message complete
                    TAILQ_REMOVE(&self->inbound, item, _entry_);

                    bool handled_by_observer = false;
                    if (item->type == MQTT_PUBLISH && observer_is_available(self->mqtt_observer)) {
                        struct mqtt_publish msg;
                        mqtt_parse_publish(&msg, item->buffer.data, item->buffer.length, MQTT_QOS_FROM_FLAGS(item->flags));
                        handled_by_observer = stream_mqtt_notify_observer(self, item->type, item->flags, &msg);
                    }
                    if (handled_by_observer) {
                        mqtt_frame_item_delete(item);
                    } else {
                        //Save received packet so that it may be read by application
                        TAILQ_INSERT_TAIL(&self->incoming, item, _entry_);
                    }
                }
            }
        }   break;

        case MQTT_PINGREQ:
            stream_mqtt_pingresp(self);
            // Stream is still valid
            timer_start(&self->keep_alive_timer, TIMER_SEC, self->keep_alive);
            break;

        case MQTT_PINGRESP:
            self->keep_alive_responded = true;
            break;

        case MQTT_DISCONNECT: {
            bool handled_by_observer = false;
            if (observer_is_available(self->mqtt_observer)) {
                handled_by_observer = stream_mqtt_notify_observer(self, frame->type, frame->flags, NULL);
            }
            if (!handled_by_observer) {
                // Store dummy byte to forward frame to application and avoid blocking incoming queue because of empty body
                unsigned char dummy = 0;
                stream_mqtt_append_frame(&self->incoming, frame->type, frame->flags, 0, &dummy, sizeof(dummy));
            }
        }   break;
    }
}


/**
 * Peek for MQTT frame
 *
 */
ssize_t stream_mqtt_peek_frame(struct stream_mqtt *self)
{
    char tmpbuf[MQTT_MESSAGE_BUFFER_SIZE];

    STREAM_LOG("--- mqtt:peek");
    ssize_t ret;
    do {
        if (!TAILQ_EMPTY(&self->incoming))
            break;  // Something is already waiting

        ret = stream_read(stream_mqtt_get_decorated(self), tmpbuf, sizeof(tmpbuf));
        if (ret <= 0) {
            if ((ret == 0) || (errno != EAGAIN && errno != EWOULDBLOCK))
                return ret;     // Error

            // All received data was read
            break;
        }
        else {
            // Received some data
            buffer_append(&self->buffer, tmpbuf, ret);
            STREAM_MQTT_LOG_DATA("mqtt:rd ", tmpbuf, ret);

            while (!buffer_is_empty(&self->buffer)) {
                struct mqtt_fixed_header frame;
                size_t frame_length;
                int body_offset = mqtt_parse_frame(&frame, self->buffer.data, self->buffer.length, &frame_length);
                if (body_offset == 0)
                    break;  // More data is needed

                if (body_offset < 0) {
                    // Error
                    buffer_reset(&self->buffer);
                }
                else if (body_offset > 0) {
                    // Received valid MQTT frame
                    stream_mqtt_handle_received_frame(self, &frame, self->buffer.data+body_offset);
                    buffer_cut(&self->buffer, frame_length);
                }
            }
        }
    } while (ret > 0);

    if (!TAILQ_EMPTY(&self->incoming)) {
        struct mqtt_frame_item *item = TAILQ_FIRST(&self->incoming);
        return item->buffer.length;
    }

    errno = EAGAIN;
    return -1;
}


/**
 * Read MQTT frame
 *
 */
ssize_t stream_mqtt_read_frame(struct stream_mqtt *self, unsigned char *type, unsigned char *flags,
                                                      void *buffer, size_t length)
{
     if (TAILQ_EMPTY(&self->incoming)) {
         errno = EAGAIN;
         return -1;     // No data received
     }
     struct mqtt_frame_item *item = TAILQ_FIRST(&self->incoming);
     if (item->buffer.length > length) {
         errno = ENOMEM;
         return -1;     // Bigger buffer needed
     }

     ssize_t ret = buffer_take(&item->buffer, buffer, length);
     if (type)
         *type = item->type;
     if (flags)
         *flags = item->flags;

     TAILQ_REMOVE(&self->incoming, item, _entry_);
     mqtt_frame_item_delete(item);
     return ret;
}


/**
 * Write MQTT frame
 *
 */
ssize_t stream_mqtt_write_frame(struct stream_mqtt *self, unsigned char type, unsigned char flags,
                                                      const void *buffer, size_t length)
{
    unsigned char *frame = xmalloc(length + MQTT_MAX_FIXED_HEADER_SIZE);
    size_t frame_len = mqtt_format_frame(frame, type, flags, buffer, length);
    ssize_t ret = stream_mqtt_real_write(self, frame, frame_len);
    xfree(frame);

    return ret;
}


/**
 * Write MQTT frame with packet id
 *
 */
ssize_t stream_mqtt_write_frame_packet_id(struct stream_mqtt *self, unsigned char type, unsigned short id)
{
    unsigned char body[MQTT_PACKET_ID_SIZE];
    mqtt_put_short(body, id);

    unsigned char flags = 0;
    if (type == MQTT_PUBREL)
        flags = MQTT_QOS_TO_FLAGS(MQTT_QOS_1);

    return stream_mqtt_write_frame(self, type, flags, body, sizeof(body));
}





/**
 * Send MQTT Connect message
 *
 */
ssize_t stream_mqtt_connect(struct stream_mqtt *self, bool clean_session, unsigned short keep_alive, const char *client_id,
                            const char *will_topic, unsigned char *will_msg, unsigned short will_msg_len, bool will_retain, unsigned char will_qos,
                            const char *user_name, unsigned char *password, unsigned short password_len)
{
    self->client_role = true;
    self->keep_alive = keep_alive;

    size_t body_len = mqtt_eval_connect(client_id, will_topic, will_msg_len, user_name, password_len);
    unsigned char *buffer = xmalloc(body_len + MQTT_MAX_FIXED_HEADER_SIZE);
    size_t offset = 0;

    offset += mqtt_format_fixed_header(buffer, MQTT_CONNECT, 0, body_len);
    offset += mqtt_format_connect(buffer+offset, clean_session, keep_alive, client_id,
                                                 will_topic, will_msg, will_msg_len, will_retain, will_qos,
                                                 user_name, password, password_len);
    STREAM_LOG("--- mqtt:connect");
    ssize_t ret = stream_mqtt_real_write(self, buffer, offset);
    xfree(buffer);

    return ret;
}


/**
 * Send MQTT Connack message
 *
 */
ssize_t stream_mqtt_connack(struct stream_mqtt *self, bool session_present, unsigned char return_code)
{
    unsigned char body[MQTT_CONNACK_VARIABLE_HEADER_SIZE];

    if (return_code == MQTT_CONNACK_ACCEPTED)
        stream_mqtt_set_status(self, STREAM_ST_READY);

    size_t body_len = mqtt_format_connack(body, session_present, return_code);
    return stream_mqtt_write_frame(self, MQTT_CONNACK, 0, body, body_len);
}


/**
 * Send MQTT Publish message
 *
 */
ssize_t stream_mqtt_publish(struct stream_mqtt *self, bool retain, bool dup, unsigned char qos, unsigned short id,
                         const char *topic, const unsigned char *payload, size_t payload_len)
{
    unsigned char flags = MQTT_QOS_TO_FLAGS(qos);
    if (retain)
        flags |= MQTT_RETAIN_FLAG;
    if (dup)
        flags |= MQTT_DUP_FLAG;


    size_t body_len = mqtt_eval_publish(qos, topic, payload_len);
    unsigned char *buffer = xmalloc(body_len + MQTT_MAX_FIXED_HEADER_SIZE);
    size_t offset = 0;

    offset += mqtt_format_fixed_header(buffer, MQTT_PUBLISH, flags, body_len);
    offset += mqtt_format_publish(buffer+offset, qos, id, topic, payload, payload_len);

    ssize_t ret = 1;
    bool msg_sent = TAILQ_EMPTY(&self->outgoing);
    if (msg_sent) {
        ret = stream_mqtt_real_write(self, buffer, offset);
        if (qos > MQTT_QOS_0) {
            timer_start(&self->resend_timer, TIMER_SEC, MQTT_RESEND_TIMEOUT);
            self->resend_attempts = 0;
        }
    }
    if (!msg_sent || (qos > MQTT_QOS_0)) {
        stream_mqtt_append_frame(&self->outgoing, MQTT_PUBLISH, flags, id, buffer, offset);
    }

    xfree(buffer);
    return ret;
}


/**
 * Send MQTT Pubrel message
 *
 * Message is sent automatically in response to Publish QoS 2 so that it is queued from the head
 *
 */
ssize_t stream_mqtt_pubrel(struct stream_mqtt *self, unsigned short id)
{
    size_t offset = 0;
    unsigned char buffer[MQTT_MIN_FIXED_HEADER_SIZE + MQTT_PACKET_ID_SIZE];

    offset += mqtt_format_fixed_header(buffer+offset, MQTT_PUBREL, MQTT_QOS_TO_FLAGS(MQTT_QOS_1), MQTT_PACKET_ID_SIZE);
    offset += mqtt_put_short(buffer+offset, id);

    stream_mqtt_insert_frame(&self->outgoing, MQTT_PUBREL, MQTT_QOS_TO_FLAGS(MQTT_QOS_1), id, buffer, offset);
    return stream_mqtt_real_write(self, buffer, offset);
}



/**
 * Send MQTT Subscribe message
 */
ssize_t stream_mqtt_subscribe(struct stream_mqtt *self, unsigned short id, const char *topic, unsigned char qos)
{
    unsigned char flags = MQTT_QOS_TO_FLAGS(MQTT_QOS_1);

    size_t body_len = mqtt_eval_subscribe(topic);
    unsigned char *buffer = xmalloc(body_len + MQTT_MAX_FIXED_HEADER_SIZE);
    size_t offset = 0;

    offset += mqtt_format_fixed_header(buffer, MQTT_SUBSCRIBE, flags, body_len);
    offset += mqtt_format_subscribe(buffer+offset, id, topic, qos);

    ssize_t ret = 1;
    if (TAILQ_EMPTY(&self->outgoing)) {
        ret = stream_mqtt_real_write(self, buffer, offset);
        timer_start(&self->resend_timer, TIMER_SEC, MQTT_RESEND_TIMEOUT);
        self->resend_attempts = 0;
    }
    stream_mqtt_append_frame(&self->outgoing, MQTT_SUBSCRIBE, flags, id, buffer, offset);

    xfree(buffer);
    return ret;
}


/**
 * Send MQTT Suback message
 *
 */
ssize_t stream_mqtt_suback(struct stream_mqtt *self, unsigned short id, unsigned char return_code)
{
    size_t body_size = MQTT_SUBACK_VARIABLE_HEADER_SIZE + MQTT_SUBACK_PAYLOAD_SIZE;
    unsigned char body[body_size];

    size_t body_len = mqtt_format_suback(body, id, return_code);
    return stream_mqtt_write_frame(self, MQTT_SUBACK, 0, body, body_len);
}


/**
 * Send MQTT Unsubscribe message
 *
 */
ssize_t stream_mqtt_unsubscribe(struct stream_mqtt *self, unsigned short id, const char *topic)
{
    unsigned char flags = MQTT_QOS_TO_FLAGS(MQTT_QOS_1);

    size_t body_len = mqtt_eval_unsubscribe(topic);
    unsigned char *buffer = xmalloc(body_len + MQTT_MAX_FIXED_HEADER_SIZE);
    size_t offset = 0;

    offset += mqtt_format_fixed_header(buffer, MQTT_UNSUBSCRIBE, flags, body_len);
    offset += mqtt_format_unsubscribe(buffer+offset, id, topic);

    ssize_t ret = 1;
    if (TAILQ_EMPTY(&self->outgoing)) {
        ret = stream_mqtt_real_write(self, buffer, offset);
        timer_start(&self->resend_timer, TIMER_SEC, MQTT_RESEND_TIMEOUT);
        self->resend_attempts = 0;
    }
    stream_mqtt_append_frame(&self->outgoing, MQTT_UNSUBSCRIBE, flags, id, buffer, offset);

    xfree(buffer);
    return ret;
}


/**
 * Send MQTT Disconnect message
 *
 */
ssize_t stream_mqtt_disconnect(struct stream_mqtt *self)
{
    ssize_t ret = stream_mqtt_write_frame(self, MQTT_DISCONNECT, 0, NULL, 0);
    stream_set_status(&self->stream, STREAM_ST_CLOSING);

    return ret;
}


/**
 * MQTT stream class time operation
 *
 */
int stream_mqtt_do_time(struct stream_mqtt *self)
{
    if (timer_expired(&self->keep_alive_timer)) {
        if (self->client_role) {
            if (self->keep_alive_responded) {
                stream_mqtt_pingreq(self);
                self->keep_alive_responded = false;
                timer_restart(&self->keep_alive_timer);
            }
            else {
                WARN("Stream MQTT %d fd, missing PING response", stream_mqtt_get_fd(self));
                stream_mqtt_set_status(self, STREAM_ST_CLOSING);
            }
        }
        else {
            WARN("Stream MQTT %d fd, missing PING request", stream_mqtt_get_fd(self));
            stream_mqtt_set_status(self, STREAM_ST_CLOSING);
        }
    }

    if (timer_expired(&self->resend_timer)) {
        struct mqtt_frame_item *item = TAILQ_FIRST(&self->outgoing);
        if (item) {
            if (++self->resend_attempts < MQTT_RESEND_ATTEMPTS) {
                WARN("Stream MQTT %d fd, resend %s message", stream_mqtt_get_fd(self), mqtt_packet_name(item->type));
                stream_mqtt_real_write(self, item->buffer.data, item->buffer.length);
                timer_start(&self->resend_timer, TIMER_SEC, MQTT_RESEND_TIMEOUT);
            }
            else {
                WARN("Stream MQTT %d fd, abandon %s message", stream_mqtt_get_fd(self), mqtt_packet_name(item->type));
                TAILQ_REMOVE(&self->outgoing, item, _entry_);
                mqtt_frame_item_delete(item);
                timer_stop(&self->resend_timer);
                self->resend_attempts = 0;
            }
        }
    }
    if (!timer_running(&self->resend_timer)) {
        struct mqtt_frame_item *item = TAILQ_FIRST(&self->outgoing);
        if (item) {
            stream_mqtt_real_write(self, item->buffer.data, item->buffer.length);
            timer_start(&self->resend_timer, TIMER_SEC, MQTT_RESEND_TIMEOUT);
        }
    }

    return stream_do_time(&self->stream);
}





////// Virtual function definitions for stream class

/**
 * MQTT stream virtual destructor implementation
 *
 */
void* stream_mqtt_destructor_impl(struct stream *stream)
{
    stream_mqtt_clean((struct stream_mqtt*)stream);
    stream_clean(stream);

    return xfree(stream);
}


/**
 * MQTT stream virtual read implemenation
 *
 */
ssize_t stream_mqtt_read_impl(struct stream *stream, void *buffer, size_t length)
{
    UNUSED(buffer);
    UNUSED(length);

    ERROR("MQTT supports frame mode only");
    stream_set_status(stream, STREAM_ST_CLOSING);
    return 0;
}


/**
 * MQTT stream virtual write implementation
 *
 */
ssize_t stream_mqtt_write_impl(struct stream *stream, const void *buffer, size_t length)
{
    UNUSED(buffer);
    UNUSED(length);

    ERROR("MQTT supports frame mode only");
    stream_set_status(stream, STREAM_ST_CLOSING);
    return 0;
}


/**
 * MQTT stream virtual flush implementation
 *
 */
int stream_mqtt_flush_impl(struct stream *stream)
{
    return stream_do_flush(stream);
}


/**
 * MQTT stream virtual time handler implementation
 *
 */
int stream_mqtt_time_impl(struct stream *stream)
{
    return stream_mqtt_do_time((struct stream_mqtt*)stream);
}
