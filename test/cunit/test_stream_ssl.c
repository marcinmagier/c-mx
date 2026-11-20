#include "test.h"

#include "mx/socket.h"
#include "mx/ssl.h"
#include "mx/stream_ssl.h"
#include "../../source/private_ssl.h"

#include <CUnit/Basic.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>


static void test_stream_ssl_handshake(void);
static void test_stream_ssl_trusted_client(void);
static void test_stream_ssl_missing_client_certificate(void);
static void test_stream_ssl_untrusted_client(void);
static void test_stream_ssl_invalid_key_file(void);
static void test_stream_ssl_plaintext_peer(void);
static void test_stream_ssl_truncated_handshake(void);
static void test_stream_ssl_write_drives_handshake(void);
static void test_stream_ssl_read_close_notify(void);
static void test_stream_ssl_invalid_record(void);
static void test_stream_ssl_transport_error(void);
#ifdef MX_SSL_HAS_TLS13_PSK
static void test_stream_ssl_psk(void);
static void test_stream_ssl_psk_unknown_identity(void);
static void test_stream_ssl_psk_wrong_key(void);
static void test_stream_ssl_psk_client_rejects_certificate(void);
static void test_stream_ssl_psk_server_requires_psk(void);
#endif


CU_ErrorCode cu_test_stream_ssl(void)
{
    CU_pSuite suite = CU_add_suite("Suite stream_ssl", NULL, NULL);
    if (suite) {
        CU_add_test(suite, "Test TLS handshake and bidirectional data", test_stream_ssl_handshake);
        CU_add_test(suite, "Test TLS accepts a trusted client certificate", test_stream_ssl_trusted_client);
        CU_add_test(suite, "Test TLS rejects a missing client certificate", test_stream_ssl_missing_client_certificate);
        CU_add_test(suite, "Test TLS rejects an untrusted client certificate", test_stream_ssl_untrusted_client);
        CU_add_test(suite, "Test TLS rejects a certificate PEM used as a private key", test_stream_ssl_invalid_key_file);
        CU_add_test(suite, "Test TLS rejects plaintext input", test_stream_ssl_plaintext_peer);
        CU_add_test(suite, "Test TLS fails on EOF during a handshake record", test_stream_ssl_truncated_handshake);
        CU_add_test(suite, "Test TLS writes resume a suspended handshake", test_stream_ssl_write_drives_handshake);
        CU_add_test(suite, "Test TLS read returns EOF on close_notify", test_stream_ssl_read_close_notify);
        CU_add_test(suite, "Test TLS rejects an invalid record after the handshake", test_stream_ssl_invalid_record);
        CU_add_test(suite, "Test TLS reports transport errors after the handshake", test_stream_ssl_transport_error);
#ifdef MX_SSL_HAS_TLS13_PSK
        CU_add_test(suite, "Test TLS 1.3 external PSK handshake", test_stream_ssl_psk);
        CU_add_test(suite, "Test TLS 1.3 rejects an unknown PSK identity", test_stream_ssl_psk_unknown_identity);
        CU_add_test(suite, "Test TLS 1.3 rejects a wrong PSK", test_stream_ssl_psk_wrong_key);
        CU_add_test(suite, "Test PSK client rejects certificate fallback", test_stream_ssl_psk_client_rejects_certificate);
        CU_add_test(suite, "Test PSK server rejects certificate fallback", test_stream_ssl_psk_server_requires_psk);
#endif
    }
    return CU_get_error();
}





