
#include "test.h"

#include "mx/stream_mqtt.h"
#include "mx/socket.h"
#include "mx/timer.h"

#include <CUnit/Basic.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>



#define TEST_CLIENT_ID_1        "client-id-1"
#define TEST_CLIENT_ID_2        "client-id-2"

#define TEST_MSG_ID_1           1001
#define TEST_MSG_ID_2           1002
#define TEST_MSG_ID_3           1003

#define TEST_TOPIC_1            "topic/one"
#define TEST_TOPIC_2            "topic/two"
#define TEST_TOPIC_3            "topic/three"

#define TEST_PAYLOAD_1          "payload_first"
#define TEST_PAYLOAD_2          "payload_second"
#define TEST_PAYLOAD_3          "payload_third"

#define TEST_KEEP_ALIVE         10



static void test_mqtt_validate_frame(void);
static void test_stream_mqtt_ack_for_unsent_publish(void);
static void test_stream_mqtt_duplicate_pubrec(void);

static void test_stream_mqtt_observer(void);
static void test_stream_mqtt_observer_control_frames(void);
static void test_stream_mqtt_misc(void);
static void test_stream_mqtt_peek_frame(void);
static void test_stream_mqtt_read_frame(void);

static void test_stream_mqtt_connect(void);
static void test_stream_mqtt_subscribe(void);
static void test_stream_mqtt_unsubscribe(void);
static void test_stream_mqtt_publish_qos_0(void);
static void test_stream_mqtt_publish_qos_1(void);
static void test_stream_mqtt_publish_qos_2(void);

static void test_stream_mqtt_ping(void);

static void test_stream_mqtt_resend_subscribe_unsubscribe(void);
static void test_stream_mqtt_resend_publish_pubrel(void);
static void test_stream_mqtt_resend_publish_flags(void);
static void test_stream_mqtt_queued_publish_qos_0(void);



CU_ErrorCode cu_test_stream_mqtt()
{
    // Test logging to terminal
    CU_pSuite suite = CU_add_suite("Suite stream_mqtt", NULL, NULL);
    if ( !suite ) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    CU_add_test(suite, "Test MQTT initial frame validation",        test_mqtt_validate_frame);
    CU_add_test(suite, "Test stream MQTT ACK for unsent publish",   test_stream_mqtt_ack_for_unsent_publish);
    CU_add_test(suite, "Test stream MQTT duplicate PUBREC",         test_stream_mqtt_duplicate_pubrec);

    CU_add_test(suite, "Test stream MQTT observer",                 test_stream_mqtt_observer);
    CU_add_test(suite, "Test stream MQTT observer control frames",  test_stream_mqtt_observer_control_frames);
    CU_add_test(suite, "Test stream mqtt miscellaneous functions",  test_stream_mqtt_misc);
    CU_add_test(suite, "Test stream mqtt peek frame",               test_stream_mqtt_peek_frame);
    CU_add_test(suite, "Test stream mqtt read frame",               test_stream_mqtt_read_frame);

    CU_add_test(suite, "Test stream mqtt connect",                  test_stream_mqtt_connect);
    CU_add_test(suite, "Test stream mqtt subscribe",                test_stream_mqtt_subscribe);
    CU_add_test(suite, "Test stream mqtt unsubscribe",              test_stream_mqtt_unsubscribe);
    CU_add_test(suite, "Test stream mqtt publish qos 0",            test_stream_mqtt_publish_qos_0);
    CU_add_test(suite, "Test stream mqtt publish qos 1",            test_stream_mqtt_publish_qos_1);
    CU_add_test(suite, "Test stream mqtt publish qos 2",            test_stream_mqtt_publish_qos_2);

    CU_add_test(suite, "Test stream mqtt ping request/response",    test_stream_mqtt_ping);

    CU_add_test(suite, "Test stream mqtt resend sub/unsub",         test_stream_mqtt_resend_subscribe_unsubscribe);
    CU_add_test(suite, "Test stream mqtt resend publish/pubrel",    test_stream_mqtt_resend_publish_pubrel);
    CU_add_test(suite, "Test stream mqtt resend publish flags",     test_stream_mqtt_resend_publish_flags);
    CU_add_test(suite, "Test stream mqtt queued publish qos 0",     test_stream_mqtt_queued_publish_qos_0);

    return CU_get_error();
}



void test_stream_mqtt_init(struct stream_mqtt **client, struct stream_mqtt **server)
{
    int sockets[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) < 0) {
          perror("opening stream socket pair");
          exit(1);
    }

    socket_set_non_blocking(sockets[0], 1);
    socket_set_non_blocking(sockets[1], 1);

    *client = stream_mqtt_new(stream_new(sockets[0]));
    *server = stream_mqtt_new(stream_new(sockets[1]));
}

void test_stream_mqtt_clean(struct stream_mqtt *client, struct stream_mqtt *server)
{
    close(stream_mqtt_get_fd(client));
    close(stream_mqtt_get_fd(server));

    stream_mqtt_delete(client);
    stream_mqtt_delete(server);
}




struct test_mqtt_observer_context {
    struct stream_mqtt *receiver;
    unsigned int calls;
    bool handled;
};

static int test_mqtt_on_publish(void *object, struct stream_mqtt *stream,
                                unsigned char type, unsigned char flags, void *msg)
{
    struct test_mqtt_observer_context *context = object;
    context->calls++;
    CU_ASSERT_PTR_EQUAL(stream, context->receiver);
    CU_ASSERT_EQUAL(type, MQTT_PUBLISH);
    CU_ASSERT_EQUAL(flags, MQTT_RETAIN_FLAG);

    if (type == MQTT_PUBLISH && msg) {
        const struct mqtt_publish *publish = msg;
        CU_ASSERT_EQUAL(publish->topic_len, strlen(TEST_TOPIC_1));
        CU_ASSERT_NSTRING_EQUAL(publish->topic, TEST_TOPIC_1, strlen(TEST_TOPIC_1));
        CU_ASSERT_EQUAL(publish->payload_len, strlen(TEST_PAYLOAD_1));
        CU_ASSERT_NSTRING_EQUAL(publish->payload, TEST_PAYLOAD_1, strlen(TEST_PAYLOAD_1));
    }
    else {
        CU_FAIL("Expected a decoded PUBLISH");
    }

    return context->handled;
}

static void test_stream_mqtt_observer(void)
{
    struct stream_mqtt *sender, *receiver;
    test_stream_mqtt_init(&sender, &receiver);
    struct test_mqtt_observer_context observer = { .receiver = receiver, .handled = true };
    stream_mqtt_set_observer(receiver, &observer, test_mqtt_on_publish);

    const unsigned char *payload = (const unsigned char *)TEST_PAYLOAD_1;
    size_t payload_len = strlen(TEST_PAYLOAD_1);
    CU_ASSERT_TRUE(stream_mqtt_publish(sender, true, false, MQTT_QOS_0, 0,
                                       TEST_TOPIC_1, payload, payload_len) > 0);
    CU_ASSERT_EQUAL(stream_mqtt_peek_frame(receiver), -1);
    CU_ASSERT_EQUAL(observer.calls, 1);

    observer.handled = false;
    CU_ASSERT_TRUE(stream_mqtt_publish(sender, true, false, MQTT_QOS_0, 0,
                                       TEST_TOPIC_1, payload, payload_len) > 0);
    CU_ASSERT_TRUE(stream_mqtt_peek_frame(receiver) > 0);
    unsigned char buffer[128];
    unsigned char type = 0, flags = 0;
    ssize_t bytes = stream_mqtt_read_frame(receiver, &type, &flags, buffer, sizeof(buffer));
    CU_ASSERT_TRUE(bytes > 0);
    CU_ASSERT_EQUAL(type, MQTT_PUBLISH);
    CU_ASSERT_EQUAL(flags, MQTT_RETAIN_FLAG);
    CU_ASSERT_EQUAL(observer.calls, 2);

    stream_mqtt_remove_observer(receiver);
    CU_ASSERT_TRUE(stream_mqtt_publish(sender, true, false, MQTT_QOS_0, 0,
                                       TEST_TOPIC_1, payload, payload_len) > 0);
    CU_ASSERT_TRUE(stream_mqtt_peek_frame(receiver) > 0);
    CU_ASSERT_TRUE(stream_mqtt_read_frame(receiver, &type, &flags, buffer, sizeof(buffer)) > 0);
    CU_ASSERT_EQUAL(observer.calls, 2);

    test_stream_mqtt_clean(sender, receiver);
}


