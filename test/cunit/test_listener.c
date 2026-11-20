
#include <CUnit/CUnit.h>

#include "mx/listener.h"
#include "mx/misc.h"

#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>



static void test_listener_init_clean(void);
static void test_listener_new_from_url(void);
static void test_listener_new_from_invalid_url(void);
static void test_listener_unix_lifecycle(void);
static void test_listener_inet6_lifecycle(void);
static void test_listener_list(void);



CU_ErrorCode cu_test_listener(void)
{
    CU_pSuite suite = CU_add_suite("Listener", NULL, NULL);
    if ( !suite ) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    CU_add_test(suite, "Init and clean",                test_listener_init_clean);
    CU_add_test(suite, "New from URL",                  test_listener_new_from_url);
    CU_add_test(suite, "Reject invalid URL",            test_listener_new_from_invalid_url);
    CU_add_test(suite, "Unix socket lifecycle",         test_listener_unix_lifecycle);
    CU_add_test(suite, "IPv6 socket lifecycle",         test_listener_inet6_lifecycle);
    CU_add_test(suite, "Listener list",                 test_listener_list);


    return CU_get_error();
}



static void assert_listener_stopped(const struct listener *listener)
{
    CU_ASSERT_EQUAL(listener->fd, -1);
    CU_ASSERT_PTR_NULL(listener->stream);
}


static void test_listener_init_clean(void)
{
    char address[] = "localhost";
    struct listener listener;

    listener_init(&listener, address, 1883);
    address[0] = 'L';
    CU_ASSERT_PTR_NOT_NULL(listener.address);
    if (listener.address)
        CU_ASSERT_STRING_EQUAL(listener.address, "localhost");
    CU_ASSERT_EQUAL(listener.port, 1883);
    assert_listener_stopped(&listener);

    listener_clean(&listener);
    CU_ASSERT_PTR_NULL(listener.address);
    assert_listener_stopped(&listener);
}


static void test_listener_new_from_url(void)
{
    const struct {
        const char *url;
        const char *address;
        unsigned int port;
        bool ssl;
        bool websocket;
    } cases[] = {
        {"mqtt://localhost", "localhost", 1883, false, false},
        {"mqtts://localhost", "localhost", 8883, true, false},
        {"ws://localhost", "localhost", 80, false, true},
        {"wss://localhost", "localhost", 443, true, true},
        {"wss://127.0.0.1:8443/mqtt?client=test", "127.0.0.1", 8443, true, true},
        {"mqtt://localhost:1", "localhost", 1, false, false},
        {"mqtts://localhost:65535", "localhost", 65535, true, false},
        {"127.0.0.1:1884", "127.0.0.1", 1884, false, false},
        {"file:///tmp/mx-listener.sock", "/tmp/mx-listener.sock", 0, false, false},
        {"/tmp/mx-listener.sock?mode=test", "/tmp/mx-listener.sock", 0, false, false},
    };

    for (unsigned int i = 0; i < ARRAY_SIZE(cases); i++) {
        struct listener *listener = listener_new_from_url(cases[i].url);
        if (!CU_assertImplementation(listener != NULL, __LINE__, cases[i].url,
                                     __FILE__, "", CU_FALSE))
            continue;

        CU_ASSERT_PTR_NOT_NULL(listener->address);
        if (listener->address)
            CU_ASSERT_STRING_EQUAL(listener->address, cases[i].address);
        CU_ASSERT_EQUAL(listener->port, cases[i].port);
        CU_ASSERT_EQUAL(listener->ssl, cases[i].ssl);
        CU_ASSERT_EQUAL(listener->websocket, cases[i].websocket);
        CU_ASSERT_EQUAL(listener_is_unix(listener), cases[i].port == 0);
        CU_ASSERT_FALSE(listener->bridge);
        CU_ASSERT_EQUAL(listener->keep_alive, 0);
        CU_ASSERT_EQUAL(listener->max_connections, 0);
        assert_listener_stopped(listener);

        listener_delete(listener);
    }
}


static void test_listener_new_from_invalid_url(void)
{
    const char *urls[] = {
        "http://localhost:1883",
        "mqtt://localhost:",
        "mqtt://localhost:abc",
        "mqtt://localhost:1883x",
        "mqtt://localhost:0",
        "mqtt://localhost:-1",
        "mqtt://localhost:65536",
        "mqtt://localhost:9999999999999999999999999999",
    };

    for (unsigned int i = 0; i < ARRAY_SIZE(urls); i++) {
        struct listener *listener = listener_new_from_url(urls[i]);
        CU_assertImplementation(listener == NULL, __LINE__, urls[i], __FILE__, "", CU_FALSE);
        if (listener)
            listener_delete(listener);
    }
}