static bool test_stream_ssl_install_certificate(struct ssl *ssl)
{
    EVP_PKEY_CTX *keygen = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL);
    EVP_PKEY *key = NULL;
    X509 *cert = X509_new();
    bool installed = false;

    if (keygen && cert && EVP_PKEY_keygen_init(keygen) > 0 &&
        EVP_PKEY_CTX_set_rsa_keygen_bits(keygen, 2048) > 0 &&
        EVP_PKEY_keygen(keygen, &key) > 0) {
        X509_NAME *name = X509_get_subject_name(cert);
        installed = name &&
                    ASN1_INTEGER_set(X509_get_serialNumber(cert), 1) &&
                    X509_gmtime_adj(X509_getm_notBefore(cert), -60) &&
                    X509_gmtime_adj(X509_getm_notAfter(cert), 3600) &&
                    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
                                               (const unsigned char *)"localhost", -1, -1, 0) &&
                    X509_set_issuer_name(cert, name) &&
                    X509_set_pubkey(cert, key) &&
                    X509_sign(cert, key, EVP_sha256()) > 0 &&
                    SSL_CTX_use_certificate(ssl->ctx, cert) == 1 &&
                    SSL_CTX_use_PrivateKey(ssl->ctx, key) == 1;
    }

    X509_free(cert);
    EVP_PKEY_free(key);
    EVP_PKEY_CTX_free(keygen);
    return installed;
}


static void test_stream_ssl_exchange(struct stream_ssl *sender, struct stream_ssl *receiver,
                                     const char *message)
{
    char buffer[64];
    size_t length = strlen(message);
    ssize_t written = stream_ssl_write(sender, message, length);
    CU_ASSERT_EQUAL(written, length);
    if (written != (ssize_t)length)
        return;

    ssize_t received = stream_ssl_read(receiver, buffer, sizeof(buffer));
    CU_ASSERT_EQUAL(received, length);
    if (received == (ssize_t)length)
        CU_ASSERT_EQUAL(memcmp(buffer, message, length), 0);
}


static bool test_stream_ssl_advance_handshake(struct stream_ssl *ssl_stream, struct stream *stream)
{
    if (stream_get_status(stream) == STREAM_ST_READY)
        return true;

    char byte;
    errno = 0;
    ssize_t result = stream_ssl_read(ssl_stream, &byte, sizeof(byte));
    return result < 0 && (errno == EAGAIN || errno == EWOULDBLOCK);
}


enum test_stream_ssl_handshake_outcome {
    TEST_TLS_HANDSHAKE_OK,
    TEST_TLS_CLIENT_REJECTS,
    TEST_TLS_SERVER_REJECTS
};

static void test_stream_ssl_run_pair(struct ssl *client_ssl, struct ssl *server_ssl,
                                     enum test_stream_ssl_handshake_outcome expected)
{
    int sockets[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) < 0) {
        CU_FAIL("Could not create socket pair");
        return;
    }

    socket_set_non_blocking(sockets[0], 1);
    socket_set_non_blocking(sockets[1], 1);

    struct stream_ssl *client = stream_ssl_new(client_ssl, stream_new(sockets[0]));
    struct stream_ssl *server = stream_ssl_new(server_ssl, stream_new(sockets[1]));
    struct stream *client_stream = stream_ssl_to_stream(client);
    struct stream *server_stream = stream_ssl_to_stream(server);
    CU_ASSERT_PTR_EQUAL(stream_ssl_from_stream(client_stream), client);

    stream_ssl_accept(server);
    stream_ssl_connect(client);
    enum test_stream_ssl_handshake_outcome outcome = TEST_TLS_HANDSHAKE_OK;
    for (int attempt = 0; attempt < 16; ++attempt) {
        if (!test_stream_ssl_advance_handshake(server, server_stream)) {
            outcome = TEST_TLS_SERVER_REJECTS;
            break;
        }
        if (!test_stream_ssl_advance_handshake(client, client_stream)) {
            outcome = TEST_TLS_CLIENT_REJECTS;
            break;
        }
        if (stream_get_status(server_stream) == STREAM_ST_READY &&
            stream_get_status(client_stream) == STREAM_ST_READY)
            break;
    }

    CU_ASSERT_EQUAL(outcome, expected);
    if (expected == TEST_TLS_HANDSHAKE_OK) {
        CU_ASSERT_EQUAL(stream_get_status(client_stream), STREAM_ST_READY);
        CU_ASSERT_EQUAL(stream_get_status(server_stream), STREAM_ST_READY);
        if (stream_get_status(client_stream) == STREAM_ST_READY &&
            stream_get_status(server_stream) == STREAM_ST_READY) {
            test_stream_ssl_exchange(client, server, "hello from client");
            test_stream_ssl_exchange(server, client, "hello from server");
        }
    }
    else if (expected == TEST_TLS_SERVER_REJECTS) {
        CU_ASSERT_NOT_EQUAL(stream_get_status(server_stream), STREAM_ST_READY);
    }
    else {
        CU_ASSERT_NOT_EQUAL(stream_get_status(client_stream), STREAM_ST_READY);
    }

    stream_ssl_delete(client);
    stream_ssl_delete(server);
    close(sockets[0]);
    close(sockets[1]);
}


