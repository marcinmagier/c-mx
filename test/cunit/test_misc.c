
#include "test.h"

#include "mx/base64.h"
#include "mx/buffer.h"
#include "mx/memory.h"
#include "mx/observer.h"
#include "mx/rand.h"
#include "mx/string.h"
#include "mx/url.h"

#include <CUnit/Basic.h>

#include <unistd.h>



static void test_base64(void);
static void test_buffer(void);
static void test_memory(void);
static void test_observer(void);
static void test_rand(void);

static void test_url(void);



CU_ErrorCode cu_test_misc()
{
    // Test logging to terminal
    CU_pSuite suite = CU_add_suite("Suite miscellaneous", NULL, NULL);
    if ( !suite ) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    CU_add_test(suite, "Test base64 encoding/decoding",             test_base64);
    CU_add_test(suite, "Test buffer",                               test_buffer);
    CU_add_test(suite, "Test memory allocation",                    test_memory);
    CU_add_test(suite, "Test observer",                             test_observer);
    CU_add_test(suite, "Test random",                               test_rand);

    CU_add_test(suite, "Test url parser",                           test_url);

    return CU_get_error();
}



/**
 *  Test base64
 *
 */
void test_base64(void)
{
    const char *encoded;
    const char *decoded;
    unsigned char buffer[256];
    size_t bytes;

    decoded = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    encoded = "QUJDREVGR0hJSktMTU5PUFFSU1RVVldYWVo=";
    bytes = base64_encode((unsigned char*)decoded, strlen(decoded), buffer, sizeof(buffer), false);
    CU_ASSERT_EQUAL(bytes, strlen(encoded));
    CU_ASSERT_STRING_EQUAL(buffer, encoded);
    bytes = base64_decode((unsigned char*)encoded, strlen(encoded), buffer, sizeof(buffer));
    CU_ASSERT_EQUAL(bytes, strlen(decoded));
    CU_ASSERT_STRING_EQUAL(buffer, decoded);


    decoded = "ABCDEFGHIJKLMNOPQRSTUVWXYZ abcdefghijklmnopqrstuvwxyz";
    encoded = "QUJDREVGR0hJSktMTU5PUFFSU1RVVldYWVogYWJjZGVmZ2hpamtsbW5vcHFyc3R1dnd4eXo=";
    bytes = base64_encode((unsigned char*)decoded, strlen(decoded), buffer, sizeof(buffer), false);
    CU_ASSERT_EQUAL(bytes, strlen(encoded));
    CU_ASSERT_STRING_EQUAL(buffer, encoded);
    bytes = base64_decode((unsigned char*)encoded, strlen(encoded), buffer, sizeof(buffer));
    CU_ASSERT_EQUAL(bytes, strlen(decoded));
    CU_ASSERT_STRING_EQUAL(buffer, decoded);


    decoded = "ABCDEFGHIJKLMNOPQRSTUVWXYZ abcdefghijklmnopqrstuvwxyz 1234567890";
    encoded = "QUJDREVGR0hJSktMTU5PUFFSU1RVVldYWVogYWJjZGVmZ2hpamtsbW5vcHFyc3R1dnd4eXog\nMTIzNDU2Nzg5MA==\n";
    bytes = base64_encode((unsigned char*)decoded, strlen(decoded), buffer, sizeof(buffer), true);
    CU_ASSERT_EQUAL(bytes, strlen(encoded));
    CU_ASSERT_STRING_EQUAL(buffer, encoded);
    bytes = base64_decode((unsigned char*)encoded, strlen(encoded), buffer, sizeof(buffer));
    CU_ASSERT_EQUAL(bytes, strlen(decoded));
    CU_ASSERT_STRING_EQUAL(buffer, decoded);

    // Errors
    bytes = base64_encode((unsigned char*)decoded, strlen(decoded), buffer, 1, false);
    CU_ASSERT_EQUAL(bytes, 0);
    bytes = base64_decode((unsigned char*)encoded, strlen(encoded), buffer, 1);
    CU_ASSERT_EQUAL(bytes, 0);
}


/**
 *  Test buffer
 *
 */
