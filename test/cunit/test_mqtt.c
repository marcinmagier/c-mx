#include <CUnit/CUnit.h>

#include "mx/mqtt.h"
#include "mx/misc.h"

#include <string.h>



static void test_mqtt_msg(void);
static void test_mqtt_conf_optional_fields(void);
static void test_mqtt_conf_ownership(void);
static void test_mqtt_frame_lengths(void);
static void test_mqtt_connect_options(void);
static void test_mqtt_packet_ids(void);
static void test_mqtt_connack(void);
static void test_mqtt_invalid_publish(void);
static void test_mqtt_subscription_trailing_bytes(void);
static void test_mqtt_packet_types(void);



CU_ErrorCode cu_test_mqtt(void)
{
    CU_pSuite suite = CU_add_suite("MQTT", NULL, NULL);
    if ( !suite ) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    CU_add_test(suite, "Message ownership",             test_mqtt_msg);
    CU_add_test(suite, "Optional configuration fields", test_mqtt_conf_optional_fields);
    CU_add_test(suite, "Configuration ownership",       test_mqtt_conf_ownership);
    CU_add_test(suite, "Frame lengths",                 test_mqtt_frame_lengths);
    CU_add_test(suite, "CONNECT options",               test_mqtt_connect_options);
    CU_add_test(suite, "Packet IDs",                    test_mqtt_packet_ids);
    CU_add_test(suite, "CONNACK session present",       test_mqtt_connack);
    CU_add_test(suite, "Invalid PUBLISH bodies",        test_mqtt_invalid_publish);
    CU_add_test(suite, "Subscription trailing bytes",   test_mqtt_subscription_trailing_bytes);
    CU_add_test(suite, "Packet types",                  test_mqtt_packet_types);

    return CU_get_error();
}



static void test_mqtt_msg(void)
{
    char topic[] = "test/topic";
    unsigned char payload[] = {0x41, 0x00, 0xff};
    const unsigned char expected[] = {0x41, 0x00, 0xff};
    struct mqtt_msg *copy = mqtt_msg_new();
    struct mqtt_msg moved;

    CU_ASSERT_PTR_NULL(copy->topic);
    CU_ASSERT_PTR_NULL(copy->payload);
    CU_ASSERT_EQUAL(copy->payload_len, 0);
    CU_ASSERT_EQUAL(copy->qos, MQTT_QOS_0);
    CU_ASSERT_FALSE(copy->retain);

    struct mqtt_msg *source = mqtt_msg_create(topic, payload, sizeof(payload), MQTT_QOS_2, true);
    CU_ASSERT_PTR_NOT_EQUAL(source->topic, topic);
    CU_ASSERT_PTR_NOT_EQUAL(source->payload, payload);
    topic[0] = 'X';
    payload[0] = 0;

    mqtt_msg_copy(copy, source);
    CU_ASSERT_PTR_NOT_EQUAL(copy->topic, source->topic);
    CU_ASSERT_PTR_NOT_EQUAL(copy->payload, source->payload);
    CU_ASSERT_PTR_NULL(mqtt_msg_delete(source));

    char *copied_topic = copy->topic;
    unsigned char *copied_payload = copy->payload;
    mqtt_msg_init(&moved);
    mqtt_msg_move(&moved, copy);
    CU_ASSERT_PTR_EQUAL(moved.topic, copied_topic);
    CU_ASSERT_PTR_EQUAL(moved.payload, copied_payload);
    CU_ASSERT_PTR_NULL(copy->topic);
    CU_ASSERT_PTR_NULL(copy->payload);
    CU_ASSERT_EQUAL(copy->payload_len, 0);
    CU_ASSERT_PTR_NULL(mqtt_msg_delete(copy));

    CU_ASSERT_PTR_NOT_NULL(moved.topic);
    if (moved.topic)
        CU_ASSERT_STRING_EQUAL(moved.topic, "test/topic");
    CU_ASSERT_EQUAL(moved.payload_len, sizeof(expected));
    if (moved.payload && moved.payload_len == sizeof(expected))
        CU_ASSERT_EQUAL(memcmp(moved.payload, expected, sizeof(expected)), 0);
    CU_ASSERT_PTR_NOT_NULL(moved.payload);
    CU_ASSERT_EQUAL(moved.qos, MQTT_QOS_2);
    CU_ASSERT_TRUE(moved.retain);
    mqtt_msg_clean(&moved);
    CU_ASSERT_PTR_NULL(moved.topic);
    CU_ASSERT_PTR_NULL(moved.payload);
}


