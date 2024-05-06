
#include "test.h"

#include "mx/stream_ws.h"
#include "mx/socket.h"
#include "mx/timer.h"
#include "mx/rand.h"

#include <CUnit/Basic.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>



#define TEST_KEY                "abcdefghijklmnopqrstuv=="

#define TEST_PAYLOAD_1          "payload_first"
#define TEST_PAYLOAD_2          "payload_second"
#define TEST_PAYLOAD_3          "payload_third"

#define TEST_KEEP_ALIVE         110

#define TEST_EXT16LEN           128
#define TEST_EXT64LEN           65555


static void test_stream_ws_misc(void);
static void test_stream_ws_peek(void);
static void test_stream_ws_handshake(void);

static void test_stream_ws_connect(void);
static void test_stream_ws_read_write_frame(void);
static void test_stream_ws_multiple_frames(void);

static void test_stream_ws_fragments(void);
static void test_stream_ws_ping_pong(void);
static void test_stream_ws_masking(void);
static void test_stream_ws_extended_length_msg(void);



CU_ErrorCode cu_test_stream_ws()
{
    // Test logging to terminal
    CU_pSuite suite = CU_add_suite("Suite stream_ws", NULL, NULL);
    if ( !suite ) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    CU_add_test(suite, "Test stream ws miscellaneous functions",    test_stream_ws_misc);
    CU_add_test(suite, "Test stream ws peek frame",                 test_stream_ws_peek);
    CU_add_test(suite, "Test stream ws handshake",                  test_stream_ws_handshake);

    CU_add_test(suite, "Test stream ws connect",                    test_stream_ws_connect);
    CU_add_test(suite, "Test stream ws read/write frame",           test_stream_ws_read_write_frame);
    CU_add_test(suite, "Test stream ws multiple frames",            test_stream_ws_multiple_frames);

    CU_add_test(suite, "Test stream ws fragments",                  test_stream_ws_fragments);
    CU_add_test(suite, "Test stream ws ping/pong",                  test_stream_ws_ping_pong);
    CU_add_test(suite, "Test stream ws masking",                    test_stream_ws_masking);
    CU_add_test(suite, "Test stream ws extended length message",    test_stream_ws_extended_length_msg);

    return CU_get_error();
}



void test_stream_ws_init(struct stream_ws **client, struct stream_ws **server)
{
    int sockets[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) < 0) {
          perror("opening stream socket pair");
          exit(1);
    }

    socket_set_non_blocking(sockets[0], 1);
    socket_set_non_blocking(sockets[1], 1);

    *client = stream_ws_new(stream_new(sockets[0]));
    *server = stream_ws_new(stream_new(sockets[1]));
}

void test_stream_ws_clean(struct stream_ws *client, struct stream_ws *server)
{
    close(stream_ws_get_fd(client));
    close(stream_ws_get_fd(server));

    stream_ws_delete(client);
    stream_ws_delete(server);
}




/**
 *  Test stream ws misc
 *
 */
void test_stream_ws_misc(void)
{
    struct stream_ws *client, *server;
    test_stream_ws_init(&client, &server);

    // Casting stream <-> stream_ws
    struct stream *stream = stream_ws_to_stream(client);
    CU_ASSERT_PTR_EQUAL(stream, client);
    CU_ASSERT_PTR_EQUAL(client, stream_ws_from_stream(stream));

    stream_ws_set_status(client, STREAM_ST_READY);
    stream_ws_set_status(server, STREAM_ST_READY);

    ssize_t bytes;
    unsigned char buffer[1024];

    bytes = stream_ws_peek_frame(client);
    CU_ASSERT_EQUAL(bytes, -1);         // Nothing received
    bytes = stream_ws_read_frame(client, NULL, NULL, buffer, sizeof(buffer));
    CU_ASSERT_EQUAL(bytes, -1);         // Nothing received
    bytes = stream_ws_read(client, buffer, sizeof(buffer));
    CU_ASSERT_EQUAL(bytes, -1);         // Nothing received

    // Flush
    stream_flush(stream_ws_to_stream(client));

    // Connect with defined key
    stream_ws_connect(client, "/", TEST_KEY, NULL);
    stream_ws_peek_frame(server);
    // Close stream without reading data
    stream_ws_write(server, TEST_PAYLOAD_1, strlen(TEST_PAYLOAD_1));
    stream_ws_peek_frame(client);

    test_stream_ws_clean(client, server);
}


/**
 *  Test stream ws peek frame
 *
 */