static void test_stream_ssl_connection(bool verify_client, bool provide_certificate, bool trust_certificate)
{
    struct ssl *server_ssl = ssl_new();
    struct ssl *client_ssl = ssl_new();
    bool installed = test_stream_ssl_install_certificate(server_ssl);
    if (installed && provide_certificate)
        installed = test_stream_ssl_install_certificate(client_ssl);
    if (installed && trust_certificate) {
        X509 *client_cert = SSL_CTX_get0_certificate(client_ssl->ctx);
        installed = X509_STORE_add_cert(SSL_CTX_get_cert_store(server_ssl->ctx), client_cert) == 1;
    }
    CU_ASSERT_TRUE(installed);
    if (installed) {
        ssl_set_verify_peer(server_ssl, verify_client);
        test_stream_ssl_run_pair(client_ssl, server_ssl,
                                 !verify_client || trust_certificate ? TEST_TLS_HANDSHAKE_OK : TEST_TLS_SERVER_REJECTS);
    }

    ssl_delete(client_ssl);
    ssl_delete(server_ssl);
}


static void test_stream_ssl_handshake(void)
{
    test_stream_ssl_connection(false, false, false);
}

static void test_stream_ssl_trusted_client(void)
{
    test_stream_ssl_connection(true, true, true);
}

static void test_stream_ssl_missing_client_certificate(void)
{
    test_stream_ssl_connection(true, false, false);
}

static void test_stream_ssl_untrusted_client(void)
{
    test_stream_ssl_connection(true, true, false);
}

static void test_stream_ssl_invalid_key_file(void)
{
    struct ssl *fixture = ssl_new();
    bool installed = test_stream_ssl_install_certificate(fixture);
    CU_ASSERT_TRUE(installed);
    if (!installed) {
        ssl_delete(fixture);
        return;
    }

    char path[] = "/tmp/mx-cunit-cert-XXXXXX";
    int fd = mkstemp(path);
    CU_ASSERT_TRUE(fd >= 0);
    if (fd < 0) {
        ssl_delete(fixture);
        return;
    }

    bool saved = false;
    FILE *file = fdopen(fd, "w");
    if (file) {
        int written = PEM_write_X509(file, SSL_CTX_get0_certificate(fixture->ctx));
        saved = fclose(file) == 0 && written == 1;
    }
    else {
        close(fd);
    }
    CU_ASSERT_TRUE(saved);
    if (saved) {
        struct ssl *loaded = ssl_new();
        ERR_clear_error();
        ssl_set_certfile(loaded, path, path);
        CU_ASSERT_PTR_NOT_NULL(SSL_CTX_get0_certificate(loaded->ctx));
        CU_ASSERT_PTR_NULL(SSL_CTX_get0_privatekey(loaded->ctx));
        CU_ASSERT_NOT_EQUAL(ERR_peek_error(), 0);
        ERR_clear_error();
        ssl_delete(loaded);
    }

    unlink(path);
    ssl_delete(fixture);
}