static void assert_listener_accepts_connection(struct listener *listener, int family)
{
    struct sockaddr_storage address;
    socklen_t address_len = sizeof(address);
    int status = getsockname(listener->fd, (struct sockaddr *)&address, &address_len);
    CU_ASSERT_EQUAL(status, 0);
    if (status != 0)
        return;
    CU_ASSERT_EQUAL(address.ss_family, family);
    if (address.ss_family != family)
        return;

    int client_fd = socket(family, SOCK_STREAM, 0);
    CU_ASSERT_TRUE(client_fd >= 0);
    if (client_fd < 0)
        return;
    status = connect(client_fd, (struct sockaddr *)&address, address_len);
    CU_ASSERT_EQUAL(status, 0);
    if (status == 0) {
        int accepted_fd = listener_accept(listener);
        CU_ASSERT_TRUE(accepted_fd >= 0);
        if (accepted_fd >= 0) {
            int flags = fcntl(accepted_fd, F_GETFL);
            CU_ASSERT_TRUE(flags >= 0 && (flags & O_NONBLOCK));
            close(accepted_fd);
        }
    }
    close(client_fd);
}


static void test_listener_unix_lifecycle(void)
{
    char directory[] = "/tmp/mx-listener-XXXXXX";
    CU_ASSERT_PTR_NOT_NULL_FATAL(mkdtemp(directory));

    char path[sizeof(directory) + sizeof("/socket")];
    snprintf(path, sizeof(path), "%s/socket", directory);
    struct listener *listener = listener_new_from_url(path);
    CU_ASSERT_PTR_NOT_NULL(listener);
    if (!listener) {
        rmdir(directory);
        return;
    }

    CU_ASSERT_EQUAL(listener_accept(listener), -1);
    int fd = listener_start(listener);
    CU_ASSERT_TRUE(fd >= 0);
    if (fd >= 0) {
        CU_ASSERT_EQUAL(listener->fd, fd);
        CU_ASSERT_PTR_NOT_NULL(listener->stream);
        CU_ASSERT_EQUAL(access(path, F_OK), 0);

        struct listener *duplicate = listener_new(path, 0);
        CU_ASSERT_EQUAL(listener_start(duplicate), -1);
        assert_listener_stopped(duplicate);
        listener_delete(duplicate);

        assert_listener_accepts_connection(listener, AF_UNIX);
        listener_stop(listener);
        assert_listener_stopped(listener);
        CU_ASSERT_EQUAL(fcntl(fd, F_GETFD), -1);
        CU_ASSERT_EQUAL(access(path, F_OK), -1);
    }

    listener_stop(listener);
    assert_listener_stopped(listener);
    listener_delete(listener);
    CU_ASSERT_EQUAL(rmdir(directory), 0);
}


static void test_listener_inet6_lifecycle(void)
{
    struct sockaddr_in6 address = {
        .sin6_family = AF_INET6,
        .sin6_addr = IN6ADDR_LOOPBACK_INIT,
    };
    socklen_t address_len = sizeof(address);

    // Port zero selects Unix sockets in the listener API. Ask the kernel for a TCP port first.
    int probe_fd = socket(AF_INET6, SOCK_STREAM, 0);
    CU_ASSERT_TRUE_FATAL(probe_fd >= 0);
    int status = bind(probe_fd, (struct sockaddr *)&address, address_len);
    if (status == 0)
        status = getsockname(probe_fd, (struct sockaddr *)&address, &address_len);
    close(probe_fd);
    CU_ASSERT_EQUAL_FATAL(status, 0);
    unsigned int port = ntohs(address.sin6_port);
    CU_ASSERT_NOT_EQUAL_FATAL(port, 0);

    struct listener *listener = listener_new("::1", port);
    CU_ASSERT_FALSE(listener_is_unix(listener));
    int fd = listener_start(listener);
    CU_ASSERT_TRUE(fd >= 0);
    if (fd >= 0) {
        CU_ASSERT_EQUAL(listener->fd, fd);
        CU_ASSERT_PTR_NOT_NULL(listener->stream);
        assert_listener_accepts_connection(listener, AF_INET6);

        listener_stop(listener);
        assert_listener_stopped(listener);
        CU_ASSERT_EQUAL(fcntl(fd, F_GETFD), -1);
    }

    listener_delete(listener);
}


static void test_listener_list(void)
{
    struct listener_list *list = listener_list_new();
    struct listener *local = listener_new("localhost", 1883);
    struct listener *secure = listener_new("localhost", 8883);
    struct listener *wildcard = listener_new(NULL, 1883);

    CU_ASSERT_TRUE(listener_list_is_empty(list));
    listener_list_add(list, local);
    listener_list_add(list, secure);
    listener_list_add(list, wildcard);
    CU_ASSERT_FALSE(listener_list_is_empty(list));
    CU_ASSERT_PTR_EQUAL(listener_list_find(list, "localhost", 1883), local);
    CU_ASSERT_PTR_EQUAL(listener_list_find(list, "localhost", 8883), secure);
    CU_ASSERT_PTR_NULL(listener_list_find(list, "127.0.0.1", 1883));
    CU_ASSERT_PTR_NULL(listener_list_find(list, "localhost", 80));

    listener_list_rm(list, local);
    CU_ASSERT_PTR_NULL(listener_list_find(list, "localhost", 1883));
    CU_ASSERT_PTR_EQUAL(listener_list_find(list, "localhost", 8883), secure);
    CU_ASSERT_PTR_NULL(listener_delete(local));

    listener_list_clean(list);
    CU_ASSERT_TRUE(listener_list_is_empty(list));
    CU_ASSERT_PTR_NULL(listener_list_delete(list));
}