void test_stream_ws_peek(void)
{
    struct stream_ws *client, *server;
    test_stream_ws_init(&client, &server);

    stream_ws_set_status(client, STREAM_ST_READY);
    stream_ws_set_status(server, STREAM_ST_READY);

    ssize_t bytes;
    unsigned char msg[] = {0x81, 0x04, 0x76, 0x65, 0x73, 0x74};
    unsigned char msg_extlen[] = {0x81, 0x7E, 0x00};

    write(stream_ws_get_fd(client), msg, 1);
    bytes = stream_ws_peek_frame(server);
    CU_ASSERT_EQUAL(bytes, -1);     // More bytes needed

    write(stream_ws_get_fd(client), msg+1, 1);
    bytes = stream_ws_peek_frame(server);
    CU_ASSERT_EQUAL(bytes, -1);     // More bytes needed

    write(stream_ws_get_fd(client), msg+2, 1);
    bytes = stream_ws_peek_frame(server);
    CU_ASSERT_EQUAL(bytes, -1);     // More bytes needed

    write(stream_ws_get_fd(client), msg+3, 3);
    bytes = stream_ws_peek_frame(server);
    CU_ASSERT_EQUAL(bytes, 4);     // Message complete
    stream_ws_read_frame(server, NULL, NULL, msg, sizeof(msg));

    write(stream_ws_get_fd(client), msg_extlen, sizeof(msg_extlen));
    bytes = stream_ws_peek_frame(server);
    CU_ASSERT_EQUAL(bytes, -1);     // More bytes needed to decode extlen

    test_stream_ws_clean(client, server);
}


/**
 *  Test stream ws handshake
 *
 */
void test_stream_ws_handshake(void)
{
    struct stream_ws *client, *server;
    test_stream_ws_init(&client, &server);

    ssize_t bytes;
    char msg_empty[] = "\r\n\r\n";
    char msg_invalid_key[] = "Sec-WebSocket-Accept: abcdefghijklmnopqrstuv==\r\n\r\n";

    // No websocket key
    write(stream_ws_get_fd(client), msg_empty, strlen(msg_empty));
    bytes = stream_ws_peek_frame(server);
    CU_ASSERT_EQUAL(bytes, 0);     // Disconnect

    stream_ws_connect(client, "/", NULL, NULL);
    // No websocket key
    write(stream_ws_get_fd(server), msg_empty, strlen(msg_empty));
    bytes = stream_ws_peek_frame(client);
    CU_ASSERT_EQUAL(bytes, 0);     // Disconnect

    write(stream_ws_get_fd(server), msg_invalid_key, strlen(msg_invalid_key));
    bytes = stream_ws_peek_frame(client);
    CU_ASSERT_EQUAL(bytes, 0);     // Disconnect

    test_stream_ws_clean(client, server);
}


/**
 *  Test stream ws connect
 *
 */
void test_stream_ws_connect(void)
{
    struct stream_ws *client, *server;
    test_stream_ws_init(&client, &server);

    int status;
    ssize_t bytes;
    unsigned char buffer[1024];

    stream_ws_connect(client, "/", NULL, NULL);

    bytes = stream_ws_peek_frame(server);
    CU_ASSERT_EQUAL(bytes, -1);         // Just connected
    status = stream_ws_get_status(server);
    CU_ASSERT_EQUAL(status, STREAM_ST_READY);
    bytes = stream_ws_peek_frame(client);
    CU_ASSERT_EQUAL(bytes, -1);         // Just connected
    status = stream_ws_get_status(client);
    CU_ASSERT_EQUAL(status, STREAM_ST_READY);

    bytes = stream_ws_write(client, TEST_PAYLOAD_1, strlen(TEST_PAYLOAD_1));
    CU_ASSERT_TRUE(bytes > 0);
    bytes = stream_ws_read(server, buffer, sizeof(buffer));
    CU_ASSERT_TRUE(bytes > 0);
    CU_ASSERT_NSTRING_EQUAL(TEST_PAYLOAD_1, buffer, bytes);

    bytes = stream_ws_write(server, TEST_PAYLOAD_2, strlen(TEST_PAYLOAD_2));
    CU_ASSERT_TRUE(bytes > 0);
    bytes = stream_ws_read(client, buffer, sizeof(buffer));
    CU_ASSERT_TRUE(bytes > 0);
    CU_ASSERT_NSTRING_EQUAL(TEST_PAYLOAD_2, buffer, bytes);

    stream_ws_disconnect(client, WS_CLOSE_NORMAL);
    stream_ws_peek_frame(server);

    test_stream_ws_clean(client, server);
}


/**
 *  Test stream ws read and write frame
 *
 */
