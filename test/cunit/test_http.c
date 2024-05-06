
#include "test.h"

#include "mx/string.h"
#include "mx/memory.h"
#include "mx/http.h"

#include <CUnit/Basic.h>

#include <string.h>



static void test_http_request(void);
static void test_http_response(void);
static void test_http_headers(void);



CU_ErrorCode cu_test_http()
{
    // Test logging to terminal
    CU_pSuite suite = CU_add_suite("Suite http", NULL, NULL);
    if ( !suite ) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    CU_add_test(suite, "Test http request",                         test_http_request);
    CU_add_test(suite, "Test http response",                        test_http_response);
    CU_add_test(suite, "Test http headers",                         test_http_headers);

    return CU_get_error();
}





/**
 *  Test http request
 *
 */
void test_http_request(void)
{
    int ret;
    bool success;
    struct http_msg_view view;

    char buffer[256];
    memset(buffer, 0, sizeof(buffer));

    struct http_msg *msg = http_msg_new_request("POST", "/test/http", "HTTP/1.1");
    CU_ASSERT_PTR_NOT_NULL(msg);

    // no headers, no body
    ret = http_msg_serialize(msg, buffer, sizeof(buffer));
    CU_ASSERT(ret > 0);
    buffer[ret] = '\0';
    LOG("%s\n", buffer);
    success = http_msg_view_parse_request(&view, buffer);
    CU_ASSERT_TRUE(success);
    CU_ASSERT_NSTRING_EQUAL(view.method, msg->method, strlen(msg->method));
    CU_ASSERT_NSTRING_EQUAL(view.uri, msg->uri, strlen(msg->uri));
    CU_ASSERT_NSTRING_EQUAL(view.version, msg->version, strlen(msg->version));

    // no headers, body
    http_msg_append_body(msg, "hello", strlen("hello"));
    ret = http_msg_serialize(msg, buffer, sizeof(buffer));
    CU_ASSERT(ret > 0);
    buffer[ret] = '\0';
    LOG("%s\n", buffer);
    success = http_msg_view_parse_request(&view, buffer);
    CU_ASSERT_TRUE(success);
    CU_ASSERT_NSTRING_EQUAL(view.body, msg->body->data, msg->body->length);
    CU_ASSERT_NSTRING_EQUAL(view.version, msg->version, strlen(msg->version));

    // headers, body
    http_msg_add_header(msg, "first", "test1");
    http_msg_add_header(msg, "second", NULL);
    http_msg_add_header(msg, "third", "test3");
    http_msg_append_body(msg, "world", strlen("world"));
    ret = http_msg_serialize(msg, buffer, sizeof(buffer));
    CU_ASSERT(ret > 0);
    buffer[ret] = '\0';
    LOG("%s\n", buffer);
    success = http_msg_view_parse_request(&view, buffer);
    CU_ASSERT_TRUE(success);
    CU_ASSERT_NSTRING_EQUAL(view.body, msg->body->data, msg->body->length);

    // validate
    struct http_msg tmp;
    http_msg_init(&tmp);
    ret = http_msg_parse_request(&tmp, buffer);
    CU_ASSERT(ret > 0);
    CU_ASSERT_STRING_EQUAL(tmp.method, msg->method);
    CU_ASSERT_STRING_EQUAL(tmp.uri, msg->uri);
    CU_ASSERT_STRING_EQUAL(tmp.version, msg->version);

    http_msg_clean(&tmp);

    msg = http_msg_delete(msg);
    CU_ASSERT_PTR_NULL(msg);
}


/**
 *  Test http response
 *
 */