static void test_mqtt_conf_optional_fields(void)
{
    const unsigned char password[] = {0xff};
    struct mqtt_conf *conf = mqtt_conf_new();
    struct mqtt_conf *anonymous = mqtt_conf_create(NULL, 0, NULL, NULL, sizeof(password));
    struct mqtt_conf *empty_password = mqtt_conf_create(NULL, 60, NULL, password, 0);

    CU_ASSERT_EQUAL(conf->keep_alive, 180);
    CU_ASSERT_PTR_NULL(conf->client_id);
    CU_ASSERT_PTR_NULL(conf->user_name);
    CU_ASSERT_PTR_NULL(conf->password);
    CU_ASSERT_EQUAL(conf->password_len, 0);

    mqtt_conf_copy(conf, anonymous);
    CU_ASSERT_EQUAL(conf->keep_alive, 0);
    CU_ASSERT_PTR_NULL(conf->client_id);
    CU_ASSERT_PTR_NULL(conf->user_name);
    CU_ASSERT_PTR_NULL(conf->password);
    CU_ASSERT_EQUAL(conf->password_len, 0);
    CU_ASSERT_PTR_NULL(empty_password->password);
    CU_ASSERT_EQUAL(empty_password->password_len, 0);

    CU_ASSERT_PTR_NULL(mqtt_conf_delete(empty_password));
    CU_ASSERT_PTR_NULL(mqtt_conf_delete(anonymous));
    CU_ASSERT_PTR_NULL(mqtt_conf_delete(conf));
}


static void test_mqtt_conf_ownership(void)
{
    char client_id[] = "test-client";
    char user[] = "test-user";
    unsigned char password[] = {0x70, 0x00, 0xff};
    const unsigned char expected[] = {0x70, 0x00, 0xff};
    struct mqtt_conf copy;
    struct mqtt_conf moved;
    struct mqtt_conf *source = mqtt_conf_create(client_id, 45, user, password, sizeof(password));

    CU_ASSERT_PTR_NOT_EQUAL(source->client_id, client_id);
    CU_ASSERT_PTR_NOT_EQUAL(source->user_name, user);
    CU_ASSERT_PTR_NOT_EQUAL(source->password, password);
    client_id[0] = 'X';
    user[0] = 'X';
    password[0] = 0;

    mqtt_conf_init(&copy);
    mqtt_conf_copy(&copy, source);
    CU_ASSERT_PTR_NOT_EQUAL(copy.client_id, source->client_id);
    CU_ASSERT_PTR_NOT_EQUAL(copy.user_name, source->user_name);
    CU_ASSERT_PTR_NOT_EQUAL(copy.password, source->password);
    CU_ASSERT_PTR_NULL(mqtt_conf_delete(source));

    char *copied_id = copy.client_id;
    char *copied_user = copy.user_name;
    unsigned char *copied_password = copy.password;
    mqtt_conf_init(&moved);
    mqtt_conf_move(&moved, &copy);
    CU_ASSERT_PTR_EQUAL(moved.client_id, copied_id);
    CU_ASSERT_PTR_EQUAL(moved.user_name, copied_user);
    CU_ASSERT_PTR_EQUAL(moved.password, copied_password);
    CU_ASSERT_PTR_NULL(copy.client_id);
    CU_ASSERT_PTR_NULL(copy.user_name);
    CU_ASSERT_PTR_NULL(copy.password);
    mqtt_conf_clean(&copy);

    CU_ASSERT_PTR_NOT_NULL(moved.client_id);
    if (moved.client_id)
        CU_ASSERT_STRING_EQUAL(moved.client_id, "test-client");
    CU_ASSERT_PTR_NOT_NULL(moved.user_name);
    if (moved.user_name)
        CU_ASSERT_STRING_EQUAL(moved.user_name, "test-user");
    CU_ASSERT_EQUAL(moved.password_len, sizeof(expected));
    CU_ASSERT_PTR_NOT_NULL(moved.password);
    if (moved.password && moved.password_len == sizeof(expected))
        CU_ASSERT_EQUAL(memcmp(moved.password, expected, sizeof(expected)), 0);
    CU_ASSERT_EQUAL(moved.keep_alive, 45);

    mqtt_conf_clean(&moved);
    CU_ASSERT_PTR_NULL(moved.client_id);
    CU_ASSERT_PTR_NULL(moved.user_name);
    CU_ASSERT_PTR_NULL(moved.password);
}


