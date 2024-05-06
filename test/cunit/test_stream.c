
#include "test.h"

#include "mx/stream.h"
#include "mx/observer.h"
#include "mx/socket.h"
#include "mx/idler.h"
#include "mx/rand.h"

#include <CUnit/Basic.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>


// At the time of writing it was possible to write 219264 bytes directly
#define SOCKET_BUFFER_SIZE      219264
#define BUFFER_SIZE             500000



static void test_stream_misc(void);

static void test_stream_idler(void);
static void test_stream_queuing(void);
static void test_stream_observer(void);



CU_ErrorCode cu_test_stream()
{
    // Test logging to terminal
    CU_pSuite suite = CU_add_suite("Suite stream", NULL, NULL);
    if ( !suite ) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    CU_add_test(suite, "Test stream ws miscellaneous functions",    test_stream_misc);
    CU_add_test(suite, "Test stream queuing",                       test_stream_queuing);
    CU_add_test(suite, "Test stream with idler",                    test_stream_idler);
    CU_add_test(suite, "Test stream observer",                      test_stream_observer);

    return CU_get_error();
}



void test_stream_init(struct stream **client, struct stream **server)
{
    int sockets[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) < 0) {
          perror("opening stream socket pair");
          exit(1);
    }

    socket_set_non_blocking(sockets[0], 1);
    socket_set_non_blocking(sockets[1], 1);

    *client = stream_new(sockets[0]);
    *server = stream_new(sockets[1]);
}

void test_stream_clean(struct stream *client, struct stream *server)
{
    close(stream_get_fd(client));
    close(stream_get_fd(server));

    stream_delete(client);
    stream_delete(server);
}


int test_stream_on_ready(void *observer, struct stream *stream)
{
    char *buffer = (char*)observer;

    ssize_t bytes = stream_read(stream, buffer, BUFFER_SIZE);

    return bytes;
}





/**
 *  Test stream misc
 *
 */
void test_stream_misc(void)
{
    struct stream *client, *server;
    test_stream_init(&client, &server);


    test_stream_clean(client, server);
}


/**
 *  Test stream queuing
 *
 */
void test_stream_queuing(void)
{
    ssize_t bytes;
    size_t offset;
    bool status;
    unsigned char request[BUFFER_SIZE];
    unsigned char response[BUFFER_SIZE];

    rand_data(request, sizeof(request));

    struct stream *client, *server;
    test_stream_init(&client, &server);

    // New stream has no outgoing data
    status = stream_has_outgoing_data(client);
    CU_ASSERT_FALSE(status);
    // Write data, fill socket buffer
    offset = 0;
    stream_write(client, request+offset, SOCKET_BUFFER_SIZE);
    // No more data can be written to the socket, queue data
    offset += SOCKET_BUFFER_SIZE;
    stream_write(client, request+offset, SOCKET_BUFFER_SIZE);
    // Extend queued data
    offset += SOCKET_BUFFER_SIZE;
    stream_write(client, request+offset, sizeof(request)-offset);
    //
    status = stream_has_outgoing_data(client);
    CU_ASSERT_TRUE(status);

    // Receive
    bytes = stream_read(server, response, sizeof(response));
    CU_ASSERT(bytes > 0);
    offset = bytes;
    CU_ASSERT(offset < sizeof(response));
    //
    stream_handle_outgoing_data(client);
    bytes = stream_read(server, response+offset, sizeof(response)-offset);
    CU_ASSERT(bytes > 0);
    offset += bytes;
    CU_ASSERT(offset < sizeof(response));
    //
    stream_flush(client);
    bytes = stream_read(server, response+offset, sizeof(response)-offset);
    CU_ASSERT(bytes > 0);
    offset += bytes;
    CU_ASSERT(offset == sizeof(response));

    CU_ASSERT_EQUAL(0, memcmp(request, response, sizeof(request)));

    test_stream_clean(client, server);
}


/**
 *  Test stream idler
 *
 */
