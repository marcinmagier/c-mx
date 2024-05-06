
#ifndef __MX_MQTT_H_
#define __MX_MQTT_H_


#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>



#define MQTT_TYPE_FLAGS_IDX         0
#define MQTT_LENGTH_IDX             1

#define MQTT_TYPE_FLAGS_SIZE        1
#define MQTT_MIN_LENGTH_SIZE        1
#define MQTT_MAX_LENGTH_SIZE        4
#define MQTT_MIN_FIXED_HEADER_SIZE  (MQTT_TYPE_FLAGS_SIZE + MQTT_MIN_LENGTH_SIZE)
#define MQTT_MAX_FIXED_HEADER_SIZE  (MQTT_TYPE_FLAGS_SIZE + MQTT_MAX_LENGTH_SIZE)

#define MQTT_STR_LENGTH_SIZE        2
#define MQTT_PACKET_ID_SIZE         2



enum mqtt_packet_type_e
{
    MQTT_FORBIDDEN = 0,
    MQTT_CONNECT,
    MQTT_CONNACK,
    MQTT_PUBLISH,
    MQTT_PUBACK,
    MQTT_PUBREC,
    MQTT_PUBREL,
    MQTT_PUBCOMP,
    MQTT_SUBSCRIBE,
    MQTT_SUBACK,
    MQTT_UNSUBSCRIBE,
    MQTT_UNSUBACK,
    MQTT_PINGREQ,
    MQTT_PINGRESP,
    MQTT_DISCONNECT
};


#define MQTT_RETAIN_FLAG            (0x1 << 0)
#define MQTT_QOS_POS                1
#define MQTT_QOS_MSK                0x3
#define MQTT_QOS_TO_FLAGS(qos)      ((qos & MQTT_QOS_MSK) << MQTT_QOS_POS)
#define MQTT_QOS_FROM_FLAGS(qos)    ((qos >> MQTT_QOS_POS) & MQTT_QOS_MSK)
#define MQTT_DUP_FLAG               (0x1 << 3)

enum mqtt_qos_e
{
    MQTT_QOS_0 = 0,
    MQTT_QOS_1 = 1,
    MQTT_QOS_2 = 2
};


#define MQTT_PROTOCOL_NAME          "MQTT"
#define MQTT_PROTOCOL_NAME_SIZE     4
#define MQTT_PROTOCOL_LEVEL         0x4



struct mqtt_fixed_header
{
    unsigned char type;
    unsigned char flags;

    size_t body_length;
};


int mqtt_parse_frame(struct mqtt_fixed_header *header, const unsigned char *data, size_t length, size_t *frame_length);
size_t mqtt_format_frame(unsigned char *buffer, unsigned char type, unsigned char flags,
                                                const unsigned char *body_data, size_t body_length);

size_t mqtt_format_fixed_header(unsigned char *buffer, unsigned char type, unsigned char flags, size_t body_length);



#define MQTT_ID_VARIABLE_HEADER_SIZE                2       // Packet Identifier

struct mqtt_var_header_id
{
    unsigned short id;
};

ssize_t mqtt_parse_var_header_id(struct mqtt_var_header_id *payload, const unsigned char *buffer, size_t length);
size_t mqtt_format_var_header_id(unsigned char *buffer, unsigned short id);





#define MQTT_CONNECT_VARIABLE_HEADER_SIZE           10      // 6 + 1 + 1 + 2

#define MQTT_CONNECT_RESERVED       (0x1 << 0)
#define MQTT_CONNECT_CLEAN_SESSION  (0x1 << 1)
#define MQTT_CONNECT_WILL_FLAG      (0x1 << 2)
#define MQTT_CONNECT_WILL_QOS_POS   3
#define MQTT_CONNECT_WILL_QOS_MSK   (0x3 << 3)
#define MQTT_CONNECT_WILL_QOS_0     (0x0 << 3)
#define MQTT_CONNECT_WILL_QOS_1     (0x1 << 3)
#define MQTT_CONNECT_WILL_QOS_2     (0x2 << 3)
#define MQTT_CONNECT_WILL_RETAIN    (0x1 << 5)
#define MQTT_CONNECT_PASSWORD       (0x1 << 6)
#define MQTT_CONNECT_USER_NAME      (0x1 << 7)