static void test_stream_ssl_reject_input(const void *input, size_t length, bool truncated)
{
    int sockets[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) < 0) {
        CU_FAIL("Could not create socket pair");
        return;
    }

    socket_set_non_blocking(sockets[0], 1);
    struct ssl *ssl = ssl_new();
    struct stream_ssl *server = stream_ssl_new(ssl, stream_new(sockets[0]));
    stream_ssl_accept(server);

    ssize_t written = write(sockets[1], input, length);
    CU_ASSERT_EQUAL(written, length);
    if (written == (ssize_t)length) {
        char byte;
        if (truncated) {
            errno = 0;
            ssize_t pending = stream_ssl_read(server, &byte, sizeof(byte));
            int pending_error = errno;
            CU_ASSERT_EQUAL(pending, -1);
            CU_ASSERT_TRUE(pending_error == EAGAIN || pending_error == EWOULDBLOCK);
            CU_ASSERT_EQUAL(shutdown(sockets[1], SHUT_WR), 0);
        }

        errno = 0;
        ssize_t result = stream_ssl_read(server, &byte, sizeof(byte));
        int error = errno;
        if (truncated) {
            CU_ASSERT_TRUE(result <= 0);  // OpenSSL may report EOF or a TLS error.
        }
        else {
            CU_ASSERT_EQUAL(result, -1);
        }
        if (result < 0) {
            CU_ASSERT_TRUE(error != EAGAIN && error != EWOULDBLOCK);
        }
        CU_ASSERT_NOT_EQUAL(stream_get_status(stream_ssl_to_stream(server)), STREAM_ST_READY);
    }

    stream_ssl_delete(server);
    ssl_delete(ssl);
    close(sockets[0]);
    close(sockets[1]);
}


static void test_stream_ssl_plaintext_peer(void)
{
    const char request[] = "GET / HTTP/1.1\r\n\r\n";
    test_stream_ssl_reject_input(request, sizeof(request) - 1, false);
}

static void test_stream_ssl_truncated_handshake(void)
{
    // A TLS handshake record header with no payload.
    const unsigned char header[] = { 0x16, 0x03, 0x03, 0x00, 0x10 };
    test_stream_ssl_reject_input(header, sizeof(header), true);
}

static int test_stream_ssl_defer_certificate(SSL *connection, void *arg)
{
    (void)connection;
    unsigned int *calls = arg;
    ++*calls;
    return *calls == 1 ? -1 : 1;
}


static bool test_stream_ssl_handshake_by_writing(struct stream_ssl *server, SSL *peer,
                                                 const unsigned int *certificate_calls)
{
    const char payload = 'x';
    ssize_t written = stream_ssl_write(server, &payload, sizeof(payload));
    bool pending = stream_try_again(written);
    CU_ASSERT_TRUE(pending);
    if (!pending)
        return false;

    int connected = SSL_connect(peer);
    int error = SSL_get_error(peer, connected);
    CU_ASSERT_EQUAL(error, SSL_ERROR_WANT_READ);
    if (error != SSL_ERROR_WANT_READ)
        return false;

    if (certificate_calls) {
        written = stream_ssl_write(server, &payload, sizeof(payload));
        CU_ASSERT_EQUAL(written, -1);
        CU_ASSERT_EQUAL(*certificate_calls, 1);
        CU_ASSERT_NOT_EQUAL(stream_get_status(stream_ssl_to_stream(server)), STREAM_ST_READY);
    }

    bool sent = false;
    for (int attempt = 0; attempt < 16; ++attempt) {
        if (!sent) {
            written = stream_ssl_write(server, &payload, sizeof(payload));
            sent = written == sizeof(payload);
            if (!sent && !stream_try_again(written)) {
                CU_FAIL("TLS stream failed to complete the handshake while writing");
                return false;
            }
        }
        if (connected != 1) {
            connected = SSL_connect(peer);
            if (connected != 1 && SSL_get_error(peer, connected) != SSL_ERROR_WANT_READ) {
                CU_FAIL("OpenSSL peer failed to complete the handshake");
                return false;
            }
        }
        if (sent && connected == 1) {
            char received = 0;
            int bytes = SSL_read(peer, &received, sizeof(received));
            CU_ASSERT_EQUAL(bytes, sizeof(payload));
            CU_ASSERT_EQUAL(received, payload);
            CU_ASSERT_EQUAL(stream_get_status(stream_ssl_to_stream(server)), STREAM_ST_READY);
            if (certificate_calls) {
                CU_ASSERT_EQUAL(*certificate_calls, 2);
            }
            return bytes == sizeof(payload);
        }
    }

    CU_FAIL("TLS handshake did not finish");
    return false;
}