struct test_mqtt_control_observer {
    struct stream_mqtt *stream;
    unsigned char expected_type;
    unsigned int calls;
};

static int test_mqtt_on_control_frame(void *object, struct stream_mqtt *stream,
                                      unsigned char type, unsigned char flags, void *msg)
{
    struct test_mqtt_control_observer *observer = object;
    observer->calls++;
    CU_ASSERT_PTR_EQUAL(stream, observer->stream);
    CU_ASSERT_EQUAL(type, observer->expected_type);

    if (type == MQTT_DISCONNECT) {
        CU_ASSERT_EQUAL(flags, 0);
        CU_ASSERT_PTR_NULL(msg);
        return 1;
    }

    CU_ASSERT_PTR_NOT_NULL(msg);
    if (!msg)
        return 1;

    if (type != MQTT_PUBLISH) {
        unsigned char expected_flags = type == MQTT_SUBSCRIBE || type == MQTT_UNSUBSCRIBE
                                       ? MQTT_QOS_TO_FLAGS(MQTT_QOS_1) : 0;
        CU_ASSERT_EQUAL(flags, expected_flags);
    }

    switch (type) {
        case MQTT_CONNECT: {
            const struct mqtt_connect *connect = msg;
            CU_ASSERT_TRUE(connect->clean_session);
            CU_ASSERT_EQUAL(connect->keep_alive, TEST_KEEP_ALIVE);
            CU_ASSERT_EQUAL(connect->client_id_len, strlen(TEST_CLIENT_ID_1));
            CU_ASSERT_NSTRING_EQUAL(connect->client_id, TEST_CLIENT_ID_1, strlen(TEST_CLIENT_ID_1));
            break;
        }
        case MQTT_CONNACK: {
            const struct mqtt_connack *connack = msg;
            CU_ASSERT_FALSE(connack->session_present);
            CU_ASSERT_EQUAL(connack->return_code, MQTT_CONNACK_ACCEPTED);
            break;
        }
        case MQTT_SUBSCRIBE: {
            const struct mqtt_subscribe *subscribe = msg;
            CU_ASSERT_EQUAL(subscribe->id, TEST_MSG_ID_1);
            CU_ASSERT_EQUAL(subscribe->qos, MQTT_QOS_1);
            CU_ASSERT_EQUAL(subscribe->topic_len, strlen(TEST_TOPIC_1));
            CU_ASSERT_NSTRING_EQUAL(subscribe->topic, TEST_TOPIC_1, strlen(TEST_TOPIC_1));
            break;
        }
        case MQTT_SUBACK: {
            const struct mqtt_suback *suback = msg;
            CU_ASSERT_EQUAL(suback->id, TEST_MSG_ID_1);
            CU_ASSERT_EQUAL(suback->return_code, MQTT_SUBACK_SUCCESS_MAX_QOS_1);
            break;
        }
        case MQTT_UNSUBSCRIBE: {
            const struct mqtt_unsubscribe *unsubscribe = msg;
            CU_ASSERT_EQUAL(unsubscribe->id, TEST_MSG_ID_1);
            CU_ASSERT_EQUAL(unsubscribe->topic_len, strlen(TEST_TOPIC_1));
            CU_ASSERT_NSTRING_EQUAL(unsubscribe->topic, TEST_TOPIC_1, strlen(TEST_TOPIC_1));
            break;
        }
        case MQTT_UNSUBACK:
            CU_ASSERT_EQUAL(((const struct mqtt_unsuback *)msg)->id, TEST_MSG_ID_1);
            break;
        case MQTT_PUBACK:
        case MQTT_PUBCOMP:
            CU_ASSERT_EQUAL(((const struct mqtt_var_header_id *)msg)->id, TEST_MSG_ID_1);
            break;
        case MQTT_PUBLISH:
            CU_ASSERT_EQUAL(MQTT_QOS_FROM_FLAGS(flags), MQTT_QOS_2);
            CU_ASSERT_EQUAL(((const struct mqtt_publish *)msg)->id, TEST_MSG_ID_1);
            break;
        default:
            CU_FAIL("Unexpected MQTT observer notification");
            break;
    }

    return 1;
}

static void test_mqtt_expect_notification(struct test_mqtt_control_observer *observer, unsigned char type)
{
    unsigned int previous_calls = observer->calls;
    observer->expected_type = type;
    CU_ASSERT_EQUAL(stream_mqtt_peek_frame(observer->stream), -1);
    CU_ASSERT_EQUAL(observer->calls, previous_calls + 1);
}

static void test_stream_mqtt_observer_control_frames(void)
{
    struct stream_mqtt *client, *server;
    test_stream_mqtt_init(&client, &server);
    struct test_mqtt_control_observer client_observer = { .stream = client };
    struct test_mqtt_control_observer server_observer = { .stream = server };
    stream_mqtt_set_observer(client, &client_observer, test_mqtt_on_control_frame);
    stream_mqtt_set_observer(server, &server_observer, test_mqtt_on_control_frame);

    CU_ASSERT_TRUE(stream_mqtt_connect(client, true, TEST_KEEP_ALIVE, TEST_CLIENT_ID_1,
                                       NULL, NULL, 0, false, MQTT_QOS_0, NULL, NULL, 0) > 0);
    test_mqtt_expect_notification(&server_observer, MQTT_CONNECT);
    CU_ASSERT_TRUE(stream_mqtt_connack(server, false, MQTT_CONNACK_ACCEPTED) > 0);
    test_mqtt_expect_notification(&client_observer, MQTT_CONNACK);

    CU_ASSERT_TRUE(stream_mqtt_subscribe(client, TEST_MSG_ID_1, TEST_TOPIC_1, MQTT_QOS_1) > 0);
    test_mqtt_expect_notification(&server_observer, MQTT_SUBSCRIBE);
    CU_ASSERT_TRUE(stream_mqtt_suback(server, TEST_MSG_ID_1, MQTT_SUBACK_SUCCESS_MAX_QOS_1) > 0);
    test_mqtt_expect_notification(&client_observer, MQTT_SUBACK);

    CU_ASSERT_TRUE(stream_mqtt_unsubscribe(client, TEST_MSG_ID_1, TEST_TOPIC_1) > 0);
    test_mqtt_expect_notification(&server_observer, MQTT_UNSUBSCRIBE);
    CU_ASSERT_TRUE(stream_mqtt_unsuback(server, TEST_MSG_ID_1) > 0);
    test_mqtt_expect_notification(&client_observer, MQTT_UNSUBACK);

    unsigned int client_calls = client_observer.calls;
    unsigned int server_calls = server_observer.calls;
    CU_ASSERT_TRUE(stream_mqtt_publish(client, false, false, MQTT_QOS_2, TEST_MSG_ID_1, TEST_TOPIC_1,
                                        (const unsigned char *)TEST_PAYLOAD_1, strlen(TEST_PAYLOAD_1)) > 0);
    CU_ASSERT_EQUAL(stream_mqtt_peek_frame(server), -1);
    CU_ASSERT_EQUAL(server_observer.calls, server_calls);
    CU_ASSERT_EQUAL(stream_mqtt_peek_frame(client), -1);
    CU_ASSERT_EQUAL(client_observer.calls, client_calls);
    test_mqtt_expect_notification(&server_observer, MQTT_PUBLISH);
    test_mqtt_expect_notification(&client_observer, MQTT_PUBCOMP);

    CU_ASSERT_TRUE(stream_mqtt_publish(client, false, false, MQTT_QOS_1, TEST_MSG_ID_1, TEST_TOPIC_1,
                                        (const unsigned char *)TEST_PAYLOAD_1, strlen(TEST_PAYLOAD_1)) > 0);
    stream_mqtt_remove_observer(server);
    CU_ASSERT_TRUE(stream_mqtt_peek_frame(server) > 0);
    test_mqtt_expect_notification(&client_observer, MQTT_PUBACK);

    unsigned char buffer[128], type, flags;
    CU_ASSERT_TRUE(stream_mqtt_read_frame(server, &type, &flags, buffer, sizeof(buffer)) > 0);
    stream_mqtt_set_observer(server, &server_observer, test_mqtt_on_control_frame);
    CU_ASSERT_TRUE(stream_mqtt_disconnect(client) > 0);
    test_mqtt_expect_notification(&server_observer, MQTT_DISCONNECT);

    test_stream_mqtt_clean(client, server);
}