void test_stream_ws_read_write_frame(void)
{
    struct stream_ws *client, *server;
    test_stream_ws_init(&client, &server);

    stream_ws_set_status(client, STREAM_ST_READY);
    stream_ws_set_status(server, STREAM_ST_READY);

    ssize_t bytes;
    unsigned char buffer[256];
    unsigned char fin;
    unsigned char opcode;

    bytes = stream_ws_write_frame(client, WS_FIN_FLAG, WS_OPCODE_BINARY, NULL, TEST_PAYLOAD_1, strlen(TEST_PAYLOAD_1));
    CU_ASSERT_TRUE(bytes > 0);

    bytes = stream_ws_peek_frame(server);
    CU_ASSERT_TRUE(bytes > 0);
    // Buffer too small
    bytes = stream_ws_read_frame(server, &fin, &opcode, buffer, 1);
    CU_ASSERT_TRUE(bytes < 0);
    // Buffer long enough
    bytes = stream_ws_read_frame(server, &fin, &opcode, buffer, sizeof(buffer));
    CU_ASSERT_TRUE(bytes > 0);
    CU_ASSERT_NOT_EQUAL(fin, 0);
    CU_ASSERT_EQUAL(opcode, WS_OPCODE_BINARY);
    CU_ASSERT_NSTRING_EQUAL(buffer, TEST_PAYLOAD_1, bytes);

    test_stream_ws_clean(client, server);
}


/**
 *  Test stream ws multiple frames
 *
 */
void test_stream_ws_multiple_frames(void)
{
    struct stream_ws *client, *server;
    test_stream_ws_init(&client, &server);

    stream_ws_set_status(client, STREAM_ST_READY);
    stream_ws_set_status(server, STREAM_ST_READY);

    ssize_t bytes;
    unsigned char buffer[256];
    unsigned char fin;
    unsigned char opcode;

    bytes = stream_ws_write_frame(client, WS_FIN_FLAG, WS_OPCODE_BINARY, NULL, TEST_PAYLOAD_1, strlen(TEST_PAYLOAD_1));
    CU_ASSERT_TRUE(bytes > 0);
    bytes = stream_ws_write_frame(client, WS_FIN_FLAG, WS_OPCODE_BINARY, NULL, TEST_PAYLOAD_2, strlen(TEST_PAYLOAD_2));
    CU_ASSERT_TRUE(bytes > 0);

    bytes = stream_ws_peek_frame(server);
    CU_ASSERT_TRUE(bytes > 0);
    bytes = stream_ws_read_frame(server, &fin, &opcode, buffer, sizeof(buffer));
    CU_ASSERT_TRUE(bytes > 0);
    CU_ASSERT_NOT_EQUAL(fin, 0);
    CU_ASSERT_EQUAL(opcode, WS_OPCODE_BINARY);
    CU_ASSERT_NSTRING_EQUAL(buffer, TEST_PAYLOAD_1, bytes);
    bytes = stream_ws_peek_frame(server);
    CU_ASSERT_TRUE(bytes > 0);
    bytes = stream_ws_read_frame(server, &fin, &opcode, buffer, sizeof(buffer));
    CU_ASSERT_TRUE(bytes > 0);
    CU_ASSERT_NOT_EQUAL(fin, 0);
    CU_ASSERT_EQUAL(opcode, WS_OPCODE_BINARY);
    CU_ASSERT_NSTRING_EQUAL(buffer, TEST_PAYLOAD_2, bytes);

    test_stream_ws_clean(client, server);
}


/**
 *  Test stream ws fragments
 *
 */
void test_stream_ws_fragments(void)
{
    struct stream_ws *client, *server;
    test_stream_ws_init(&client, &server);

    stream_ws_set_status(client, STREAM_ST_READY);
    stream_ws_set_status(server, STREAM_ST_READY);

    ssize_t bytes;
    unsigned char buffer[256];
    unsigned char fin;
    unsigned char opcode;

    bytes = stream_ws_write_frame(client, 0, WS_OPCODE_TEXT, NULL, "first", strlen("first"));
    CU_ASSERT_TRUE(bytes > 0);
    bytes = stream_ws_peek_frame(server);
    CU_ASSERT_TRUE(bytes < 0);      // Frame not complete
    bytes = stream_ws_read_frame(server, &fin, &opcode, buffer, sizeof(buffer));
    CU_ASSERT_TRUE(bytes < 0);      // Frame not complete

    bytes = stream_ws_write_frame(client, 0, WS_OPCODE_CONTINUE, NULL, "_", strlen("_"));
    CU_ASSERT_TRUE(bytes > 0);
    bytes = stream_ws_peek_frame(server);
    CU_ASSERT_TRUE(bytes < 0);      // Frame not complete

    bytes = stream_ws_write_frame(client, WS_FIN_FLAG, WS_OPCODE_CONTINUE, NULL, "second", strlen("second"));
    CU_ASSERT_TRUE(bytes > 0);
    bytes = stream_ws_peek_frame(server);
    CU_ASSERT_TRUE(bytes > 0);      // Frame not complete

    bytes = stream_ws_read_frame(server, &fin, &opcode, buffer, sizeof(buffer));
    CU_ASSERT_TRUE(bytes > 0);
    CU_ASSERT_NOT_EQUAL(fin, 0);
    CU_ASSERT_EQUAL(opcode, WS_OPCODE_TEXT);
    CU_ASSERT_NSTRING_EQUAL(buffer, "first_second", bytes);

    test_stream_ws_clean(client, server);
}


