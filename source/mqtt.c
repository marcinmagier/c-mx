
#include "mx/mqtt.h"

#include "mx/memory.h"
#include "mx/string.h"
#include "mx/misc.h"

#include <string.h>


#define MQTT_DEFAULT_KEEP_ALIVE     (3*60)



static const char* mqtt_packet_names[] = {
        ENUM_STR(MQTT_FORBIDDEN),
        ENUM_STR(MQTT_CONNECT),
        ENUM_STR(MQTT_CONNACK),
        ENUM_STR(MQTT_PUBLISH),
        ENUM_STR(MQTT_PUBACK),
        ENUM_STR(MQTT_PUBREC),
        ENUM_STR(MQTT_PUBREL),
        ENUM_STR(MQTT_PUBCOMP),
        ENUM_STR(MQTT_SUBSCRIBE),
        ENUM_STR(MQTT_SUBACK),
        ENUM_STR(MQTT_UNSUBSCRIBE),
        ENUM_STR(MQTT_UNSUBACK),
        ENUM_STR(MQTT_PINGREQ),
        ENUM_STR(MQTT_PINGRESP),
        ENUM_STR(MQTT_DISCONNECT),
};





/**
 * Decode MQTT packet length
 *
 */
int mqtt_decode_length(const unsigned char *buffer, size_t length, size_t *value)
{
    unsigned char c;
    int multiplier = 1;
    int idx = 0;
    size_t val = 0;

    do {
        if (length <= (size_t)idx)
            return 0;   // Need more data
        if (idx >= MQTT_MAX_LENGTH_SIZE)
            return -1;  // Error

        c = buffer[idx++];

        val += (c & 127) * multiplier;
        multiplier *= 128;
    } while ((c & 128) != 0);

    if (value)
        *value = val;
    return idx;
}


/**
 * Encode MQTT packet length
 *
 * Buffer size should be at least MQTT_MIN_LENGTH_SIZE
 *
 */
int mqtt_encode_length(unsigned char *buffer, size_t value)
{
    int idx = 0;

    do {
        unsigned char d = value % 128;
        value /= 128;

        if (value > 0)
            d |= 0x80;  // Mark more digits to encode

        if (buffer)
            buffer[idx] = d;

        idx++;
    } while (value > 0);

    return idx;
}


/**
 * Perform meta-analysis of the data and find mqtt frame
 *
 */
int mqtt_parse_frame(struct mqtt_fixed_header *frame, const unsigned char *data, size_t length, size_t *frame_length)
{
    size_t expected_frame_lenght = MQTT_MIN_FIXED_HEADER_SIZE;

    if (length < expected_frame_lenght) {
        if (frame_length)
            *frame_length = expected_frame_lenght;
        return 0;   // Wait for basic header
    }

    frame->type = (data[MQTT_TYPE_FLAGS_IDX] >> 4) & 0xF;
    frame->flags = (data[MQTT_TYPE_FLAGS_IDX]    ) & 0xF;

    int length_bytes = mqtt_decode_length(data+MQTT_LENGTH_IDX, length-MQTT_LENGTH_IDX, &frame->body_length);
    if (length_bytes < 0) {
        return length_bytes;    // Error
    }
    else if (length_bytes == 0) {
        // Need more data
        if (frame_length)
            *frame_length = length + 1;
        return 0;
    }

    int body_offset = MQTT_LENGTH_IDX + length_bytes;
    expected_frame_lenght = body_offset + frame->body_length;
    if (frame_length)
        *frame_length = expected_frame_lenght;
    if (length < expected_frame_lenght)
        return 0;

    return body_offset;
}


/**
 * Format MQTT frame
 *
 * Buffer should be MQTT_MAX_HEADER_SIZE longer than data length
 *
 */