static void test_mqtt_frame_lengths(void)
{
    const unsigned char body[128] = {0};
    unsigned char buffer[MQTT_MAX_FIXED_HEADER_SIZE + sizeof(body)];
    struct mqtt_fixed_header frame = {0};
    size_t expected = 0;
    size_t length = mqtt_format_frame(buffer, MQTT_PUBLISH, MQTT_RETAIN_FLAG, body, sizeof(body));

    CU_ASSERT_EQUAL(length, 131);
    CU_ASSERT_EQUAL(buffer[0], 0x31);
    CU_ASSERT_EQUAL(buffer[1], 0x80);
    CU_ASSERT_EQUAL(buffer[2], 0x01);
    CU_ASSERT_EQUAL(mqtt_parse_frame(&frame, buffer, 1, &expected), 0);
    CU_ASSERT_EQUAL(expected, 2);
    CU_ASSERT_EQUAL(mqtt_parse_frame(&frame, buffer, 2, &expected), 0);
    CU_ASSERT_EQUAL(expected, 3);
    CU_ASSERT_EQUAL(mqtt_parse_frame(&frame, buffer, 2, NULL), 0);
    CU_ASSERT_EQUAL(mqtt_parse_frame(&frame, buffer, 3, &expected), 0);
    CU_ASSERT_EQUAL(expected, length);
    CU_ASSERT_EQUAL(mqtt_parse_frame(&frame, buffer, length, &expected), 3);
    CU_ASSERT_EQUAL(expected, length);
    CU_ASSERT_EQUAL(frame.type, MQTT_PUBLISH);
    CU_ASSERT_EQUAL(frame.flags, MQTT_RETAIN_FLAG);
    CU_ASSERT_EQUAL(frame.body_length, sizeof(body));
}


static void test_mqtt_connect_options(void)
{
    unsigned char buffer[64];
    unsigned char will[] = {0x00, 0xff};
    const unsigned char password[] = {0x70, 0x00};
    struct mqtt_connect packet = {0};
    size_t length = mqtt_format_connect(buffer, true, 60, "client", "will",
                                       will, sizeof(will), true, MQTT_QOS_2,
                                       "user", password, sizeof(password));

    CU_ASSERT_EQUAL(length, mqtt_eval_connect("client", "will", sizeof(will),
                                              "user", password, sizeof(password)));
    CU_ASSERT_EQUAL_FATAL(mqtt_parse_connect(&packet, buffer, length), length);
    CU_ASSERT_EQUAL(packet.protocol_name_len, MQTT_PROTOCOL_NAME_SIZE);
    CU_ASSERT_NSTRING_EQUAL(packet.protocol_name, MQTT_PROTOCOL_NAME, MQTT_PROTOCOL_NAME_SIZE);
    CU_ASSERT_EQUAL(packet.protocol_level, MQTT_PROTOCOL_LEVEL);
    CU_ASSERT_TRUE(packet.clean_session);
    CU_ASSERT_EQUAL(packet.keep_alive, 60);
    CU_ASSERT_EQUAL(packet.client_id_len, 6);
    CU_ASSERT_NSTRING_EQUAL(packet.client_id, "client", 6);
    CU_ASSERT_EQUAL(packet.will_topic_len, 4);
    CU_ASSERT_NSTRING_EQUAL(packet.will_topic, "will", 4);
    CU_ASSERT_TRUE(packet.will_retain);
    CU_ASSERT_EQUAL(packet.will_qos, MQTT_QOS_2);
    CU_ASSERT_EQUAL_FATAL(packet.will_msg_len, sizeof(will));
    CU_ASSERT_EQUAL(memcmp(packet.will_msg, will, sizeof(will)), 0);
    CU_ASSERT_EQUAL(packet.user_name_len, 4);
    CU_ASSERT_NSTRING_EQUAL(packet.user_name, "user", 4);
    CU_ASSERT_EQUAL_FATAL(packet.password_len, sizeof(password));
    CU_ASSERT_EQUAL(memcmp(packet.password, password, sizeof(password)), 0);
    buffer[length] = 0;
    CU_ASSERT_EQUAL(mqtt_parse_connect(&packet, buffer, length + 1), -1);
}


static void test_mqtt_packet_ids(void)
{
    unsigned char buffer[MQTT_PACKET_ID_SIZE + 1] = {0};
    struct mqtt_var_header_id header = {0};
    struct mqtt_unsuback unsuback = {0};

    CU_ASSERT_EQUAL(mqtt_format_var_header_id(buffer, 0x1234), MQTT_PACKET_ID_SIZE);
    CU_ASSERT_EQUAL(buffer[0], 0x12);
    CU_ASSERT_EQUAL(buffer[1], 0x34);
    CU_ASSERT_EQUAL(mqtt_parse_var_header_id(&header, buffer, MQTT_PACKET_ID_SIZE), MQTT_PACKET_ID_SIZE);
    CU_ASSERT_EQUAL(header.id, 0x1234);
    CU_ASSERT_EQUAL(mqtt_parse_var_header_id(&header, buffer, sizeof(buffer)), -1);

    CU_ASSERT_EQUAL(mqtt_format_unsuback(buffer, 0xabcd), MQTT_UNSUBACK_VARIABLE_HEADER_SIZE);
    CU_ASSERT_EQUAL(buffer[0], 0xab);
    CU_ASSERT_EQUAL(buffer[1], 0xcd);
    CU_ASSERT_EQUAL(mqtt_parse_unsuback(&unsuback, buffer, MQTT_UNSUBACK_VARIABLE_HEADER_SIZE),
                    MQTT_UNSUBACK_VARIABLE_HEADER_SIZE);
    CU_ASSERT_EQUAL(unsuback.id, 0xabcd);
    CU_ASSERT_EQUAL(mqtt_parse_unsuback(&unsuback, buffer, sizeof(buffer)), -1);
}