struct mqtt_connect
{
    const char *protocol_name;
    unsigned short protocol_name_len;
    unsigned char protocol_level;

    bool clean_session;
    unsigned short keep_alive;

    unsigned char will_qos;
    bool will_retain;
    const char *will_topic;
    unsigned short will_topic_len;
    const unsigned char *will_msg;
    unsigned short will_msg_len;

    const char *client_id;
    unsigned short client_id_len;

    const char *user_name;
    unsigned short user_name_len;

    const unsigned char *password;
    unsigned short password_len;
};


ssize_t mqtt_parse_connect(struct mqtt_connect *payload, const unsigned char *buffer, size_t length);
size_t mqtt_eval_connect(const char *client_id, const char *will_topic, unsigned short will_msg_len,
                         const char *user_name, unsigned short password_len);
size_t mqtt_format_connect(unsigned char *buffer, bool clean_session, unsigned short keep_alive, const char *client_id,
                         const char *will_topic, unsigned char *will_msg, unsigned short will_msg_len, bool will_retain, unsigned char will_qos,
                         const char *user_name, unsigned char *password, unsigned short password_len);





#define MQTT_CONNACK_VARIABLE_HEADER_SIZE           2       // 1 + 1

#define MQTT_CONNACK_FLAGS_POS              1
#define MQTT_CONNACK_SESSION_PRESENT_FLAG   0x1
#define MQTT_CONNACK_RETURN_CODE_POS        2

enum mqtt_connack_return_code_e {
    MQTT_CONNACK_ACCEPTED = 0,
    MQTT_CONNACK_REFUSED_PROTOCOL_VERSION = 1,
    MQTT_CONNACK_REFUSED_IDENTIFIER_REJECTED = 2,
    MQTT_CONNACK_REFUSED_SERVER_UNAVAILABLE = 3,
    MQTT_CONNACK_REFUSED_BAD_USER_NAME_OR_PASSWORD = 4,
    MQTT_CONNACK_REFUSED_NOT_AUTHORIZED = 5
};


struct mqtt_connack
{
    bool session_present;
    unsigned char return_code;
};


ssize_t mqtt_parse_connack(struct mqtt_connack *payload, const unsigned char *buffer, size_t length);
size_t mqtt_format_connack(unsigned char *buffer, bool session_present, unsigned char return_code);





#define MQTT_PUBLISH_VARIABLE_HEADER_SIZE           4       // 2 + 2

struct mqtt_publish
{
    unsigned short id;
    const char *topic;
    unsigned short topic_len;
    const unsigned char *payload;
    unsigned short payload_len;
};


ssize_t mqtt_parse_publish(struct mqtt_publish *payload, const unsigned char *buffer, size_t length, unsigned char qos);
size_t mqtt_eval_publish(unsigned char qos, const char *topic, size_t payload_len);
size_t mqtt_format_publish(unsigned char *buffer, unsigned char qos, unsigned short id, const char *topic,
                           const unsigned char *payload, size_t payload_len);





#define MQTT_SUBSCRIBE_VARIABLE_HEADER_SIZE         2       // Packet Identifier

struct mqtt_subscribe
{
    unsigned short id;
    const char *topic;
    unsigned short topic_len;
    unsigned char qos;
};


ssize_t mqtt_parse_subscribe(struct mqtt_subscribe *payload, const unsigned char *buffer, size_t length);
size_t mqtt_eval_subscribe(const char *topic);
size_t mqtt_format_subscribe(unsigned char *buffer, unsigned short id, const char *topic, unsigned char qos);





#define MQTT_SUBACK_VARIABLE_HEADER_SIZE            2       // Packet Identifier
#define MQTT_SUBACK_PAYLOAD_SIZE                    1       // Return Code