size_t mqtt_format_frame(unsigned char *buffer, unsigned char type, unsigned char flags,
                                                const unsigned char *body_data, size_t body_length)
{
    int body_offset = mqtt_format_fixed_header(buffer, type, flags, body_length);

    if (body_length)
        memcpy(buffer+body_offset, body_data, body_length);

    return body_offset + body_length;
}


/**
 * Format MQTT fixed header
 *
 * Buffer should be at least MQTT_MAX_HEADER_SIZE long
 *
 */
size_t mqtt_format_fixed_header(unsigned char *buffer, unsigned char type, unsigned char flags, size_t body_length)
{
    buffer[MQTT_TYPE_FLAGS_IDX] = (type << 4) | flags;
    size_t body_offset = MQTT_LENGTH_IDX + mqtt_encode_length(buffer+MQTT_LENGTH_IDX, body_length);

    return body_offset;
}







/**
 * Parse MQTT variable header that contains packet id
 *
 */
ssize_t mqtt_parse_var_header_id(struct mqtt_var_header_id *payload, const unsigned char *buffer, size_t length)
{
    unsigned int offset = 0;

    // --- Variable header ---
    // Packet Identifier
    offset += mqtt_get_short(buffer+offset, &payload->id);

    if (offset < length)
        return -1;

    return offset;
}


/**
 * Format MQTT variable header that contains packet id
 *
 * Buffer should be at least MQTT_ID_VARIABLE_HEADER_SIZE long
 *
 */
size_t mqtt_format_var_header_id(unsigned char *buffer, unsigned short id)
{
    unsigned int offset = 0;

    // --- Variable header ---
    // Packet Identifier
    offset += mqtt_put_short(buffer+offset, id);

    return offset;
}







/**
 * Parse MQTT Connect packet body
 *
 */
ssize_t mqtt_parse_connect(struct mqtt_connect *payload, const unsigned char *buffer, size_t length)
{
    size_t offset = 0;    // Skip protocol_name size
    // Protocol Name
    offset += mqtt_get_string(buffer+offset, &payload->protocol_name, &payload->protocol_name_len);
    // Protocol Level
    offset += mqtt_get_byte(buffer+offset, &payload->protocol_level);

    unsigned char flags;
    // Connect Flags
    offset += mqtt_get_byte(buffer+offset, &flags);
    payload->clean_session = (flags & MQTT_CONNECT_CLEAN_SESSION) ? true : false;
    payload->will_retain = (flags & MQTT_CONNECT_WILL_RETAIN) ? true : false;
    payload->will_qos = (flags & MQTT_CONNECT_WILL_QOS_MSK) >> MQTT_CONNECT_WILL_QOS_POS;
    // Keep Alive
    offset += mqtt_get_short(buffer+offset, &payload->keep_alive);

    // Client Identifier
    offset += mqtt_get_string(buffer+offset, &payload->client_id, &payload->client_id_len);
    if (flags & MQTT_CONNECT_WILL_FLAG) {
        offset += mqtt_get_string(buffer+offset, &payload->will_topic, &payload->will_topic_len);
        offset += mqtt_get_short(buffer+offset, &payload->will_msg_len);
        offset += mqtt_get_data(buffer+offset, &payload->will_msg, payload->will_msg_len);
    }
    if (flags & MQTT_CONNECT_USER_NAME) {
        offset += mqtt_get_string(buffer+offset, &payload->user_name, &payload->user_name_len);
    }
    if (flags & MQTT_CONNECT_PASSWORD) {
        offset += mqtt_get_short(buffer+offset, &payload->password_len);
        offset += mqtt_get_data(buffer+offset, &payload->password, payload->password_len);
    }

    if (offset < length)
        return -1;

    return offset;
}

/**
 * Evaluate MQTT Connect body size
 *
 */