/**
 *  Test stream ws ping pong
 *
 */
void test_stream_ws_ping_pong(void)
{
    struct stream_ws *client, *server;
    test_stream_ws_init(&client, &server);

    unsigned int time = 100*1000;
    clock_update(time, time);

    int status;

    stream_ws_connect(client, "/", NULL, NULL);

    stream_ws_peek_frame(server);
    stream_ws_peek_frame(client);

    // Mock keep-alive messages
    for (int i=0; i<5; i++) {
        time += TEST_KEEP_ALIVE*1000;   // skip TEST_KEEP_ALIVE interval
        clock_update(time, time);
        stream_time(stream_ws_to_stream(client));
        stream_ws_peek_frame(server);
        stream_ws_peek_frame(client);
        status = stream_ws_get_status(client);
        CU_ASSERT_EQUAL(status, STREAM_ST_READY);
        status = stream_ws_get_status(server);
        CU_ASSERT_EQUAL(status, STREAM_ST_READY);
    }

    // Mock keep-alive timeout
    time += 5*TEST_KEEP_ALIVE*1000;     // skip 5*TEST_KEEP_ALIVE interval
    clock_update(time, time);
    stream_time(stream_ws_to_stream(server));
    status = stream_ws_get_status(server);
    CU_ASSERT_EQUAL(status, STREAM_ST_CLOSING);
    // From the client point of view new ping request should be sent
    stream_time(stream_ws_to_stream(client));
    status = stream_ws_get_status(client);
    CU_ASSERT_EQUAL(status, STREAM_ST_READY);

    // Mock missing ping response
    time += TEST_KEEP_ALIVE*1000;       // skip TEST_KEEP_ALIVE interval
    clock_update(time, time);
    stream_time(stream_ws_to_stream(client));
    status = stream_ws_get_status(client);
    CU_ASSERT_EQUAL(status, STREAM_ST_CLOSING);

    test_stream_ws_clean(client, server);
}


/**
 *  Test stream ws masking
 *
 */
void test_stream_ws_masking(void)
{
    struct stream_ws *client, *server;
    test_stream_ws_init(&client, &server);

    unsigned int time = 100*1000;
    clock_update(time, time);

    int status;

    stream_ws_connect(client, "/", NULL, NULL);

    stream_ws_peek_frame(server);
    stream_ws_peek_frame(client);

    stream_ws_write_frame(server, WS_FIN_FLAG, WS_OPCODE_TEXT, (const unsigned char*)"1234", TEST_PAYLOAD_1, strlen(TEST_PAYLOAD_1));
    // Server should not send masked requests
    stream_ws_peek_frame(client);
    status = stream_ws_get_status(client);
    CU_ASSERT_EQUAL(status, STREAM_ST_CLOSING);

    test_stream_ws_clean(client, server);
}


/**
 *  Test stream ws extended length message
 *
 */
void test_stream_ws_extended_length_msg(void)
{
    struct stream_ws *client, *server;
    test_stream_ws_init(&client, &server);

    unsigned int time = 100*1000;
    clock_update(time, time);

    stream_ws_connect(client, "/", NULL, NULL);

    stream_ws_peek_frame(server);
    stream_ws_peek_frame(client);

    ssize_t bytes;
    unsigned char request[TEST_EXT64LEN];
    unsigned char response[TEST_EXT64LEN];

    rand_data(request, sizeof(request));

    // Test EXTLEN16
    stream_ws_write_frame(client, WS_FIN_FLAG, WS_OPCODE_TEXT, (const unsigned char*)"test", request, TEST_EXT16LEN);
    bytes = stream_ws_peek_frame(server);
    CU_ASSERT_TRUE(bytes > 0);
    bytes = stream_ws_read_frame(server, NULL, NULL, response, sizeof(response));
    CU_ASSERT_TRUE(bytes > 0);
    CU_ASSERT_EQUAL(0, memcmp(request, response, TEST_EXT16LEN));

    // Test EXTLEN64
    stream_ws_write_frame(client, WS_FIN_FLAG, WS_OPCODE_TEXT, (const unsigned char*)"test", request, TEST_EXT64LEN);
    bytes = stream_ws_peek_frame(server);
    CU_ASSERT_TRUE(bytes > 0);
    bytes = stream_ws_read_frame(server, NULL, NULL, response, sizeof(response));
    CU_ASSERT_TRUE(bytes > 0);
    CU_ASSERT_EQUAL(0, memcmp(request, response, TEST_EXT64LEN));

    test_stream_ws_clean(client, server);
}