void test_http_response(void)
{
    int ret;
    bool success;
    struct http_msg_view view;

    char buffer[256];
    memset(buffer, 0, sizeof(buffer));

    struct http_msg *msg = http_msg_new_response("HTTP/1.1", 200, "OK");
    CU_ASSERT_PTR_NOT_NULL(msg);

    // no headers, no body
    ret = http_msg_serialize(msg, buffer, sizeof(buffer));
    CU_ASSERT(ret > 0);
    buffer[ret] = '\0';
    LOG("%s\n", buffer);
    success = http_msg_view_parse_response(&view, buffer);
    CU_ASSERT_TRUE(success);
    CU_ASSERT_NSTRING_EQUAL(view.version, msg->version, strlen(msg->version));
    CU_ASSERT_NSTRING_EQUAL(view.status, "200", strlen("200"));
    CU_ASSERT_NSTRING_EQUAL(view.reason, msg->reason, strlen(msg->reason));

    // no headers, body
    http_msg_append_body(msg, "hello", strlen("hello"));
    ret = http_msg_serialize(msg, buffer, sizeof(buffer));
    CU_ASSERT(ret > 0);
    buffer[ret] = '\0';
    LOG("%s\n", buffer);
    success = http_msg_view_parse_response(&view, buffer);
    CU_ASSERT_TRUE(success);
    CU_ASSERT_NSTRING_EQUAL(view.body, msg->body->data, msg->body->length);
    CU_ASSERT_NSTRING_EQUAL(view.version, msg->version, strlen(msg->version));

    // headers, body
    http_msg_add_header(msg, "first", "test1");
    http_msg_add_header(msg, "second", NULL);
    http_msg_add_header(msg, "third", "test3");
    http_msg_append_body(msg, "world", strlen("world"));
    ret = http_msg_serialize(msg, buffer, sizeof(buffer));
    CU_ASSERT(ret > 0);
    buffer[ret] = '\0';
    LOG("%s\n", buffer);
    success = http_msg_view_parse_response(&view, buffer);
    CU_ASSERT_TRUE(success);
    CU_ASSERT_NSTRING_EQUAL(view.body, msg->body->data, msg->body->length);

    // validate
    struct http_msg tmp;
    http_msg_init(&tmp);
    ret = http_msg_parse_response(&tmp, buffer);
    CU_ASSERT(ret > 0);
    CU_ASSERT_STRING_EQUAL(tmp.version, msg->version);
    CU_ASSERT_EQUAL(tmp.status, msg->status);
    CU_ASSERT_STRING_EQUAL(tmp.reason, msg->reason);

    http_msg_clean(&tmp);

    msg = http_msg_delete(msg);
    CU_ASSERT_PTR_NULL(msg);
}


/**
 *  Test http headers
 *
 */
void test_http_headers(void)
{
    int ret;
    bool success;
    struct http_msg_view view;
    const char *header_name;
    const char *header_value;

    char buffer[256];
    memset(buffer, 0, sizeof(buffer));

    struct http_msg *msg = http_msg_new_request("GET", "/", "HTTP/1.1");
    CU_ASSERT_PTR_NOT_NULL(msg);

    // no headers
    ret = http_msg_serialize(msg, buffer, sizeof(buffer));
    CU_ASSERT(ret > 0);
    buffer[ret] = '\0';
    LOG("%s\n", buffer);
    success = http_msg_view_parse_request(&view, buffer);
    CU_ASSERT_TRUE(success);
    header_name = http_header_find(view.headers, "first", NULL);
    CU_ASSERT_PTR_NULL(header_name);

    // add headers
    http_msg_add_header(msg, "first", "test1");
    http_msg_add_header(msg, "second", NULL);
    http_msg_add_header(msg, "third", "test3");
    ret = http_msg_serialize(msg, buffer, sizeof(buffer));
    CU_ASSERT(ret > 0);
    buffer[ret] = '\0';
    LOG("%s\n", buffer);
    success = http_msg_view_parse_request(&view, buffer);
    CU_ASSERT_TRUE(success);
    // check
    header_name = http_header_find(view.headers, "first", &header_value);
    CU_ASSERT_PTR_NOT_NULL(header_name);
    CU_ASSERT_PTR_NOT_NULL(header_value);
    CU_ASSERT_NSTRING_EQUAL(header_value, "test1", strlen("test1"));
    header_name = http_header_find(view.headers, "second", &header_value);
    CU_ASSERT_PTR_NOT_NULL(header_name);
    CU_ASSERT_PTR_NOT_NULL(header_value);
    CU_ASSERT_EQUAL(0, xstr_word_len(header_value, NULL));

    // remove headers
    http_msg_remove_header(msg, "second");
    http_msg_remove_header(msg, "first");
    ret = http_msg_serialize(msg, buffer, sizeof(buffer));
    CU_ASSERT(ret > 0);
    buffer[ret] = '\0';
    LOG("%s\n", buffer);
    success = http_msg_view_parse_request(&view, buffer);
    CU_ASSERT_TRUE(success);
    // check
    header_name = http_header_find(view.headers, "first", NULL);
    CU_ASSERT_PTR_NULL(header_name);
    header_name = http_header_find(view.headers, "second", NULL);
    CU_ASSERT_PTR_NULL(header_name);
    header_name = http_header_find(view.headers, "third", &header_value);
    CU_ASSERT_PTR_NOT_NULL(header_name);
    CU_ASSERT_PTR_NOT_NULL(header_value);
    CU_ASSERT_NSTRING_EQUAL(header_value, "test3", strlen("test3"));

    msg = http_msg_delete(msg);
    CU_ASSERT_PTR_NULL(msg);
}