size_t mqtt_eval_connect(const char *client_id, const char *will_topic, unsigned short will_msg_len,
                         const char *user_name, unsigned short password_len)
{
    size_t body_size = MQTT_CONNECT_VARIABLE_HEADER_SIZE;
    if (client_id)
        body_size += MQTT_STR_LENGTH_SIZE + strlen(client_id);
    if (will_topic)
        body_size += MQTT_STR_LENGTH_SIZE + strlen(will_topic);
    if (will_msg_len > 0)
        body_size += MQTT_STR_LENGTH_SIZE + will_msg_len;
    if (user_name)
        body_size += MQTT_STR_LENGTH_SIZE + strlen(user_name);
    if (password_len > 0)
        body_size += MQTT_STR_LENGTH_SIZE + password_len;

    return body_size;
}


/**
 * Format MQTT Connect packet body
 *
 * Buffer should be long enought to store entire message. Use mqtt_eval_connect() to find expected size of buffer
 */
size_t mqtt_format_connect(unsigned char *buffer, bool clean_session, unsigned short keep_alive, const char *client_id,
                         const char *will_topic, unsigned char *will_msg, unsigned short will_msg_len, bool will_retain, unsigned char will_qos,
                         const char *user_name, unsigned char *password, unsigned short password_len)
{
    size_t offset = 0;

    // --- Variable header ---
    // Protocol Name
    offset += mqtt_put_string(buffer+offset, MQTT_PROTOCOL_NAME);
    // Protocol Level
    offset += mqtt_put_byte(buffer+offset, MQTT_PROTOCOL_LEVEL);
    // Connect Flags
    unsigned char flags = 0;
    if (clean_session)
        flags |= MQTT_CONNECT_CLEAN_SESSION;
    if (will_topic && will_msg) {
        flags |= MQTT_CONNECT_WILL_FLAG;
        flags |= (will_qos << MQTT_CONNECT_WILL_QOS_POS) & MQTT_CONNECT_WILL_QOS_MSK;
        if (will_retain)
            flags |= MQTT_CONNECT_WILL_RETAIN;
    }
    if (password)
        flags |= MQTT_CONNECT_PASSWORD;
    if (user_name)
        flags |= MQTT_CONNECT_USER_NAME;
    offset += mqtt_put_byte(buffer+offset, flags);
    offset += mqtt_put_short(buffer+offset, keep_alive);

    // --- Payload ---
    // Client Identifier
    offset += mqtt_put_string(buffer+offset, client_id);
    // Will Topic
    if (will_topic)
        offset += mqtt_put_string(buffer+offset, will_topic);
    if (will_msg) {
        offset += mqtt_put_short(buffer+offset, will_msg_len);
        offset += mqtt_put_data(buffer+offset, will_msg, will_msg_len);
    }
    // User Name
    if (user_name)
        offset += mqtt_put_string(buffer+offset, user_name);
    if (password) {
        offset += mqtt_put_short(buffer+offset, password_len);
        offset += mqtt_put_data(buffer+offset, password, password_len);
    }

    return offset;
}



/**
 * Parse MQTT CONNACK packet body
 *
 */
ssize_t mqtt_parse_connack(struct mqtt_connack *payload, const unsigned char *buffer, size_t length)
{
    size_t offset = 0;

    // --- Variable header ---
    // Flags
    unsigned char flags = 0;
    offset += mqtt_get_byte(buffer+offset, &flags);
    payload->session_present = (flags & MQTT_CONNACK_SESSION_PRESENT_FLAG) ? true : false;
    // Return Code
    offset += mqtt_get_byte(buffer+offset, &payload->return_code);

    if (offset < length)
        return -1;

    return offset;
}


/**
 * Format MQTT CONNACK packet body
 *
 * Buffer should be at least MQTT_CONNACK_VARIABLE_HEADER_SIZE long
 *
 */