/**
 *  Test stream mqtt misc
 *
 */
void test_stream_mqtt_misc(void)
{
    struct stream_mqtt *client, *server;
    test_stream_mqtt_init(&client, &server);

    ssize_t bytes;

    // Casting stream <-> stream_mqtt
    struct stream *stream = stream_mqtt_to_stream(client);
    CU_ASSERT_PTR_EQUAL(stream, client);
    CU_ASSERT_PTR_EQUAL(client, stream_mqtt_from_stream(stream));

    unsigned char byte = 0;
    // Error, stream mqtt supports only frame writting
    bytes = stream_write(stream_mqtt_to_stream(client), &byte, sizeof(byte));
    CU_ASSERT_EQUAL(bytes, 0);
    // Error, stream mqtt supports only frame reading
    bytes = stream_read(stream_mqtt_to_stream(client), &byte, sizeof(byte));
    CU_ASSERT_EQUAL(bytes, 0);

    // Flush
    stream_flush(stream_mqtt_to_stream(client));

    test_stream_mqtt_clean(client, server);
}


/**
 *  Test stream mqtt peek frame
 *
 */
void test_stream_mqtt_peek_frame(void)
{
    struct stream_mqtt *client, *server;
    test_stream_mqtt_init(&client, &server);

    unsigned char msg_invalid[] = {0x20, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    unsigned char msg_publish[] = { MQTT_PUBLISH << 4, 3, 0, 1, 'a' };
    ssize_t bytes;

    bytes = stream_mqtt_peek_frame(server);
    CU_ASSERT_EQUAL(bytes, -1);     // Nothing received

    write(stream_mqtt_get_fd(client), msg_invalid, sizeof(msg_invalid));
    bytes = stream_mqtt_peek_frame(server);
    CU_ASSERT_EQUAL(bytes, -1);     // More bytes needed

    for (size_t i = 0; i < sizeof(msg_publish) - 1; ++i) {
        CU_ASSERT_EQUAL(write(stream_mqtt_get_fd(client), msg_publish + i, 1), 1);
        CU_ASSERT_EQUAL(stream_mqtt_peek_frame(server), -1);
    }
    CU_ASSERT_EQUAL(write(stream_mqtt_get_fd(client), msg_publish + sizeof(msg_publish) - 1, 1), 1);
    CU_ASSERT_EQUAL(stream_mqtt_peek_frame(server), 3);

    test_stream_mqtt_clean(client, server);
}


/**
 *  Test stream mqtt read frame
 *
 */
void test_stream_mqtt_read_frame(void)
{
    struct stream_mqtt *client, *server;
    test_stream_mqtt_init(&client, &server);

    ssize_t bytes;
    unsigned char buffer[64];
    unsigned char type;
    unsigned char flags;

    // Nothing received
    bytes = stream_mqtt_read_frame(server, &type, &flags, buffer, 1);
    CU_ASSERT_EQUAL(bytes, -1);

    // First frame
    ssize_t written = stream_mqtt_publish(client, false, false, MQTT_QOS_0, 0,
                                          TEST_TOPIC_1, (const unsigned char *)TEST_PAYLOAD_1,
                                          strlen(TEST_PAYLOAD_1));
    CU_ASSERT_TRUE(written > MQTT_MIN_FIXED_HEADER_SIZE);
    ssize_t publish_body_len = written - MQTT_MIN_FIXED_HEADER_SIZE;
    bytes = stream_mqtt_peek_frame(server);
    CU_ASSERT_EQUAL(bytes, publish_body_len);

    // Second frame
    stream_mqtt_disconnect(client);
    bytes = stream_mqtt_peek_frame(server);
    CU_ASSERT_EQUAL(bytes, publish_body_len);      // Previously received data is expected

    // Buffer shorter than expected
    bytes = stream_mqtt_read_frame(server, &type, &flags, buffer, 1);
    CU_ASSERT_EQUAL(bytes, -1);

    bytes = stream_mqtt_read_frame(server, &type, &flags, buffer, sizeof(buffer));
    CU_ASSERT_EQUAL(bytes, publish_body_len);      // First frame received

    bytes = stream_mqtt_peek_frame(server);
    CU_ASSERT_EQUAL(bytes, 1);      // Second frame received, dummy byte is used to forward disconnect to application
    bytes = stream_mqtt_read_frame(server, &type, &flags, buffer, sizeof(buffer));
    CU_ASSERT_EQUAL(bytes, 1);

    test_stream_mqtt_clean(client, server);
}





/**
 *  Test MQTT connect frame
 *
 */
void test_stream_mqtt_connect(void)
{
    struct stream_mqtt *client, *server;
    test_stream_mqtt_init(&client, &server);

    unsigned char buffer[512];
    unsigned char type;
    unsigned char flags;
    ssize_t ret;
    ssize_t bytes;
    ssize_t written;

    written = stream_mqtt_connect(client, true, TEST_KEEP_ALIVE, TEST_CLIENT_ID_1, NULL, NULL, 0, false, 0, NULL, NULL, 0);

    bytes = stream_mqtt_peek_frame(server);
    CU_ASSERT_EQUAL(written-MQTT_MIN_FIXED_HEADER_SIZE, bytes);

    bytes = stream_mqtt_read_frame(server, &type, &flags, buffer, sizeof(buffer));
    CU_ASSERT_EQUAL(type, MQTT_CONNECT);
    CU_ASSERT_EQUAL(flags, 0);

    struct mqtt_connect connect_msg;
    ret = mqtt_parse_connect(&connect_msg, buffer, bytes);
    CU_ASSERT_EQUAL(bytes, ret);
    CU_ASSERT_EQUAL(connect_msg.keep_alive, TEST_KEEP_ALIVE);
    CU_ASSERT_NSTRING_EQUAL(connect_msg.client_id, TEST_CLIENT_ID_1, strlen(TEST_CLIENT_ID_1));

    // Response
    bytes = stream_mqtt_peek_frame(client);
    CU_ASSERT_EQUAL(bytes, -1);     // MQTT_CONNACK is not automatically sent by stream_mqtt

    stream_mqtt_connack(server, false, MQTT_CONNACK_ACCEPTED);
    bytes = stream_mqtt_peek_frame(client);
    CU_ASSERT_EQUAL(bytes, MQTT_CONNACK_VARIABLE_HEADER_SIZE);
    bytes = stream_mqtt_read_frame(client, &type, &flags, buffer, sizeof(buffer));
    CU_ASSERT_EQUAL(bytes, MQTT_CONNACK_VARIABLE_HEADER_SIZE);
    CU_ASSERT_EQUAL(type, MQTT_CONNACK);
    CU_ASSERT_EQUAL(flags, 0);

    struct mqtt_connack connack_msg;
    ret = mqtt_parse_connack(&connack_msg, buffer, bytes);
    CU_ASSERT_EQUAL(bytes, ret);
    CU_ASSERT_FALSE(connack_msg.session_present);
    CU_ASSERT_EQUAL(connack_msg.return_code, MQTT_CONNACK_ACCEPTED);

    test_stream_mqtt_clean(client, server);
}


static void test_stream_mqtt_establish_connection(struct stream_mqtt *client, struct stream_mqtt *server)
{
    unsigned char buffer[128], type, flags;

    CU_ASSERT_TRUE(stream_mqtt_connect(client, true, TEST_KEEP_ALIVE, TEST_CLIENT_ID_1,
                                       NULL, NULL, 0, false, MQTT_QOS_0, NULL, NULL, 0) > 0);
    CU_ASSERT_TRUE(stream_mqtt_peek_frame(server) > 0);
    CU_ASSERT_TRUE(stream_mqtt_read_frame(server, &type, &flags, buffer, sizeof(buffer)) > 0);

    CU_ASSERT_TRUE(stream_mqtt_connack(server, false, MQTT_CONNACK_ACCEPTED) > 0);
    CU_ASSERT_EQUAL(stream_mqtt_peek_frame(client), MQTT_CONNACK_VARIABLE_HEADER_SIZE);
    CU_ASSERT_EQUAL(stream_mqtt_read_frame(client, &type, &flags, buffer, sizeof(buffer)),
                    MQTT_CONNACK_VARIABLE_HEADER_SIZE);
}


/**
 *  Test MQTT subscribe frame
 *
 */
void test_stream_mqtt_subscribe(void)
{
    struct stream_mqtt *client, *server;
    test_stream_mqtt_init(&client, &server);
    test_stream_mqtt_establish_connection(client, server);

    unsigned char buffer[512];
    unsigned char type;
    unsigned char flags;
    ssize_t ret;
    ssize_t bytes;
    ssize_t written;

    written = stream_mqtt_subscribe(client, TEST_MSG_ID_1, TEST_TOPIC_1, MQTT_QOS_2);

    bytes = stream_mqtt_peek_frame(server);
    CU_ASSERT_EQUAL(written-MQTT_MIN_FIXED_HEADER_SIZE, bytes);

    bytes = stream_mqtt_read_frame(server, &type, &flags, buffer, sizeof(buffer));
    CU_ASSERT_EQUAL(type, MQTT_SUBSCRIBE);
    CU_ASSERT_EQUAL(MQTT_QOS_FROM_FLAGS(flags), MQTT_QOS_1);

    struct mqtt_subscribe subscribe_msg;
    mqtt_parse_subscribe(&subscribe_msg, buffer, bytes);
    CU_ASSERT_EQUAL(subscribe_msg.id, TEST_MSG_ID_1);
    CU_ASSERT_EQUAL(subscribe_msg.qos, MQTT_QOS_2);
    CU_ASSERT_NSTRING_EQUAL(subscribe_msg.topic, TEST_TOPIC_1, strlen(TEST_TOPIC_1));

    // Response
    bytes = stream_mqtt_peek_frame(client);
    CU_ASSERT_EQUAL(bytes, -1);     // MQTT_SUBACK is not automatically sent by stream_mqtt

    stream_mqtt_suback(server, TEST_MSG_ID_1, MQTT_SUBACK_SUCCESS_MAX_QOS_2);
    bytes = stream_mqtt_peek_frame(client);
    CU_ASSERT_EQUAL(bytes, MQTT_SUBACK_VARIABLE_HEADER_SIZE+MQTT_SUBACK_PAYLOAD_SIZE);
    bytes = stream_mqtt_read_frame(client, &type, &flags, buffer, sizeof(buffer));
    CU_ASSERT_EQUAL(bytes, MQTT_SUBACK_VARIABLE_HEADER_SIZE+MQTT_SUBACK_PAYLOAD_SIZE);
    CU_ASSERT_EQUAL(type, MQTT_SUBACK);
    CU_ASSERT_EQUAL(flags, 0);

    struct mqtt_suback suback_msg;
    ret = mqtt_parse_suback(&suback_msg, buffer, bytes);
    CU_ASSERT_EQUAL(bytes, ret);
    CU_ASSERT_EQUAL(suback_msg.id, TEST_MSG_ID_1);
    CU_ASSERT_EQUAL(suback_msg.return_code, MQTT_SUBACK_SUCCESS_MAX_QOS_2);

    test_stream_mqtt_clean(client, server);
}


/**
 *  Test MQTT unsubscribe frame
 *
 */
void test_stream_mqtt_unsubscribe(void)
{
    struct stream_mqtt *client, *server;
    test_stream_mqtt_init(&client, &server);
    test_stream_mqtt_establish_connection(client, server);

    unsigned char buffer[512];
    unsigned char type;
    unsigned char flags;
    ssize_t bytes;
    ssize_t written;

    written = stream_mqtt_unsubscribe(client, TEST_MSG_ID_1, TEST_TOPIC_1);

    bytes = stream_mqtt_peek_frame(server);
    CU_ASSERT_EQUAL(written-MQTT_MIN_FIXED_HEADER_SIZE, bytes);

    bytes = stream_mqtt_read_frame(server, &type, &flags, buffer, sizeof(buffer));
    CU_ASSERT_EQUAL(type, MQTT_UNSUBSCRIBE);
    CU_ASSERT_EQUAL(MQTT_QOS_FROM_FLAGS(flags), MQTT_QOS_1);

    struct mqtt_unsubscribe unsubscribe_msg;
    mqtt_parse_unsubscribe(&unsubscribe_msg, buffer, bytes);
    CU_ASSERT_EQUAL(unsubscribe_msg.id, TEST_MSG_ID_1);
    CU_ASSERT_NSTRING_EQUAL(unsubscribe_msg.topic, TEST_TOPIC_1, strlen(TEST_TOPIC_1));

    // Response
    bytes = stream_mqtt_peek_frame(client);
    CU_ASSERT_EQUAL(bytes, -1);     // MQTT_UNSUBACK is not automatically sent by stream_mqtt

    written = stream_mqtt_unsuback(server, TEST_MSG_ID_1);
    bytes = stream_mqtt_peek_frame(client);
    CU_ASSERT_EQUAL(written-MQTT_MIN_FIXED_HEADER_SIZE, bytes);

    test_stream_mqtt_clean(client, server);
}


/**
 *  Test MQTT publish frame with QoS 0
 *
 */
void test_stream_mqtt_publish_qos_0(void)
{
    struct stream_mqtt *client, *server;
    test_stream_mqtt_init(&client, &server);

    unsigned char buffer[512];
    unsigned char type;
    unsigned char flags;
    ssize_t bytes;
    ssize_t written;

    written = stream_mqtt_publish(client, true, false, MQTT_QOS_0, TEST_MSG_ID_1, TEST_TOPIC_1,
                                    (unsigned char *)TEST_PAYLOAD_1, strlen(TEST_PAYLOAD_1));

    bytes = stream_mqtt_peek_frame(server);
    CU_ASSERT_EQUAL(written-MQTT_MIN_FIXED_HEADER_SIZE, bytes);

    bytes = stream_mqtt_read_frame(server, &type, &flags, buffer, sizeof(buffer));
    CU_ASSERT_EQUAL(type, MQTT_PUBLISH);
    CU_ASSERT_EQUAL(MQTT_QOS_FROM_FLAGS(flags), MQTT_QOS_0);
    CU_ASSERT_TRUE(MQTT_RETAIN_FLAG & flags);
    CU_ASSERT_FALSE(MQTT_DUP_FLAG & flags);

    struct mqtt_publish publish_msg = {0};
    mqtt_parse_publish(&publish_msg, buffer, bytes, MQTT_QOS_FROM_FLAGS(flags));
    CU_ASSERT_NOT_EQUAL(publish_msg.id, TEST_MSG_ID_1);     // Message ID is not sent for QoS 0
    CU_ASSERT_EQUAL(publish_msg.topic_len, strlen(TEST_TOPIC_1));
    CU_ASSERT_NSTRING_EQUAL(publish_msg.topic, TEST_TOPIC_1, publish_msg.topic_len);
    CU_ASSERT_EQUAL(publish_msg.payload_len, strlen(TEST_PAYLOAD_1));
    CU_ASSERT_NSTRING_EQUAL(publish_msg.payload, TEST_PAYLOAD_1, publish_msg.payload_len);

    // Response
    bytes = stream_mqtt_peek_frame(client);
    CU_ASSERT_EQUAL(bytes, -1);     // MQTT_PUBLISH QoS 0 is not acknowledged

    test_stream_mqtt_clean(client, server);
}


/**
 *  Test MQTT publish frame with QoS 1
 *
 */
void test_stream_mqtt_publish_qos_1(void)
{
    struct stream_mqtt *client, *server;
    test_stream_mqtt_init(&client, &server);

    unsigned char buffer[512];
    unsigned char type;
    unsigned char flags;
    ssize_t bytes;
    ssize_t written;

    written = stream_mqtt_publish(client, false, true, MQTT_QOS_1, TEST_MSG_ID_1, TEST_TOPIC_1,
                                    (unsigned char *)TEST_PAYLOAD_1, strlen(TEST_PAYLOAD_1));

    bytes = stream_mqtt_peek_frame(server);
    CU_ASSERT_EQUAL(written-MQTT_MIN_FIXED_HEADER_SIZE, bytes);

    bytes = stream_mqtt_read_frame(server, &type, &flags, buffer, sizeof(buffer));
    CU_ASSERT_EQUAL(type, MQTT_PUBLISH);
    CU_ASSERT_EQUAL(MQTT_QOS_FROM_FLAGS(flags), MQTT_QOS_1);
    CU_ASSERT_FALSE(MQTT_RETAIN_FLAG & flags);
    CU_ASSERT_TRUE(MQTT_DUP_FLAG & flags);

    struct mqtt_publish publish_msg = {0};
    mqtt_parse_publish(&publish_msg, buffer, bytes, MQTT_QOS_FROM_FLAGS(flags));
    CU_ASSERT_EQUAL(publish_msg.id, TEST_MSG_ID_1);
    CU_ASSERT_EQUAL(publish_msg.topic_len, strlen(TEST_TOPIC_1));
    CU_ASSERT_NSTRING_EQUAL(publish_msg.topic, TEST_TOPIC_1, publish_msg.topic_len);
    CU_ASSERT_EQUAL(publish_msg.payload_len, strlen(TEST_PAYLOAD_1));
    CU_ASSERT_NSTRING_EQUAL(publish_msg.payload, TEST_PAYLOAD_1, publish_msg.payload_len);

    // Response
    bytes = stream_mqtt_peek_frame(client);
    CU_ASSERT_EQUAL(bytes, -1);     // MQTT_PUBACK is not forwarded to the application

    test_stream_mqtt_clean(client, server);
}


/**
 *  Test MQTT publish frame with QoS 2
 *
 */
void test_stream_mqtt_publish_qos_2(void)
{
    struct stream_mqtt *client, *server;
    test_stream_mqtt_init(&client, &server);

    unsigned char buffer[512];
    unsigned char type;
    unsigned char flags;
    ssize_t bytes;
    ssize_t written;

    written = stream_mqtt_publish(client, false, false, MQTT_QOS_2, TEST_MSG_ID_1, TEST_TOPIC_1,
                                    (unsigned char *)TEST_PAYLOAD_1, strlen(TEST_PAYLOAD_1));

    bytes = stream_mqtt_peek_frame(server);
    CU_ASSERT_EQUAL(bytes, -1);     // MQTT_PUBLISH is pending for now
    bytes = stream_mqtt_peek_frame(client);
    CU_ASSERT_EQUAL(bytes, -1);     // MQTT_PUBREC is not forwarded to the application

    bytes = stream_mqtt_peek_frame(server);
    CU_ASSERT_EQUAL(written-MQTT_MIN_FIXED_HEADER_SIZE, bytes);

    bytes = stream_mqtt_read_frame(server, &type, &flags, buffer, sizeof(buffer));
    CU_ASSERT_EQUAL(type, MQTT_PUBLISH);
    CU_ASSERT_EQUAL(MQTT_QOS_FROM_FLAGS(flags), MQTT_QOS_2);
    CU_ASSERT_FALSE(MQTT_RETAIN_FLAG & flags);
    CU_ASSERT_FALSE(MQTT_DUP_FLAG & flags);

    struct mqtt_publish publish_msg = {0};
    mqtt_parse_publish(&publish_msg, buffer, bytes, MQTT_QOS_FROM_FLAGS(flags));
    CU_ASSERT_EQUAL(publish_msg.id, TEST_MSG_ID_1);
    CU_ASSERT_EQUAL(publish_msg.topic_len, strlen(TEST_TOPIC_1));
    CU_ASSERT_NSTRING_EQUAL(publish_msg.topic, TEST_TOPIC_1, publish_msg.topic_len);
    CU_ASSERT_EQUAL(publish_msg.payload_len, strlen(TEST_PAYLOAD_1));
    CU_ASSERT_NSTRING_EQUAL(publish_msg.payload, TEST_PAYLOAD_1, publish_msg.payload_len);

    // Response
    bytes = stream_mqtt_peek_frame(client);
    CU_ASSERT_EQUAL(bytes, -1);     // MQTT_PUBCOMP is not forwarded to the application

    test_stream_mqtt_clean(client, server);
}



/**
 *  Test MQTT ping request/response
 *
 */
void test_stream_mqtt_ping(void)
{
    struct stream_mqtt *client, *server;
    test_stream_mqtt_init(&client, &server);
    unsigned int time = 100*1000;
    clock_update(time, time);

    unsigned char buffer[512];
    unsigned char type;
    unsigned char flags;
    ssize_t bytes;
    int status;

    status = stream_mqtt_get_status(client);
    CU_ASSERT_EQUAL(status, STREAM_ST_INIT);
    status = stream_mqtt_get_status(server);
    CU_ASSERT_EQUAL(status, STREAM_ST_INIT);

    // Connect
    stream_mqtt_connect(client, true, TEST_KEEP_ALIVE, TEST_CLIENT_ID_1, NULL, NULL, 0, false, 0, NULL, NULL, 0);
    bytes = stream_mqtt_peek_frame(server);
    CU_ASSERT_NOT_EQUAL(bytes, -1);     // MQTT_CONNECT
    bytes = stream_mqtt_read_frame(server, &type, &flags, buffer, sizeof(buffer));
    CU_ASSERT_EQUAL(type, MQTT_CONNECT);

    // Connack
    stream_mqtt_connack(server, false, MQTT_CONNACK_ACCEPTED);
    bytes = stream_mqtt_peek_frame(client);
    CU_ASSERT_NOT_EQUAL(bytes, -1);     // MQTT_CONNACK
    bytes = stream_mqtt_read_frame(client, &type, &flags, buffer, sizeof(buffer));
    CU_ASSERT_EQUAL(type, MQTT_CONNACK);

    // Mock keep-alive messages
    for (int i=0; i<5; i++) {
        time += TEST_KEEP_ALIVE*1000;   // skip TEST_KEEP_ALIVE interval
        clock_update(time, time);
        stream_time(stream_mqtt_to_stream(client));
        stream_mqtt_peek_frame(server);
        stream_mqtt_peek_frame(client);
        status = stream_mqtt_get_status(client);
        CU_ASSERT_EQUAL(status, STREAM_ST_READY);
        status = stream_mqtt_get_status(server);
        CU_ASSERT_EQUAL(status, STREAM_ST_READY);
    }

    // Mock keep-alive timeout
    time += 5*TEST_KEEP_ALIVE*1000;     // skip 5*TEST_KEEP_ALIVE interval
    clock_update(time, time);
    stream_time(stream_mqtt_to_stream(server));
    status = stream_mqtt_get_status(server);
    CU_ASSERT_EQUAL(status, STREAM_ST_CLOSING);

    // From the client point of view new ping request should be sent
    stream_time(stream_mqtt_to_stream(client));
    status = stream_mqtt_get_status(client);
    CU_ASSERT_EQUAL(status, STREAM_ST_READY);

    // Mock missing ping response
    time += TEST_KEEP_ALIVE*1000;       // skip TEST_KEEP_ALIVE interval
    clock_update(time, time);
    stream_time(stream_mqtt_to_stream(client));
    status = stream_mqtt_get_status(client);
    CU_ASSERT_EQUAL(status, STREAM_ST_CLOSING);

    test_stream_mqtt_clean(client, server);
}



/**
 *  Test MQTT resend subscribe/unsubscribe
 *
 */
void test_stream_mqtt_resend_subscribe_unsubscribe(void)
{
    struct stream_mqtt *client, *server;
    test_stream_mqtt_init(&client, &server);
    unsigned int time = 100*1000;
    clock_update(time, time);

    unsigned char buffer[512];
    unsigned char type;
    unsigned char flags;
    ssize_t bytes;
    struct mqtt_subscribe subscribe_msg;
    struct mqtt_unsubscribe unsubscribe_msg;

    stream_mqtt_subscribe(client, TEST_MSG_ID_1, TEST_TOPIC_1, MQTT_QOS_1);
    stream_mqtt_unsubscribe(client, TEST_MSG_ID_1, TEST_TOPIC_1);
    stream_mqtt_subscribe(client, TEST_MSG_ID_2, TEST_TOPIC_2, MQTT_QOS_2);

    stream_mqtt_peek_frame(server);
    bytes = stream_mqtt_read_frame(server, &type, &flags, buffer, sizeof(buffer));
    CU_ASSERT_EQUAL(type, MQTT_SUBSCRIBE);
    CU_ASSERT_EQUAL(flags, MQTT_QOS_TO_FLAGS(MQTT_QOS_1));
    mqtt_parse_subscribe(&subscribe_msg, buffer, bytes);
    CU_ASSERT_EQUAL(subscribe_msg.id, TEST_MSG_ID_1);
    CU_ASSERT_EQUAL(subscribe_msg.qos, MQTT_QOS_1);
    CU_ASSERT_NSTRING_EQUAL(subscribe_msg.topic, TEST_TOPIC_1, strlen(TEST_TOPIC_1));

    // Check resending
    for (int i=0; i<2; i++) {
        time += 33*1000;       // skip 33 seconds
        clock_update(time, time);
        stream_time(stream_mqtt_to_stream(client));     // Client should resend message

        stream_mqtt_peek_frame(server);
        bytes = stream_mqtt_read_frame(server, &type, &flags, buffer, sizeof(buffer));
        CU_ASSERT_TRUE(bytes > 0);
        CU_ASSERT_EQUAL(type, MQTT_SUBSCRIBE);
        CU_ASSERT_EQUAL(flags, MQTT_QOS_TO_FLAGS(MQTT_QOS_1));
        mqtt_parse_subscribe(&subscribe_msg, buffer, bytes);
        CU_ASSERT_EQUAL(subscribe_msg.id, TEST_MSG_ID_1);
        CU_ASSERT_EQUAL(subscribe_msg.qos, MQTT_QOS_1);
        CU_ASSERT_NSTRING_EQUAL(subscribe_msg.topic, TEST_TOPIC_1, strlen(TEST_TOPIC_1));
    }

    // Message MQTT_SUBSCRIBE should be abandoned
    time += 33*1000;       // skip 33 seconds
    clock_update(time, time);
    stream_time(stream_mqtt_to_stream(client));     // Client should send next message

    stream_mqtt_peek_frame(server);
    bytes = stream_mqtt_read_frame(server, &type, &flags, buffer, sizeof(buffer));
    CU_ASSERT_TRUE(bytes > 0);
    CU_ASSERT_EQUAL(type, MQTT_UNSUBSCRIBE);
    CU_ASSERT_EQUAL(flags, MQTT_QOS_TO_FLAGS(MQTT_QOS_1));
    mqtt_parse_unsubscribe(&unsubscribe_msg, buffer, bytes);
    CU_ASSERT_EQUAL(unsubscribe_msg.id, TEST_MSG_ID_1);
    CU_ASSERT_NSTRING_EQUAL(unsubscribe_msg.topic, TEST_TOPIC_1, strlen(TEST_TOPIC_1));

    // Check resending
    for (int i=0; i<2; i++) {
        time += 33*1000;       // skip 33 seconds
        clock_update(time, time);
        stream_time(stream_mqtt_to_stream(client));     // Client should resend message

        stream_mqtt_peek_frame(server);
        bytes = stream_mqtt_read_frame(server, &type, &flags, buffer, sizeof(buffer));
        CU_ASSERT_TRUE(bytes > 0);
        CU_ASSERT_EQUAL(type, MQTT_UNSUBSCRIBE);
        CU_ASSERT_EQUAL(flags, MQTT_QOS_TO_FLAGS(MQTT_QOS_1));
        mqtt_parse_unsubscribe(&unsubscribe_msg, buffer, bytes);
        CU_ASSERT_EQUAL(unsubscribe_msg.id, TEST_MSG_ID_1);
        CU_ASSERT_NSTRING_EQUAL(unsubscribe_msg.topic, TEST_TOPIC_1, strlen(TEST_TOPIC_1));
    }

    // Message MQTT_UNSUBSCRIBE should be abandoned
    time += 33*1000;       // skip 33 seconds
    clock_update(time, time);
    stream_time(stream_mqtt_to_stream(client));     // Client should send next message

    stream_mqtt_peek_frame(server);
    bytes = stream_mqtt_read_frame(server, &type, &flags, buffer, sizeof(buffer));
    CU_ASSERT_TRUE(bytes > 0);
    CU_ASSERT_EQUAL(type, MQTT_SUBSCRIBE);
    CU_ASSERT_EQUAL(flags, MQTT_QOS_TO_FLAGS(MQTT_QOS_1));
    mqtt_parse_subscribe(&subscribe_msg, buffer, bytes);
    CU_ASSERT_EQUAL(subscribe_msg.id, TEST_MSG_ID_2);
    CU_ASSERT_EQUAL(subscribe_msg.qos, MQTT_QOS_2);
    CU_ASSERT_NSTRING_EQUAL(subscribe_msg.topic, TEST_TOPIC_2, strlen(TEST_TOPIC_2));

    test_stream_mqtt_clean(client, server);
}


/**
 *  Test MQTT resend publish/pubrel
 *
 */
void test_stream_mqtt_resend_publish_pubrel(void)
{
    struct stream_mqtt *client, *server;
    test_stream_mqtt_init(&client, &server);
    unsigned int time = 100*1000;
    clock_update(time, time);

    unsigned char buffer[512];
    unsigned char type;
    unsigned char flags;
    ssize_t bytes;
    struct mqtt_publish publish_msg;

    stream_mqtt_publish(client, false, false, MQTT_QOS_2, TEST_MSG_ID_1, TEST_TOPIC_1,
                                (unsigned char *)TEST_PAYLOAD_1, strlen(TEST_PAYLOAD_1));
    stream_mqtt_publish(client, false, false, MQTT_QOS_2, TEST_MSG_ID_2, TEST_TOPIC_2,
                                (unsigned char *)TEST_PAYLOAD_2, strlen(TEST_PAYLOAD_2));
    stream_mqtt_publish(client, false, false, MQTT_QOS_2, TEST_MSG_ID_3, TEST_TOPIC_3,
                                (unsigned char *)TEST_PAYLOAD_3, strlen(TEST_PAYLOAD_3));

    // Retry twice, then process the response before the exchange is abandoned.
    for (int i=0; i<2; i++) {
        time += 33*1000;       // skip 33 seconds
        clock_update(time, time);
        stream_time(stream_mqtt_to_stream(client));     // Client should resend MSG_ID_1
    }

    stream_mqtt_peek_frame(server);     // Received MSG_ID_1, sent PUBREC
    stream_mqtt_peek_frame(client);     // Received PUBREC, sent PUBREL

    // Retry twice, then process the response before the exchange is abandoned.
    for (int i=0; i<2; i++) {
        time += 33*1000;       // skip 33 seconds
        clock_update(time, time);
        stream_time(stream_mqtt_to_stream(client));     // Client should resend PUBREL
    }

    stream_mqtt_peek_frame(server);     // Received PUBREL for MSG_ID_1, sent PUBCOMP
    stream_mqtt_peek_frame(client);     // Received PUBCOMP for MSG_ID_1
    stream_time(stream_mqtt_to_stream(client));     // Send queued MSG_ID_2

    bytes = stream_mqtt_read_frame(server, &type, &flags, buffer, sizeof(buffer));
    CU_ASSERT_FATAL(bytes > 0);
    CU_ASSERT_EQUAL(type, MQTT_PUBLISH);
    mqtt_parse_publish(&publish_msg, buffer, bytes, MQTT_QOS_FROM_FLAGS(flags));
    CU_ASSERT_EQUAL(publish_msg.id, TEST_MSG_ID_1);
    CU_ASSERT_NSTRING_EQUAL(publish_msg.topic, TEST_TOPIC_1, strlen(TEST_TOPIC_1));
    CU_ASSERT_NSTRING_EQUAL(publish_msg.payload, TEST_PAYLOAD_1, strlen(TEST_PAYLOAD_1));

    stream_mqtt_peek_frame(server);     // Received MSG_ID_2, sent PUBREC
    stream_mqtt_peek_frame(client);     // Received PUBREC, sent PUBREL
    stream_mqtt_peek_frame(server);     // Received PUBREL, sent PUBCOMP

    bytes = stream_mqtt_read_frame(server, &type, &flags, buffer, sizeof(buffer));
    CU_ASSERT_FATAL(bytes > 0);
    CU_ASSERT_EQUAL(type, MQTT_PUBLISH);
    mqtt_parse_publish(&publish_msg, buffer, bytes, MQTT_QOS_FROM_FLAGS(flags));
    CU_ASSERT_EQUAL(publish_msg.id, TEST_MSG_ID_2);
    CU_ASSERT_NSTRING_EQUAL(publish_msg.topic, TEST_TOPIC_2, strlen(TEST_TOPIC_2));
    CU_ASSERT_NSTRING_EQUAL(publish_msg.payload, TEST_PAYLOAD_2, strlen(TEST_PAYLOAD_2));

    test_stream_mqtt_clean(client, server);
}


void test_stream_mqtt_resend_publish_flags(void)
{
    for (unsigned char qos = MQTT_QOS_1; qos <= MQTT_QOS_2; ++qos) {
        struct stream_mqtt *client, *server;
        test_stream_mqtt_init(&client, &server);
        stream_mqtt_publish(client, true, false, qos, TEST_MSG_ID_1, TEST_TOPIC_1,
                            (const unsigned char *)TEST_PAYLOAD_1, strlen(TEST_PAYLOAD_1));

        for (int phase = 0; phase < (qos == MQTT_QOS_2 ? 2 : 1); ++phase) {
            unsigned char expected[512];
            unsigned char received[512];
            ssize_t length = read(stream_mqtt_get_fd(server), expected, sizeof(expected));
            CU_ASSERT_TRUE(length > MQTT_MIN_FIXED_HEADER_SIZE);
            if (length <= MQTT_MIN_FIXED_HEADER_SIZE)
                break;

            unsigned char flags = phase == 0 ?
                (MQTT_PUBLISH << 4) | MQTT_QOS_TO_FLAGS(qos) | MQTT_RETAIN_FLAG :
                (MQTT_PUBREL << 4) | MQTT_QOS_TO_FLAGS(MQTT_QOS_1);
            CU_ASSERT_EQUAL(expected[MQTT_TYPE_FLAGS_IDX], flags);
            if (phase == 0)
                expected[MQTT_TYPE_FLAGS_IDX] |= MQTT_DUP_FLAG;

            for (int retry = 0; retry < 2; ++retry) {
                clock_update(31 * 1000, 0);
                stream_time(stream_mqtt_to_stream(client));
                ssize_t bytes = read(stream_mqtt_get_fd(server), received, sizeof(received));
                CU_ASSERT_EQUAL(bytes, length);
                if (bytes == length)
                    CU_ASSERT_EQUAL(memcmp(received, expected, length), 0);
            }

            if (phase == 0 && qos == MQTT_QOS_2) {
                stream_mqtt_pubrec(server, TEST_MSG_ID_1);
                stream_mqtt_peek_frame(client);
            }
        }

        test_stream_mqtt_clean(client, server);
    }
}


static void test_mqtt_validate_frame(void)
{
    struct mqtt_fixed_header frame = { .type = MQTT_SUBSCRIBE, .flags = MQTT_QOS_TO_FLAGS(MQTT_QOS_1) };
    const unsigned char body[] = { 0, 1, 0, 1, 'a', MQTT_QOS_0 };
    const unsigned char zero_id[] = { 0, 0 };

    CU_ASSERT_TRUE(mqtt_validate_frame(&frame, body, sizeof(body)));
    CU_ASSERT_FALSE(mqtt_validate_frame(&frame, body, sizeof(body) - 1));
    frame.type = MQTT_UNSUBSCRIBE;
    CU_ASSERT_TRUE(mqtt_validate_frame(&frame, body, sizeof(body) - 1));
    CU_ASSERT_FALSE(mqtt_validate_frame(&frame, body, sizeof(body) - 2));

    frame.type = MQTT_PUBREL;
    CU_ASSERT_TRUE(mqtt_validate_frame(&frame, body, MQTT_PACKET_ID_SIZE));
    CU_ASSERT_FALSE(mqtt_validate_frame(&frame, zero_id, sizeof(zero_id)));
    frame.flags = 0;
    CU_ASSERT_FALSE(mqtt_validate_frame(&frame, body, MQTT_PACKET_ID_SIZE));

    frame.type = MQTT_FORBIDDEN;
    CU_ASSERT_FALSE(mqtt_validate_frame(&frame, NULL, 0));
    frame.type = 15;
    CU_ASSERT_FALSE(mqtt_validate_frame(&frame, NULL, 0));
}


static void test_stream_mqtt_ack_for_unsent_publish(void)
{
    struct stream_mqtt *sender, *peer;
    test_stream_mqtt_init(&sender, &peer);

    stream_mqtt_publish(sender, false, false, MQTT_QOS_1, TEST_MSG_ID_1, TEST_TOPIC_1,
                        (const unsigned char *)TEST_PAYLOAD_1, strlen(TEST_PAYLOAD_1));
    stream_mqtt_publish(sender, false, false, MQTT_QOS_1, TEST_MSG_ID_2, TEST_TOPIC_2,
                        (const unsigned char *)TEST_PAYLOAD_2, strlen(TEST_PAYLOAD_2));

    stream_mqtt_puback(peer, TEST_MSG_ID_1);
    stream_mqtt_puback(peer, TEST_MSG_ID_2);
    stream_mqtt_peek_frame(sender);
    CU_ASSERT_FALSE(stream_mqtt_is_frame_pending(sender, MQTT_PUBLISH, TEST_MSG_ID_1));
    CU_ASSERT_TRUE(stream_mqtt_is_frame_pending(sender, MQTT_PUBLISH, TEST_MSG_ID_2));

    stream_time(stream_mqtt_to_stream(sender));
    stream_mqtt_puback(peer, TEST_MSG_ID_2);
    stream_mqtt_peek_frame(sender);
    CU_ASSERT_FALSE(stream_mqtt_is_frame_pending(sender, MQTT_PUBLISH, TEST_MSG_ID_2));

    test_stream_mqtt_clean(sender, peer);
}


static void test_stream_mqtt_duplicate_pubrec(void)
{
    struct stream_mqtt *sender, *peer;
    test_stream_mqtt_init(&sender, &peer);
    unsigned char original[64];
    unsigned char received[64];
    int peer_fd = stream_mqtt_get_fd(peer);

    stream_mqtt_publish(sender, false, false, MQTT_QOS_2, TEST_MSG_ID_1, TEST_TOPIC_1,
                        (const unsigned char *)TEST_PAYLOAD_1, strlen(TEST_PAYLOAD_1));
    CU_ASSERT_TRUE(read(peer_fd, received, sizeof(received)) > 0);

    stream_mqtt_pubrec(peer, TEST_MSG_ID_2);
    stream_mqtt_peek_frame(sender);
    CU_ASSERT_EQUAL(read(peer_fd, received, sizeof(received)), -1);
    CU_ASSERT_TRUE(stream_mqtt_is_frame_pending(sender, MQTT_PUBLISH, TEST_MSG_ID_1));

    stream_mqtt_pubrec(peer, TEST_MSG_ID_1);
    stream_mqtt_peek_frame(sender);
    ssize_t length = read(peer_fd, original, sizeof(original));
    CU_ASSERT_EQUAL(length, MQTT_MIN_FIXED_HEADER_SIZE + MQTT_PACKET_ID_SIZE);
    if (length > 0)
        CU_ASSERT_EQUAL(original[MQTT_TYPE_FLAGS_IDX] >> 4, MQTT_PUBREL);

    stream_mqtt_pubrec(peer, TEST_MSG_ID_1);
    stream_mqtt_peek_frame(sender);
    ssize_t repeated = read(peer_fd, received, sizeof(received));
    CU_ASSERT_EQUAL(repeated, length);
    if (repeated == length && length > 0)
        CU_ASSERT_EQUAL(memcmp(original, received, length), 0);

    stream_mqtt_pubcomp(peer, TEST_MSG_ID_1);
    stream_mqtt_peek_frame(sender);
    CU_ASSERT_FALSE(stream_mqtt_is_frame_pending(sender, 0, 0));

    test_stream_mqtt_clean(sender, peer);
}


void test_stream_mqtt_queued_publish_qos_0(void)
{
    struct stream_mqtt *client, *server;
    test_stream_mqtt_init(&client, &server);

    stream_mqtt_publish(client, false, false, MQTT_QOS_1, TEST_MSG_ID_1, TEST_TOPIC_1,
                        (const unsigned char *)TEST_PAYLOAD_1, strlen(TEST_PAYLOAD_1));
    for (int i = 0; i < 2; ++i) {
        stream_mqtt_publish(client, false, false, MQTT_QOS_0, 0, TEST_TOPIC_2,
                            (const unsigned char *)TEST_PAYLOAD_2, strlen(TEST_PAYLOAD_2));
    }
    stream_mqtt_publish(client, false, false, MQTT_QOS_1, TEST_MSG_ID_3, TEST_TOPIC_3,
                        (const unsigned char *)TEST_PAYLOAD_3, strlen(TEST_PAYLOAD_3));

    unsigned char buffer[512];
    unsigned char type, flags;
    stream_mqtt_peek_frame(server);
    ssize_t bytes = stream_mqtt_read_frame(server, &type, &flags, buffer, sizeof(buffer));
    CU_ASSERT_TRUE(bytes > 0);
    stream_mqtt_peek_frame(client);
    stream_time(stream_mqtt_to_stream(client));

    for (int i = 0; i < 3; ++i) {
        stream_mqtt_peek_frame(server);
        bytes = stream_mqtt_read_frame(server, &type, &flags, buffer, sizeof(buffer));
        CU_ASSERT_TRUE(bytes > 0);
        if (bytes <= 0)
            continue;

        CU_ASSERT_EQUAL(type, MQTT_PUBLISH);
        CU_ASSERT_EQUAL(flags, MQTT_QOS_TO_FLAGS(i == 2 ? MQTT_QOS_1 : MQTT_QOS_0));
        struct mqtt_publish msg = {0};
        CU_ASSERT_EQUAL(mqtt_parse_publish(&msg, buffer, bytes, MQTT_QOS_FROM_FLAGS(flags)), bytes);
        const char *topic = i == 2 ? TEST_TOPIC_3 : TEST_TOPIC_2;
        const char *payload = i == 2 ? TEST_PAYLOAD_3 : TEST_PAYLOAD_2;
        CU_ASSERT_EQUAL(msg.topic_len, strlen(topic));
        CU_ASSERT_NSTRING_EQUAL(msg.topic, topic, msg.topic_len);
        CU_ASSERT_EQUAL(msg.payload_len, strlen(payload));
        CU_ASSERT_EQUAL(memcmp(msg.payload, payload, msg.payload_len), 0);
        if (i == 2)
            CU_ASSERT_EQUAL(msg.id, TEST_MSG_ID_3);
    }

    clock_update(31 * 1000, 0);
    stream_time(stream_mqtt_to_stream(client));
    stream_mqtt_peek_frame(server);
    bytes = stream_mqtt_read_frame(server, &type, &flags, buffer, sizeof(buffer));
    CU_ASSERT_TRUE(bytes > 0);
    if (bytes > 0) {
        CU_ASSERT_EQUAL(type, MQTT_PUBLISH);
        CU_ASSERT_EQUAL(flags, MQTT_QOS_TO_FLAGS(MQTT_QOS_1) | MQTT_DUP_FLAG);
        struct mqtt_publish msg = {0};
        mqtt_parse_publish(&msg, buffer, bytes, MQTT_QOS_FROM_FLAGS(flags));
        CU_ASSERT_EQUAL(msg.id, TEST_MSG_ID_3);
    }

    stream_mqtt_peek_frame(client);
    stream_time(stream_mqtt_to_stream(client));
    clock_update(31 * 1000, 0);
    stream_time(stream_mqtt_to_stream(client));
    CU_ASSERT_EQUAL(stream_mqtt_peek_frame(server), -1);

    test_stream_mqtt_clean(client, server);
}