static void test_stream_ssl_with_openssl_peer(bool defer_certificate,
                                              void (*check)(struct stream_ssl *, SSL *, int *))
{
    struct ssl *server_ssl = ssl_new();
    struct ssl *client_ssl = ssl_new();
    bool installed = test_stream_ssl_install_certificate(server_ssl);
    CU_ASSERT_TRUE(installed);
    if (!installed) {
        ssl_delete(client_ssl);
        ssl_delete(server_ssl);
        return;
    }

    int sockets[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) < 0) {
        CU_FAIL("Could not create socket pair");
        ssl_delete(client_ssl);
        ssl_delete(server_ssl);
        return;
    }
    socket_set_non_blocking(sockets[0], 1);
    socket_set_non_blocking(sockets[1], 1);

    SSL *peer = SSL_new(client_ssl->ctx);
    bool attached = peer && SSL_set_fd(peer, sockets[1]) == 1;
    CU_ASSERT_TRUE(attached);
    if (attached) {
        unsigned int certificate_calls = 0;
        if (defer_certificate)
            SSL_CTX_set_cert_cb(server_ssl->ctx, test_stream_ssl_defer_certificate, &certificate_calls);

        struct stream_ssl *server = stream_ssl_new(server_ssl, stream_new(sockets[0]));
        stream_ssl_accept(server);
        if (test_stream_ssl_handshake_by_writing(server, peer, defer_certificate ? &certificate_calls : NULL) && check)
            check(server, peer, &sockets[0]);

        stream_ssl_delete(server);
    }
    SSL_free(peer);
    if (sockets[0] >= 0)
        close(sockets[0]);
    close(sockets[1]);
    ssl_delete(client_ssl);
    ssl_delete(server_ssl);
}


static void test_stream_ssl_check_close_notify(struct stream_ssl *server, SSL *peer, int *socket_fd)
{
    (void)socket_fd;
    CU_ASSERT_EQUAL(SSL_shutdown(peer), 0);
    char byte;
    CU_ASSERT_EQUAL(stream_ssl_read(server, &byte, sizeof(byte)), 0);
}

static void test_stream_ssl_check_invalid_record(struct stream_ssl *server, SSL *peer, int *socket_fd)
{
    (void)socket_fd;
    // A TLS application record too short to contain an authentication tag.
    const unsigned char record[] = { 0x17, 0x03, 0x03, 0x00, 0x01, 0x00 };
    ssize_t written = write(SSL_get_fd(peer), record, sizeof(record));
    CU_ASSERT_EQUAL(written, sizeof(record));
    if (written == sizeof(record)) {
        char byte = 'x';
        ssize_t result = stream_ssl_read(server, &byte, sizeof(byte));
        bool retry = stream_try_again(result);
        CU_ASSERT_EQUAL(result, -1);
        CU_ASSERT_FALSE(retry);
        CU_ASSERT_EQUAL(stream_ssl_write(server, &byte, sizeof(byte)), -1);
    }
}