size_t mqtt_format_connack(unsigned char *buffer, bool session_present, unsigned char return_code)
{
    size_t offset = 0;

    // --- Variable header ---
    // Flags
    unsigned char flags = 0;
    if (session_present)
        flags |= MQTT_CONNACK_SESSION_PRESENT_FLAG;
    offset += mqtt_put_byte(buffer+offset, flags);
    // Return Code
    offset += mqtt_put_byte(buffer+offset, return_code);

    return offset;
}







/**
 * Parse MQTT PUBLISH packet body
 *
 */
ssize_t mqtt_parse_publish(struct mqtt_publish *payload, const unsigned char *buffer, size_t length, unsigned char qos)
{
    size_t offset = 0;

    // --- Variable header ---
    // Topic Name
    offset += mqtt_get_string(buffer+offset, &payload->topic, &payload->topic_len);
    // Packet Identifier
    if (qos > MQTT_QOS_0)
        offset += mqtt_get_short(buffer+offset, &payload->id);

    // --- Payload ---
    payload->payload_len = length-offset;
    offset += mqtt_get_data(buffer+offset, &payload->payload, payload->payload_len);

    return offset;
}


/**
 * Evaluate MQTT PUBLISH packet body length
 *
 */
size_t mqtt_eval_publish(unsigned char qos, const char *topic, size_t payload_len)
{
    size_t body_size = 0;

    body_size += MQTT_STR_LENGTH_SIZE + strlen(topic);
    if (qos > MQTT_QOS_0)
        body_size += MQTT_PACKET_ID_SIZE;
    body_size += payload_len;

    return body_size;
}


/**
 * Format MQTT PUBLISH packet body
 *
 *  Buffer should be long enought to store entire message. Use mqtt_eval_publish() to find expected size of buffer
 */
size_t mqtt_format_publish(unsigned char *buffer, unsigned char qos, unsigned short id, const char *topic,
                           const unsigned char *payload, size_t payload_len)
{
    unsigned int offset = 0;

    // --- Variable header ---
    //Topic Name
    offset += mqtt_put_string(buffer+offset, topic);
    // Packet Identifier
    if (qos > MQTT_QOS_0)
        offset += mqtt_put_short(buffer+offset, id);

    // --- Payload ---
    offset += mqtt_put_data(buffer+offset, payload, payload_len);

    return offset;
}





/**
 * Parse MQTT SUBSCRIBE packet body
 *
 */
ssize_t mqtt_parse_subscribe(struct mqtt_subscribe *payload, const unsigned char *buffer, size_t length)
{
    size_t offset = 0;

    // --- Variable header ---
    // Packet Identifier
    offset += mqtt_get_short(buffer+offset, &payload->id);

    // --- Payload ---
    // Topic Name
    offset += mqtt_get_string(buffer+offset, &payload->topic, &payload->topic_len);
    // Requested QoS
    offset += mqtt_get_byte(buffer+offset, &payload->qos);

    if (offset < length)
        return -1;

    return offset;
}


/**
 * Evaluate MQTT SUBSCRIBE packet body length
 *
 */
size_t mqtt_eval_subscribe(const char *topic)
{
    size_t body_size = 0;

    body_size += MQTT_PACKET_ID_SIZE;
    body_size += MQTT_STR_LENGTH_SIZE + strlen(topic);
    body_size += 1; // Requested QoS

    return body_size;
}


/**
 * Format MQTT SUBSCRIBE packet body
 *
 *  Buffer should be long enought to store entire message. Use mqtt_eval_subscribe() to find expected size of buffer
 */
size_t mqtt_format_subscribe(unsigned char *buffer, unsigned short id, const char *topic, unsigned char qos)
{
    unsigned int offset = 0;

    // --- Variable header ---
    // Packet Identifier
    offset += mqtt_put_short(buffer+offset, id);

    // --- Payload ---
    // Topic Filter
    offset += mqtt_put_string(buffer+offset, topic);
    // Requested QoS
    offset += mqtt_put_byte(buffer+offset, qos);

    return offset;
}





/**
 * Parse MQTT SUBACK packet body
 *
 */
