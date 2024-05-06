
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



CU_ErrorCode cu_test_stream_mqtt()
{
    // Test logging to terminal
    CU_pSuite suite = CU_add_suite("Suite stream_mqtt", NULL, NULL);
    if ( !suite ) {
        CU_cleanup_registry();
        return CU_get_error();
    }

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
    unsigned char msg_connack[] = {0x20, 0x02, 0x00, 0x00};
    ssize_t bytes;

    bytes = stream_mqtt_peek_frame(server);
    CU_ASSERT_EQUAL(bytes, -1);     // Nothing received

    write(stream_mqtt_get_fd(client), msg_invalid, sizeof(msg_invalid));
    bytes = stream_mqtt_peek_frame(server);
    CU_ASSERT_EQUAL(bytes, -1);     // More bytes needed

    write(stream_mqtt_get_fd(client), msg_connack, 1);
    bytes = stream_mqtt_peek_frame(server);
    CU_ASSERT_EQUAL(bytes, -1);     // More bytes needed
    write(stream_mqtt_get_fd(client), msg_connack+1, 1);
    bytes = stream_mqtt_peek_frame(server);
    CU_ASSERT_EQUAL(bytes, -1);     // More bytes needed
    write(stream_mqtt_get_fd(client), msg_connack+2, 1);
    bytes = stream_mqtt_peek_frame(server);
    CU_ASSERT_EQUAL(bytes, -1);     // More bytes needed
    write(stream_mqtt_get_fd(client), msg_connack+3, 1);
    bytes = stream_mqtt_peek_frame(server);
    CU_ASSERT_EQUAL(bytes, 2);

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
    stream_mqtt_connack(client, false, MQTT_CONNACK_ACCEPTED);
    bytes = stream_mqtt_peek_frame(server);
    CU_ASSERT_EQUAL(bytes, 2);

    // Second frame
    stream_mqtt_disconnect(client);
    bytes = stream_mqtt_peek_frame(server);
    CU_ASSERT_EQUAL(bytes, 2);      // Previously received data is expected

    // Buffer shorter than expected
    bytes = stream_mqtt_read_frame(server, &type, &flags, buffer, 1);
    CU_ASSERT_EQUAL(bytes, -1);

    bytes = stream_mqtt_read_frame(server, &type, &flags, buffer, sizeof(buffer));
    CU_ASSERT_EQUAL(bytes, 2);      // First frame received

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


/**
 *  Test MQTT subscribe frame
 *
 */
void test_stream_mqtt_subscribe(void)
{
    struct stream_mqtt *client, *server;
    test_stream_mqtt_init(&client, &server);

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

    // Check resending
    for (int i=0; i<3; i++) {
        time += 33*1000;       // skip 33 seconds
        clock_update(time, time);
        stream_time(stream_mqtt_to_stream(client));     // Client should resend MSG_ID_1
    }

    stream_mqtt_peek_frame(server);     // Received MSG_ID_1, sent PUBREC
    stream_mqtt_peek_frame(client);     // Received PUBREC, sent PUBREL

    // Check resending
    for (int i=0; i<3; i++) {
        time += 33*1000;       // skip 33 seconds
        clock_update(time, time);
        stream_time(stream_mqtt_to_stream(client));     // Client should resend PUBREL
    }

    stream_mqtt_peek_frame(server);     // Received MSG_ID_2, sent PUBREC
    stream_mqtt_peek_frame(client);     // Received PUBREC, sent PUBREL
    stream_mqtt_peek_frame(server);     // Received PUBREL, sent PUBCOMP

    bytes = stream_mqtt_read_frame(server, &type, &flags, buffer, sizeof(buffer));
    CU_ASSERT_EQUAL(type, MQTT_PUBLISH);
    mqtt_parse_publish(&publish_msg, buffer, bytes, MQTT_QOS_FROM_FLAGS(flags));
    CU_ASSERT_EQUAL(publish_msg.id, TEST_MSG_ID_1);
    CU_ASSERT_NSTRING_EQUAL(publish_msg.topic, TEST_TOPIC_1, strlen(TEST_TOPIC_1));
    CU_ASSERT_NSTRING_EQUAL(publish_msg.payload, TEST_PAYLOAD_1, strlen(TEST_PAYLOAD_1));

    bytes = stream_mqtt_read_frame(server, &type, &flags, buffer, sizeof(buffer));
    CU_ASSERT_EQUAL(type, MQTT_PUBLISH);
    mqtt_parse_publish(&publish_msg, buffer, bytes, MQTT_QOS_FROM_FLAGS(flags));
    CU_ASSERT_EQUAL(publish_msg.id, TEST_MSG_ID_2);
    CU_ASSERT_NSTRING_EQUAL(publish_msg.topic, TEST_TOPIC_2, strlen(TEST_TOPIC_2));
    CU_ASSERT_NSTRING_EQUAL(publish_msg.payload, TEST_PAYLOAD_2, strlen(TEST_PAYLOAD_2));

    test_stream_mqtt_clean(client, server);
}