void test_stream_idler(void)
{
    unsigned char request[BUFFER_SIZE];
    unsigned char response[BUFFER_SIZE];
    rand_data(request, sizeof(request));

    struct stream *client, *server, *tmp;
    test_stream_init(&client, &server);

    struct idler *idler = idler_new();
    CU_ASSERT_PTR_NOT_NULL(idler);
    idler_add_stream(idler, client);
    idler_add_stream(idler, server);

    int op;
    op = idler_wait(idler, 100);
    CU_ASSERT_EQUAL(op, IDLER_TIMEOUT);

    // Nothing to do
    unsigned int stream_status;
    tmp = idler_get_next_stream(idler, NULL, &stream_status);
    CU_ASSERT_PTR_NULL(tmp);
    stream_status = idler_get_stream_status(idler, client);
    CU_ASSERT_EQUAL(stream_status, 0);

    // Write data and expect that some data will be queued
    stream_write(client, request, sizeof(request));

    // Client is blocked, server is ready for reading
    op = idler_wait(idler, 100);
    CU_ASSERT_EQUAL(op, IDLER_OPERATION);
    //
    stream_status = idler_get_stream_status(idler, client);
    CU_ASSERT_FALSE(stream_status);
    stream_status = idler_get_stream_status(idler, server);
    CU_ASSERT_TRUE(stream_status & STREAM_INCOMING_READY);
    //
    tmp = idler_get_next_stream(idler, NULL, &stream_status);
    CU_ASSERT_PTR_NOT_NULL(tmp);
    CU_ASSERT_PTR_EQUAL(tmp, server);
    CU_ASSERT_TRUE(stream_status & STREAM_INCOMING_READY);
    //
    tmp = idler_get_next_stream(idler, tmp, &stream_status);
    CU_ASSERT_PTR_NULL(tmp);

    // Receive
    stream_read(server, response, sizeof(response));

    // Client is ready for writting, server has nothing to do
    op = idler_wait(idler, 100);
    CU_ASSERT_EQUAL(op, IDLER_OPERATION);
    //
    stream_status = idler_get_stream_status(idler, client);
    CU_ASSERT_TRUE(stream_status & STREAM_OUTGOING_READY);
    stream_status = idler_get_stream_status(idler, server);
    CU_ASSERT_FALSE(stream_status);
    //
    tmp = idler_get_next_stream(idler, NULL, &stream_status);
    CU_ASSERT_PTR_NOT_NULL(tmp);
    CU_ASSERT_PTR_EQUAL(tmp, client);
    CU_ASSERT_TRUE(stream_status & STREAM_OUTGOING_READY);
    //
    tmp = idler_get_next_stream(idler, tmp, &stream_status);
    CU_ASSERT_PTR_NULL(tmp);

    // Remove stream
    tmp = idler_find_stream(idler, stream_get_fd(client));
    CU_ASSERT_PTR_NOT_NULL(tmp);
    idler_remove_stream(idler, tmp);
    tmp = idler_find_stream(idler, stream_get_fd(client));
    CU_ASSERT_PTR_NULL(tmp);
    // The other stream will be removed on idler delete

    // Delete idler
    idler = idler_delete(idler);
    CU_ASSERT_PTR_NULL(idler);

    test_stream_clean(client, server);
}


/**
 *  Test stream observer
 *
 */
void test_stream_observer(void)
{
    unsigned char buffer[BUFFER_SIZE];

    const char *hello = "hello";
    const size_t hello_len = strlen(hello)+1;

    struct stream *client, *server;
    test_stream_init(&client, &server);

    stream_set_observer(server, buffer, test_stream_on_ready);

    stream_write(client, hello, hello_len);

    ssize_t bytes = stream_handle_incoming_data(server);
    CU_ASSERT_EQUAL(bytes, hello_len);
    CU_ASSERT_STRING_EQUAL(hello, buffer);

    test_stream_clean(client, server);
}