ssize_t mqtt_parse_suback(struct mqtt_suback *payload, const unsigned char *buffer, size_t length)
{
    unsigned int offset = 0;

    // --- Variable header ---
    // Packet Identifier
    offset += mqtt_get_short(buffer+offset, &payload->id);

    // --- Payload ---
    // Return Code
    offset += mqtt_get_byte(buffer+offset, &payload->return_code);

    if (offset < length)
        return -1;

    return offset;
}


/**
 * Format MQTT SUBACK packet body
 *
 * Buffer should be at least (MQTT_SUBACK_VARIABLE_HEADER_SIZE + MQTT_SUBACK_PAYLOAD_SIZE) long
 *
 */
size_t mqtt_format_suback(unsigned char *buffer, unsigned short id, unsigned char return_code)
{
    unsigned int offset = 0;

    // --- Variable header ---
    // Packet Identifier
    offset += mqtt_put_short(buffer+offset, id);

    // --- Payload ---
    // Return Code
    offset += mqtt_put_byte(buffer+offset, return_code);

    return offset;
}





/**
 * Parse MQTT UNSUBSCRIBE packet body
 *
 */
ssize_t mqtt_parse_unsubscribe(struct mqtt_unsubscribe *payload, const unsigned char *buffer, size_t length)
{
    size_t offset = 0;

    // --- Variable header ---
    // Packet Identifier
    offset += mqtt_get_short(buffer+offset, &payload->id);

    // --- Payload ---
    // Topic Name
    offset += mqtt_get_string(buffer+offset, &payload->topic, &payload->topic_len);

    if (offset < length)
        return -1;

    return offset;
}


/**
 * Evaluate MQTT UNSUBSCRIBE packet body length
 *
 */
size_t mqtt_eval_unsubscribe(const char *topic)
{
    size_t body_size = 0;

    body_size += MQTT_PACKET_ID_SIZE;
    body_size += MQTT_STR_LENGTH_SIZE + strlen(topic);

    return body_size;
}


/**
 * Format MQTT UNSUBSCRIBE packet body
 *
 *  Buffer should be long enought to store entire message. Use mqtt_eval_unsubscribe() to find expected size of buffer
 */
size_t mqtt_format_unsubscribe(unsigned char *buffer, unsigned short id, const char *topic)
{
    unsigned int offset = 0;

    // --- Variable header ---
    // Packet Identifier
    offset += mqtt_put_short(buffer+offset, id);

    // --- Payload ---
    // Topic Filter
    offset += mqtt_put_string(buffer+offset, topic);

    return offset;
}





/**
 * Parse MQTT UNSUBACK packet body
 *
 */
ssize_t mqtt_parse_unsuback(struct mqtt_unsuback *payload, const unsigned char *buffer, size_t length)
{
    unsigned int offset = 0;

    // --- Variable header ---
    // Packet Identifier
    offset += mqtt_get_short(buffer+offset, &payload->id);

    if (offset < length)
        return -1;

    return offset;
}


/**
 * Format MQTT UNSUBACK packet body
 *
 * Buffer should be at least (MQTT_UNSUBACK_VARIABLE_HEADER_SIZE) long
 *
 */
size_t mqtt_format_unsuback(unsigned char *buffer, unsigned short id)
{
    unsigned int offset = 0;

    // --- Variable header ---
    // Packet Identifier
    offset += mqtt_put_short(buffer+offset, id);

    return offset;
}





/**
 * Put 1 byte into buffer
 *
 */
int mqtt_put_byte(unsigned char *buffer, unsigned char value)
{
    buffer[0] = value;
    return 1;
}


/**
 * Put short into buffer
 *
 */
int mqtt_put_short(unsigned char *buffer, unsigned short value)
{
    buffer[0] = (unsigned char)((value >> 8) & 0xFF);
    buffer[1] = (unsigned char)(value        & 0xFF);

    return 2;
}