void test_buffer(void)
{
    struct buffer *buffer = buffer_create(8);
    CU_ASSERT_PTR_NOT_NULL(buffer);

    const char *hello = "hello";
    size_t hello_len = strlen(hello);
    buffer_append(buffer, hello, hello_len);
    CU_ASSERT_EQUAL(hello_len, buffer->length);
    CU_ASSERT_NSTRING_EQUAL(hello, buffer->data, buffer->length);

    const char *world = "world";
    size_t world_len = strlen(world);
    buffer_append(buffer, world, world_len);
    CU_ASSERT_EQUAL(hello_len+world_len, buffer->length);
    CU_ASSERT_NSTRING_EQUAL("helloworld", buffer->data, hello_len+world_len);

    // Copy buffer
    struct buffer *buffcopy = buffer_new();
    buffer_copy(buffcopy, buffer);
    CU_ASSERT_PTR_NOT_EQUAL(buffcopy->data, buffer->data);
    CU_ASSERT_NSTRING_EQUAL(buffcopy->data, buffer->data, buffer->length);
    buffer_delete(buffcopy);

    char chunk[64];
    size_t bytes;
    bytes = buffer_take(buffer, chunk, hello_len);
    CU_ASSERT_EQUAL(bytes, hello_len);
    chunk[bytes] = '\0';
    CU_ASSERT_STRING_EQUAL(chunk, hello);
    CU_ASSERT_EQUAL(world_len, buffer->length);
    CU_ASSERT_NSTRING_EQUAL(world, buffer->data, buffer->length);

    buffer_append(buffer, hello, hello_len);
    CU_ASSERT_EQUAL(world_len+hello_len, buffer->length);
    CU_ASSERT_NSTRING_EQUAL("worldhello", buffer->data, buffer->length);

    buffer = buffer_delete(buffer);
    CU_ASSERT_PTR_NULL(buffer);
}


/**
 *  Test memory
 *
 */
void test_memory(void)
{
    unsigned char *buffer;
    unsigned char *tmp;

    buffer = xcalloc(5, 4);
    CU_ASSERT_PTR_NOT_NULL(buffer);
    rand_data(buffer, 20);
    tmp = xmemdup(buffer, 20);
    CU_ASSERT_PTR_NOT_NULL(tmp);
    CU_ASSERT_EQUAL(0, memcmp(buffer, tmp, 20));
    buffer = xfree(buffer);
    CU_ASSERT_PTR_NULL(buffer);
    tmp = xfree(tmp);
    CU_ASSERT_PTR_NULL(tmp);

    buffer = xmallocz(20);
    CU_ASSERT_PTR_NOT_NULL(buffer);
    rand_data(buffer, 20);
    tmp = xmemdupz(buffer, 20);
    CU_ASSERT_PTR_NOT_NULL(tmp);
    CU_ASSERT_EQUAL(0, memcmp(buffer, tmp, 20));
    buffer = xfree(buffer);
    CU_ASSERT_PTR_NULL(buffer);
    tmp = xfree(tmp);
    CU_ASSERT_PTR_NULL(tmp);
}


/**
 *  Test observer
 *
 */
void test_observer(void)
{
    struct observer *first, *second;
    struct observer_list *observers;

    observers = observer_list_new();
    CU_ASSERT_PTR_NOT_NULL(observers);

    first = observer_new();
    CU_ASSERT_PTR_NOT_NULL(first);
    first->notify_handler = (observer_notify_fn)1000;
    first->notify_type = 1;
    second = observer_new();
    CU_ASSERT_PTR_NOT_NULL(second);
    second->notify_handler = (observer_notify_fn)2000;
    second->notify_type = 2;

    observer_list_add_observer(observers, first);
    observer_list_add_observer(observers, second);

    struct observer *tmp;
    tmp = observer_list_find_observer(observers, NULL, 2, (observer_notify_fn)3000);
    CU_ASSERT_PTR_NULL(tmp);
    tmp = observer_list_find_observer(observers, NULL, 3, (observer_notify_fn)2000);
    CU_ASSERT_PTR_NULL(tmp);
    tmp = observer_list_find_observer(observers, (void*)3, 0, NULL);
    CU_ASSERT_PTR_NULL(tmp);
    tmp = observer_list_find_observer(observers, NULL, 2, (observer_notify_fn)2000);
    CU_ASSERT_PTR_NOT_NULL(tmp);
    CU_ASSERT_PTR_EQUAL(tmp, second);

    // First observer will be remove on observer_list_delete()
    observer_list_remove_observer(observers, second);
    second = observer_delete(second);
    CU_ASSERT_PTR_NULL(second);

    observers = observer_list_delete(observers);
    CU_ASSERT_PTR_NULL(observers);
}


/**
 *  Test random
 *
 */
void test_rand(void)
{
    // Test entropy, adding entrypy requires root priviledges
    int fd = rand_dev_open(RAND_DEV_URANDOM);
    CU_ASSERT(fd > 0);
    rand_dev_add_entropy(fd, "hello", 5, 50);
    rand_dev_close(fd);

    unsigned int value;
    value = 1;
    rand_time_increase(&value, 10);
    CU_ASSERT_EQUAL(value, 2);
    rand_time_increase(&value, 10);
    CU_ASSERT_EQUAL(value, 4);
    rand_time_increase(&value, 10);
    CU_ASSERT((value == 7) | (value == 8));
    rand_time_increase(&value, 10);
    CU_ASSERT_EQUAL(value, 10);
    rand_time_increase(&value, 10);
    CU_ASSERT_EQUAL(value, 10);

    rand_time_decrease(&value);
    CU_ASSERT_EQUAL(value, 5);
}


