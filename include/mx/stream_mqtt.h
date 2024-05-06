
#ifndef __MX_STREAM_MQTT_H_
#define __MX_STREAM_MQTT_H_


#include "mx/stream.h"
#include "mx/mqtt.h"

#include <stdbool.h>



struct stream_mqtt;


typedef int (*stream_on_mqtt_msg_received_clbk)(void *object, struct stream_mqtt *stream, unsigned char type, unsigned char flags, void *msg);



struct stream_mqtt* stream_mqtt_new(struct stream *decorated);
struct stream_mqtt* stream_mqtt_delete(struct stream_mqtt *self);

struct stream* stream_mqtt_to_stream(struct stream_mqtt *self);
struct stream_mqtt* stream_mqtt_from_stream(struct stream *self);


static inline int stream_mqtt_get_fd(struct stream_mqtt *self) {
    return stream_get_fd(stream_mqtt_to_stream(self));
}

static inline void stream_mqtt_set_status(struct stream_mqtt *self, int status) {
    stream_set_status(stream_mqtt_to_stream(self), status);
}
static inline int stream_mqtt_get_status(struct stream_mqtt *self) {
    return stream_get_status(stream_mqtt_to_stream(self));
}

void stream_mqtt_set_observer(struct stream_mqtt *self, void *object, stream_on_mqtt_msg_received_clbk handler);
void stream_mqtt_remove_observer(struct stream_mqtt *self);



ssize_t stream_mqtt_peek_frame(struct stream_mqtt *self);
ssize_t stream_mqtt_read_frame(struct stream_mqtt *self, unsigned char *type, unsigned char *flags,
                                                      void *buffer, size_t length);
ssize_t stream_mqtt_write_frame(struct stream_mqtt *self, unsigned char type, unsigned char flags,
                                                      const void *buffer, size_t length);

ssize_t stream_mqtt_write_frame_packet_id(struct stream_mqtt *self, unsigned char type, unsigned short id);



ssize_t stream_mqtt_connect(struct stream_mqtt *self, bool clean_session, unsigned short keep_alive, const char *client_id,
                            const char *will_topic, unsigned char *will_msg, unsigned short will_msg_len, bool will_retain, unsigned char will_qos,
                            const char *user_name, unsigned char *password, unsigned short password_len);

ssize_t stream_mqtt_connack(struct stream_mqtt *self, bool session_present, unsigned char return_code);



ssize_t stream_mqtt_publish(struct stream_mqtt *self, bool retain, bool dup, unsigned char qos, unsigned short id,
                            const char *topic, const unsigned char *payload, size_t payload_len);

static inline ssize_t stream_mqtt_puback(struct stream_mqtt *self, unsigned short id) {
    return stream_mqtt_write_frame_packet_id(self, MQTT_PUBACK, id);
}

static inline ssize_t stream_mqtt_pubrec(struct stream_mqtt *self, unsigned short id) {
    return stream_mqtt_write_frame_packet_id(self, MQTT_PUBREC, id);
}

ssize_t stream_mqtt_pubrel(struct stream_mqtt *self, unsigned short id);

static inline ssize_t stream_mqtt_pubcomp(struct stream_mqtt *self, unsigned short id) {
    return stream_mqtt_write_frame_packet_id(self, MQTT_PUBCOMP, id);
}



ssize_t stream_mqtt_subscribe(struct stream_mqtt *self, unsigned short id, const char *topic, unsigned char qos);

ssize_t stream_mqtt_suback(struct stream_mqtt *self, unsigned short id, unsigned char return_code);

ssize_t stream_mqtt_unsubscribe(struct stream_mqtt *self, unsigned short id, const char *topic);

static inline ssize_t stream_mqtt_unsuback(struct stream_mqtt *self, unsigned short id) {
    return stream_mqtt_write_frame_packet_id(self, MQTT_UNSUBACK, id);
}



static inline ssize_t stream_mqtt_pingreq(struct stream_mqtt *self) {
    return stream_mqtt_write_frame(self, MQTT_PINGREQ, 0, NULL, 0);
}

static inline ssize_t stream_mqtt_pingresp(struct stream_mqtt *self) {
    return stream_mqtt_write_frame(self, MQTT_PINGRESP, 0, NULL, 0);
}



ssize_t stream_mqtt_disconnect(struct stream_mqtt *self);



#endif /* __MX_STREAM_MQTT_H_ */