/**
 * Put data into buffer
 *
 */
int mqtt_put_data(unsigned char *buffer, const unsigned char *data, unsigned short data_len)
{
    memcpy(buffer, data, data_len);
    return data_len;
}


/**
 * Put string into buffer
 *
 */
int mqtt_put_string(unsigned char *buffer, const char *str)
{
    size_t str_len = strlen(str);
    int offset = mqtt_put_short(buffer, str_len);
    if (str_len > 0)
        offset += mqtt_put_data(buffer+offset, (const unsigned char*)str, str_len);
    return offset;
}


/**
 * Get byte from buffer
 *
 */
int mqtt_get_byte(const unsigned char *buffer, unsigned char *value)
{
    *value = buffer[0];
    return 1;
}


/**
 * Get short from buffer
 */
int mqtt_get_short(const unsigned char *buffer, unsigned short *value)
{
    *value = ((unsigned short)buffer[0]) << 8;
    *value |= buffer[1];

    return 2;
}

/**
 * Get data from buffer
 */
int mqtt_get_data(const unsigned char *buffer, const unsigned char **data, unsigned short data_len)
{
    *data = buffer;

    return data_len;
}


/**
 * Get string from buffer
 *
 */
int mqtt_get_string(const unsigned char *buffer, const char **str, unsigned short *str_len)
{
    int offset = 0;
    offset += mqtt_get_short(buffer+offset, str_len);
    *str = (const char*)buffer+offset;
    return offset + *str_len;
}





/**
 * Initializer
 *
 */
void mqtt_msg_init(struct mqtt_msg *self)
{
    self->topic = NULL;
    self->payload = NULL;
    self->payload_len = 0;
    self->qos = MQTT_QOS_0;
    self->retain = false;
}


/**
 * Cleaner
 *
 */
void mqtt_msg_clean(struct mqtt_msg *self)
{
    if (self->topic)
        self->topic = xfree(self->topic);

    if (self->payload)
        self->payload = xfree(self->payload);
}


/*
 * Constructor
 *
 */
struct mqtt_msg* mqtt_msg_new(void)
{
    struct mqtt_msg *self = xmalloc(sizeof(struct mqtt_msg));
    mqtt_msg_init(self);
    return self;
}


/**
 * Constructor with arguments
 *
 */
struct mqtt_msg* mqtt_msg_create(const char *topic, const unsigned char *payload, unsigned short payload_len, unsigned short qos, bool retain)
{
    struct mqtt_msg *self = mqtt_msg_new();

    self->topic = xstrdup(topic);
    self->payload = xmemdup(payload, payload_len);
    self->payload_len = payload_len;
    self->qos = qos;
    self->retain = retain;

    return self;
}


/**
 * Destructor
 *
 */
struct mqtt_msg* mqtt_msg_delete(struct mqtt_msg *self)
{
    mqtt_msg_clean(self);

    return xfree(self);
}


/**
 * Copy operator
 *
 */
void mqtt_msg_copy(struct mqtt_msg *self, struct mqtt_msg *other)
{
    self->topic = xstrdup(other->topic);
    self->payload = xmemdup(other->payload, other->payload_len);
    self->payload_len = other->payload_len;
    self->qos = other->qos;
    self->retain = other->retain;
}


/**
 * Move operator
 *
 */
void mqtt_msg_move(struct mqtt_msg *self, struct mqtt_msg *other)
{
    self->topic = other->topic;
    other->topic = NULL;

    self->payload = other->payload;
    other->payload = NULL;
    self->payload_len = other->payload_len;
    other->payload_len = 0;

    self->qos = other->qos;
    self->retain = other->retain;
}





/**
 * Initializer
 *
 */
void mqtt_conf_init(struct mqtt_conf *self)
{
    self->client_id = NULL;
    self->keep_alive = MQTT_DEFAULT_KEEP_ALIVE;
    self->user_name = NULL;
    self->password = NULL;
    self->password_len = 0;
}