static void test_stream_ssl_check_transport_error(struct stream_ssl *server, SSL *peer, int *socket_fd)
{
    (void)peer;
    CU_ASSERT_EQUAL(close(*socket_fd), 0);
    *socket_fd = -1;

    char byte = 'x';
    ssize_t result = stream_ssl_read(server, &byte, sizeof(byte));
    int error = errno;
    CU_ASSERT_EQUAL(result, -1);
    CU_ASSERT_EQUAL(error, EBADF);
    CU_ASSERT_EQUAL(stream_ssl_write(server, &byte, sizeof(byte)), 0);
}


static void test_stream_ssl_write_drives_handshake(void)
{
    test_stream_ssl_with_openssl_peer(true, NULL);
}

static void test_stream_ssl_read_close_notify(void)
{
    test_stream_ssl_with_openssl_peer(false, test_stream_ssl_check_close_notify);
}

static void test_stream_ssl_invalid_record(void)
{
    test_stream_ssl_with_openssl_peer(false, test_stream_ssl_check_invalid_record);
}

static void test_stream_ssl_transport_error(void)
{
    test_stream_ssl_with_openssl_peer(false, test_stream_ssl_check_transport_error);
}

#ifdef MX_SSL_HAS_TLS13_PSK
static void test_stream_ssl_psk_connection(bool client_psk, bool server_psk,
                                           const char *client_identity, bool wrong_key,
                                           enum test_stream_ssl_handshake_outcome expected)
{
    struct ssl *client = ssl_new();
    struct ssl *server = ssl_new();
    unsigned char key[32] = {
        0x96, 0x2c, 0x55, 0x0d, 0x89, 0xa2, 0x21, 0x44,
        0x51, 0xed, 0xd8, 0xb2, 0x3f, 0x27, 0x20, 0x6d,
        0xdd, 0x00, 0x1c, 0xa3, 0xe9, 0x41, 0x5b, 0x89,
        0x89, 0xd3, 0xc7, 0xe1, 0x92, 0xf4, 0x75, 0x78
    };

    // For fallback tests, the server has a certificate it could otherwise use.
    bool configured = (client_psk && server_psk) || test_stream_ssl_install_certificate(server);
    if (configured && !server_psk) {
        X509 *server_cert = SSL_CTX_get0_certificate(server->ctx);
        configured = X509_STORE_add_cert(SSL_CTX_get_cert_store(client->ctx), server_cert) == 1;
    }
    if (configured && server_psk)
        configured = ssl_set_psk(server, "test-device", key, sizeof(key)) == 0;
    if (wrong_key)
        key[0] ^= 1;
    if (configured && client_psk) {
        configured = ssl_set_psk(client, client_identity, key, sizeof(key)) == 0;
        ssl_set_verify_peer(client, false);
    }

    CU_ASSERT_TRUE(configured);
    if (configured)
        test_stream_ssl_run_pair(client, server, expected);

    ssl_delete(client);
    ssl_delete(server);
}


static void test_stream_ssl_psk(void)
{
    test_stream_ssl_psk_connection(true, true, "test-device", false, TEST_TLS_HANDSHAKE_OK);
}

static void test_stream_ssl_psk_unknown_identity(void)
{
    test_stream_ssl_psk_connection(true, true, "other-device", false, TEST_TLS_SERVER_REJECTS);
}

static void test_stream_ssl_psk_wrong_key(void)
{
    test_stream_ssl_psk_connection(true, true, "test-device", true, TEST_TLS_SERVER_REJECTS);
}

static void test_stream_ssl_psk_client_rejects_certificate(void)
{
    test_stream_ssl_psk_connection(true, false, "test-device", false, TEST_TLS_CLIENT_REJECTS);
}

static void test_stream_ssl_psk_server_requires_psk(void)
{
    test_stream_ssl_psk_connection(false, true, NULL, false, TEST_TLS_SERVER_REJECTS);
}
#endif