enum mqtt_suback_return_code_e {
    MQTT_SUBACK_SUCCESS_MAX_QOS_0 = 0,
    MQTT_SUBACK_SUCCESS_MAX_QOS_1 = 1,
    MQTT_SUBACK_SUCCESS_MAX_QOS_2 = 2,
    MQTT_SUBACK_FAILURE           = 128
};

struct mqtt_suback
{
    unsigned short id;
    unsigned char return_code;
};

ssize_t mqtt_parse_suback(struct mqtt_suback *payload, const unsigned char *buffer, size_t length);
size_t mqtt_format_suback(unsigned char *buffer, unsigned short id, unsigned char return_code);





#define MQTT_UNSUBSCRIBE_VARIABLE_HEADER_SIZE       2       // Packet Identifier

struct mqtt_unsubscribe
{
    unsigned short id;
    const char *topic;
    unsigned short topic_len;
};


ssize_t mqtt_parse_unsubscribe(struct mqtt_unsubscribe *payload, const unsigned char *buffer, size_t length);
size_t mqtt_eval_unsubscribe(const char *topic);
size_t mqtt_format_unsubscribe(unsigned char *buffer, unsigned short id, const char *topic);





#define MQTT_UNSUBACK_VARIABLE_HEADER_SIZE          2       // Packet Identifier

struct mqtt_unsuback
{
    unsigned short id;
};

ssize_t mqtt_parse_unsuback(struct mqtt_unsuback *payload, const unsigned char *buffer, size_t length);
size_t mqtt_format_unsuback(unsigned char *buffer, unsigned short id);







int mqtt_put_byte(unsigned char *buffer, unsigned char value);
int mqtt_put_short(unsigned char *buffer, unsigned short value);
int mqtt_put_data(unsigned char *buffer, const unsigned char *data, unsigned short data_len);
int mqtt_put_string(unsigned char *buffer, const char *str);

int mqtt_get_byte(const unsigned char *buffer, unsigned char *value);
int mqtt_get_short(const unsigned char *buffer, unsigned short *value);
int mqtt_get_data(const unsigned char *buffer, const unsigned char **data, unsigned short data_len);
int mqtt_get_string(const unsigned char *buffer, const char **str, unsigned short *str_len);







struct mqtt_msg
{
    char *topic;
    unsigned char *payload;
    unsigned short payload_len;
    unsigned char qos;
    bool retain;
};

void mqtt_msg_init(struct mqtt_msg *self);
void mqtt_msg_clean(struct mqtt_msg *self);

struct mqtt_msg* mqtt_msg_new(void);
struct mqtt_msg* mqtt_msg_create(const char *topic, const unsigned char *msg, unsigned short msg_len, unsigned short qos, bool retain);
struct mqtt_msg* mqtt_msg_delete(struct mqtt_msg *self);

void mqtt_msg_copy(struct mqtt_msg *self, struct mqtt_msg *other);
void mqtt_msg_move(struct mqtt_msg *self, struct mqtt_msg *other);



struct mqtt_conf
{
    char *client_id;

    char *user_name;
    unsigned char *password;
    unsigned short password_len;

    unsigned short keep_alive;
};

void mqtt_conf_init(struct mqtt_conf *self);
void mqtt_conf_clean(struct mqtt_conf *self);

struct mqtt_conf* mqtt_conf_new(void);
struct mqtt_conf* mqtt_conf_create(const char *client_id, unsigned short keep_alive,
                                   const char *user_name, const unsigned char *password, unsigned short password_len);
struct mqtt_conf* mqtt_conf_delete(struct mqtt_conf *self);

void mqtt_conf_copy(struct mqtt_conf *self, struct mqtt_conf *other);
void mqtt_conf_move(struct mqtt_conf *self, struct mqtt_conf *other);


const char* mqtt_packet_name(unsigned char type);
unsigned char mqtt_response_type(unsigned char req_type, unsigned char qos);
unsigned char mqtt_request_type(unsigned char resp_type);


#endif /* __MX_MQTT_H_ */