/**
 * Cleaner
 *
 */
void mqtt_conf_clean(struct mqtt_conf *self)
{
    if (self->client_id)
        self->client_id = xfree(self->client_id);

    if (self->user_name)
        self->user_name = xfree(self->user_name);

    if (self->password)
        self->password = xfree(self->password);
}


/**
 * Constructor
 *
 */
struct mqtt_conf* mqtt_conf_new(void)
{
    struct mqtt_conf *self = xmalloc(sizeof(struct mqtt_conf));
    mqtt_conf_init(self);
    return self;
}


/**
 * Constructor with arguments
 *
 */
struct mqtt_conf* mqtt_conf_create(const char *client_id, unsigned short keep_alive,
                                   const char *user_name, const unsigned char *password, unsigned short password_len)
{
    struct mqtt_conf *self = mqtt_conf_new();

    if (client_id)
        self->client_id = xstrdup(client_id);
    self->keep_alive = keep_alive;
    if (user_name)
        self->user_name = xstrdup(user_name);
    if (password && (password_len > 0)) {
        self->password = xmemdup(password, password_len);
        self->password_len = password_len;
    }

    return self;
}


/**
 * Destructor
 *
 */
struct mqtt_conf* mqtt_conf_delete(struct mqtt_conf *self)
{
    mqtt_conf_clean(self);
    return xfree(self);
}


/**
 * Copy operator
 *
 */
void mqtt_conf_copy(struct mqtt_conf *self, struct mqtt_conf *other)
{
    if (other->client_id)
        self->client_id = xstrdup(other->client_id);
    self->keep_alive = other->keep_alive;
    if (other->user_name)
        self->user_name = xstrdup(other->user_name);
    if (other->password && (other->password_len > 0)) {
        self->password = xmemdup(other->password, other->password_len);
        self->password_len = other->password_len;
    }
}


/**
 * Move operator
 *
 */
void mqtt_conf_move(struct mqtt_conf *self, struct mqtt_conf *other)
{
    self->client_id = other->client_id;
    other->client_id = NULL;

    self->keep_alive = other->keep_alive;
    self->user_name = other->user_name;
    other->user_name = NULL;

    self->password = other->password;
    other->password = NULL;

    self->password_len = other->password_len;
}


/**
 * Convert MQTT message type to string
 *
 */
const char* mqtt_packet_name(unsigned char type)
{
    if (type <= MQTT_DISCONNECT)
        return mqtt_packet_names[type];

    return "MQTT_UNKNOWN";
}


/**
 * Return MQTT response type by given MQTT request type
 *
 */
unsigned char mqtt_response_type(unsigned char req_type, unsigned char qos)
{
    switch (req_type) {
        case MQTT_SUBSCRIBE:
            return MQTT_SUBACK;

        case MQTT_UNSUBSCRIBE:
            return MQTT_UNSUBACK;

        case MQTT_PUBLISH:
            if (qos == MQTT_QOS_1)
                return MQTT_PUBACK;
            if (qos == MQTT_QOS_2)
                return MQTT_PUBREC;
            break;

        case MQTT_PUBREL:
            return MQTT_PUBCOMP;
    }

    return MQTT_FORBIDDEN;
}


/**
 * Return MQTT request type by given MQTT response type
 *
 */
unsigned char mqtt_request_type(unsigned char resp_type)
{
    switch (resp_type) {
        case MQTT_SUBACK:
            return MQTT_SUBSCRIBE;

        case MQTT_UNSUBACK:
            return MQTT_UNSUBSCRIBE;

        case MQTT_PUBACK:
        case MQTT_PUBREC:
            return MQTT_PUBLISH;

        case MQTT_PUBCOMP:
            return MQTT_PUBREL;
    }

    return MQTT_FORBIDDEN;
}