static void test_mqtt_connack(void)
{
    unsigned char buffer[MQTT_CONNACK_VARIABLE_HEADER_SIZE + 1] = {0};
    struct mqtt_connack packet = {0};
    size_t length = mqtt_format_connack(buffer, true, MQTT_CONNACK_ACCEPTED);

    CU_ASSERT_EQUAL(length, MQTT_CONNACK_VARIABLE_HEADER_SIZE);
    CU_ASSERT_EQUAL(buffer[0], MQTT_CONNACK_SESSION_PRESENT_FLAG);
    CU_ASSERT_EQUAL(mqtt_parse_connack(&packet, buffer, length), length);
    CU_ASSERT_TRUE(packet.session_present);
    CU_ASSERT_EQUAL(packet.return_code, MQTT_CONNACK_ACCEPTED);
    CU_ASSERT_EQUAL(mqtt_parse_connack(&packet, buffer, sizeof(buffer)), -1);
}


static void test_mqtt_invalid_publish(void)
{
    const unsigned char empty_topic[] = {0, 0};
    const unsigned char topic[] = {0, 1, 'a'};
    const unsigned char missing_topic[] = {0, 2, 'a'};
    const unsigned char zero_id[] = {0, 1, 'a', 0, 0};
    struct mqtt_publish packet = {0};

    CU_ASSERT_EQUAL(mqtt_parse_publish(&packet, topic, 1, MQTT_QOS_0), -1);
    CU_ASSERT_EQUAL(mqtt_parse_publish(&packet, topic, sizeof(topic), MQTT_QOS_2 + 1), -1);
    CU_ASSERT_EQUAL(mqtt_parse_publish(&packet, empty_topic, sizeof(empty_topic), MQTT_QOS_0), -1);
    CU_ASSERT_EQUAL(mqtt_parse_publish(&packet, missing_topic, sizeof(missing_topic), MQTT_QOS_0), -1);
    CU_ASSERT_EQUAL(mqtt_parse_publish(&packet, topic, sizeof(topic), MQTT_QOS_1), -1);
    CU_ASSERT_EQUAL(mqtt_parse_publish(&packet, zero_id, sizeof(zero_id), MQTT_QOS_1), -1);
}


static void test_mqtt_subscription_trailing_bytes(void)
{
    const unsigned char subscribe[] = {0, 1, 0, 1, 'a', MQTT_QOS_0, 0};
    const unsigned char suback[] = {0, 1, MQTT_SUBACK_SUCCESS_MAX_QOS_0, 0};
    const unsigned char unsubscribe[] = {0, 1, 0, 1, 'a', 0};
    struct mqtt_subscribe sub = {0};
    struct mqtt_suback ack = {0};
    struct mqtt_unsubscribe unsub = {0};

    CU_ASSERT_EQUAL(mqtt_parse_subscribe(&sub, subscribe, sizeof(subscribe)), -1);
    CU_ASSERT_EQUAL(mqtt_parse_suback(&ack, suback, sizeof(suback)), -1);
    CU_ASSERT_EQUAL(mqtt_parse_unsubscribe(&unsub, unsubscribe, sizeof(unsubscribe)), -1);
}


static void test_mqtt_packet_types(void)
{
    const struct {
        unsigned char response;
        unsigned char request;
    } cases[] = {
        {MQTT_SUBACK, MQTT_SUBSCRIBE},
        {MQTT_UNSUBACK, MQTT_UNSUBSCRIBE},
        {MQTT_PUBACK, MQTT_PUBLISH},
        {MQTT_PUBREC, MQTT_PUBLISH},
        {MQTT_PUBCOMP, MQTT_PUBREL},
        {MQTT_CONNECT, MQTT_FORBIDDEN},
    };

    for (unsigned int i = 0; i < ARRAY_SIZE(cases); i++)
        CU_ASSERT_EQUAL(mqtt_request_type(cases[i].response), cases[i].request);
    CU_ASSERT_STRING_EQUAL(mqtt_packet_name(MQTT_PUBLISH), "MQTT_PUBLISH");
    CU_ASSERT_STRING_EQUAL(mqtt_packet_name(MQTT_DISCONNECT + 1), "MQTT_UNKNOWN");
}