/**
 *  Test url
 *
 */
void test_url(void)
{
    bool status;
    struct url_parser parser;

    memset(&parser, 0, sizeof(parser));
    status = url_parse(&parser, "");
    CU_ASSERT_TRUE(status);
    CU_ASSERT_PTR_NULL(parser.scheme);
    CU_ASSERT_PTR_NULL(parser.userinfo);
    CU_ASSERT_PTR_NULL(parser.port);
    CU_ASSERT_PTR_NULL(parser.path);
    CU_ASSERT_PTR_NULL(parser.query);
    //
    CU_ASSERT_EQUAL(parser.host_len, strlen(""));
    CU_ASSERT_NSTRING_EQUAL(parser.host, "", parser.host_len);


    memset(&parser, 0, sizeof(parser));
    status = url_parse(&parser, "host:1000");
    CU_ASSERT_TRUE(status);
    CU_ASSERT_PTR_NULL(parser.scheme);
    CU_ASSERT_PTR_NULL(parser.userinfo);
    CU_ASSERT_PTR_NULL(parser.path);
    CU_ASSERT_PTR_NULL(parser.query);
    //
    CU_ASSERT_EQUAL(parser.host_len, strlen("host"));
    CU_ASSERT_NSTRING_EQUAL(parser.host, "host", parser.host_len);
    CU_ASSERT_EQUAL(parser.port_len, strlen("1000"));
    CU_ASSERT_NSTRING_EQUAL(parser.port, "1000", parser.port_len);


    memset(&parser, 0, sizeof(parser));
    status = url_parse(&parser, "scheme://host:port/a/b/c");
    CU_ASSERT_TRUE(status);
    CU_ASSERT_PTR_NULL(parser.userinfo);
    CU_ASSERT_PTR_NULL(parser.query);
    //
    CU_ASSERT_EQUAL(parser.scheme_len, strlen("scheme"));
    CU_ASSERT_NSTRING_EQUAL(parser.scheme, "scheme", parser.scheme_len);
    CU_ASSERT_EQUAL(parser.host_len, strlen("host"));
    CU_ASSERT_NSTRING_EQUAL(parser.host, "host", parser.host_len);
    CU_ASSERT_EQUAL(parser.port_len, strlen("port"));
    CU_ASSERT_NSTRING_EQUAL(parser.port, "port", parser.port_len);
    CU_ASSERT_EQUAL(parser.path_len, strlen("/a/b/c"));
    CU_ASSERT_NSTRING_EQUAL(parser.path, "/a/b/c", parser.path_len);


    memset(&parser, 0, sizeof(parser));
    status = url_parse(&parser, "user@host/a/b/c?query");
    CU_ASSERT_TRUE(status);
    CU_ASSERT_PTR_NULL(parser.scheme);
    CU_ASSERT_PTR_NULL(parser.port);
    //
    CU_ASSERT_EQUAL(parser.userinfo_len, strlen("user"));
    CU_ASSERT_NSTRING_EQUAL(parser.userinfo, "user", parser.userinfo_len);
    CU_ASSERT_EQUAL(parser.host_len, strlen("host"));
    CU_ASSERT_NSTRING_EQUAL(parser.host, "host", parser.host_len);
    CU_ASSERT_EQUAL(parser.path_len, strlen("/a/b/c"));
    CU_ASSERT_NSTRING_EQUAL(parser.path, "/a/b/c", parser.path_len);
    CU_ASSERT_EQUAL(parser.query_len, strlen("query"));
    CU_ASSERT_NSTRING_EQUAL(parser.query, "query", parser.query_len);


    memset(&parser, 0, sizeof(parser));
    status = url_parse(&parser, "/a/b/c");
    CU_ASSERT_TRUE(status);
    CU_ASSERT_PTR_NULL(parser.scheme);
    CU_ASSERT_PTR_NULL(parser.userinfo);
    CU_ASSERT_PTR_NULL(parser.host);
    CU_ASSERT_PTR_NULL(parser.port);
    CU_ASSERT_PTR_NULL(parser.query);
    //
    CU_ASSERT_EQUAL(parser.path_len, strlen("/a/b/c"));
    CU_ASSERT_NSTRING_EQUAL(parser.path, "/a/b/c", parser.path_len);


    unsigned int port;
    bool encrypted;

    status = url_parse_scheme("ws", &port, &encrypted);
    CU_ASSERT_TRUE(status);
    CU_ASSERT_FALSE(encrypted);
    CU_ASSERT_EQUAL(port, 80);
    status = url_parse_scheme("wss", &port, &encrypted);
    CU_ASSERT_TRUE(status);
    CU_ASSERT_TRUE(encrypted);
    CU_ASSERT_EQUAL(port, 443);

    status = url_parse_scheme("unknown", &port, &encrypted);
    CU_ASSERT_FALSE(status);
}
