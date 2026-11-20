

#include "mx/ssl.h"
#include "mx/stream.h"

#include "mx/misc.h"
#include "mx/log.h"
#include "mx/memory.h"
#include "mx/queue.h"
#include "mx/misc.h"

#include "private_ssl.h"

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/ssl.h>
#include <openssl/bn.h>

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>



static int verify_callback(int ok, X509_STORE_CTX *ctx)
{
    if (!ok) {
        int error = X509_STORE_CTX_get_error(ctx);
        ERROR("TLS certificate verification failed at depth %d: %s",
              X509_STORE_CTX_get_error_depth(ctx), X509_verify_cert_error_string(error));
    }
    return ok;
}


#ifdef MX_SSL_HAS_TLS13_PSK
struct ssl_psk {
    size_t identity_len;
    size_t key_len;
    unsigned char key[SSL_MAX_MASTER_KEY_LENGTH];
    unsigned char identity[];
};

static CRYPTO_ONCE psk_index_once = CRYPTO_ONCE_STATIC_INIT;
static int psk_index = -1;

// OpenSSL uses the session reuse path for external PSKs.
static const unsigned char psk_session_context[] = "mx-tls13-psk";

static void ssl_psk_free(struct ssl_psk *psk)
{
    if (psk)
        OPENSSL_clear_free(psk, sizeof(*psk) + psk->identity_len);
}

static void ssl_psk_context_free(void *parent, void *ptr, CRYPTO_EX_DATA *data, int index,
                                 long argl, void *argp)
{
    UNUSED(parent);
    UNUSED(data);
    UNUSED(index);
    UNUSED(argl);
    UNUSED(argp);
    ssl_psk_free(ptr);
}

static void ssl_psk_init_index(void)
{
    psk_index = SSL_CTX_get_ex_new_index(0, NULL, NULL, NULL, ssl_psk_context_free);
}

static SSL_SESSION *ssl_psk_session(SSL *ssl, const struct ssl_psk *psk)
{
    const unsigned char cipher_id[] = { 0x13, 0x01 };  // TLS_AES_128_GCM_SHA256
    const SSL_CIPHER *cipher = SSL_CIPHER_find(ssl, cipher_id);
    SSL_SESSION *session = SSL_SESSION_new();
    if (cipher && session &&
        SSL_SESSION_set_protocol_version(session, TLS1_3_VERSION) &&
        SSL_SESSION_set_cipher(session, cipher) &&
        SSL_SESSION_set1_id_context(session, psk_session_context, sizeof(psk_session_context) - 1) &&
        SSL_SESSION_set1_master_key(session, psk->key, psk->key_len))
        return session;

    SSL_SESSION_free(session);
    return NULL;
}

static int ssl_psk_use_session(SSL *ssl, const EVP_MD *digest, const unsigned char **identity,
                               size_t *identity_len, SSL_SESSION **session)
{
    const struct ssl_psk *psk = SSL_CTX_get_ex_data(SSL_get_SSL_CTX(ssl), psk_index);
    if (!psk || (digest && EVP_MD_type(digest) != NID_sha256))
        return 0;

    *identity = psk->identity;
    *identity_len = psk->identity_len;
    *session = ssl_psk_session(ssl, psk);
    return *session != NULL;
}

static int ssl_psk_find_session(SSL *ssl, const unsigned char *identity, size_t identity_len,
                                SSL_SESSION **session)
{
    const struct ssl_psk *psk = SSL_CTX_get_ex_data(SSL_get_SSL_CTX(ssl), psk_index);
    *session = NULL;
    if (!psk || psk->identity_len != identity_len || memcmp(psk->identity, identity, identity_len))
        return 1;  // This identity does not select a PSK.

    *session = ssl_psk_session(ssl, psk);
    return *session != NULL;
}

/* Prevent certificate authentication if the peer does not select the PSK. */
static int ssl_psk_reject_local_certificate(SSL *ssl, void *arg)
{
    UNUSED(ssl);
    UNUSED(arg);
    return 0;
}

static int ssl_psk_reject_peer_certificate(X509_STORE_CTX *ctx, void *arg)
{
    UNUSED(arg);
    X509_STORE_CTX_set_error(ctx, X509_V_ERR_APPLICATION_VERIFICATION);
    return 0;
}
#endif


static unsigned int ssl_usage = 0;



void ssl_global_init(void)
{
        SSL_library_init();
        OpenSSL_add_all_algorithms();
        SSL_load_error_strings();
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
        // OpenSSL now loads error strings automatically
        // See https://www.openssl.org/docs/manmaster/man7/migration_guide.html
#else
        ERR_load_BIO_strings();
        ERR_load_crypto_strings();
#endif
}


void ssl_global_clean(void)
{
    BIO_meth_free_stream_ssl();
}


void ssl_init(struct ssl *self)
{
    if (ssl_usage++ == 0)
        ssl_global_init();


    self->psk_configured = false;
    self->ctx = SSL_CTX_new(TLS_method());
    if (!self->ctx)
        RESET("Could not create ssl context");

    /* Recommended to avoid SSLv2 & SSLv3 */
    SSL_CTX_set_options(self->ctx, SSL_OP_ALL|SSL_OP_NO_SSLv2|SSL_OP_NO_SSLv3);

    SSL_CTX_set_mode(self->ctx, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
}


void ssl_clean(struct ssl *self)
{
    SSL_CTX_free(self->ctx);


    if ((ssl_usage > 0) && (--ssl_usage == 0))
        ssl_global_clean();
}




struct ssl* ssl_new(void)
{
    struct ssl *self = xmalloc(sizeof(struct ssl));

    ssl_init(self);

    return self;
}


struct ssl* ssl_delete(struct ssl *self)
{
    ssl_clean(self);

    return xfree(self);
}


void ssl_set_certfile(struct ssl *self, const char *certfile, const char *keyfile)
{
    /* Load certificate and private key files, and check consistency */
    if (SSL_CTX_use_certificate_file(self->ctx, certfile,  SSL_FILETYPE_PEM) != 1)
        ERROR("SSL_CTX_use_certificate_file failed");

    if (SSL_CTX_use_PrivateKey_file(self->ctx, keyfile, SSL_FILETYPE_PEM) != 1)
        ERROR("SSL_CTX_use_PrivateKey_file failed");

    /* Make sure the key and certificate file match. */
    if (SSL_CTX_check_private_key(self->ctx) != 1) {
        ERROR("SSL_CTX_check_private_key failed");
    }
    else {
        INFO("certificate and private key loaded and verified");
    }
}


void ssl_set_cafile(struct ssl *self, const char *cafile)
{
    SSL_CTX_load_verify_locations(self->ctx, cafile, NULL);
}


void ssl_set_verify_peer(struct ssl *self, bool verify)
{
    if (self->psk_configured)
        return;  // PSK mode controls certificate handling.

    int verify_flags = SSL_VERIFY_NONE;
    if (verify)
        verify_flags = SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT | SSL_VERIFY_CLIENT_ONCE;

    SSL_CTX_set_verify(self->ctx, verify_flags, verify ? verify_callback : NULL);
}


int ssl_set_psk(struct ssl *self, const char *identity, const unsigned char *key, size_t key_len)
{
#ifdef MX_SSL_HAS_TLS13_PSK
    if (!self || !identity || !key || key_len < 16 || key_len > SSL_MAX_MASTER_KEY_LENGTH) {
        errno = EINVAL;
        return -1;
    }
    size_t identity_len = strlen(identity);
    if (identity_len == 0 || identity_len > 255) {
        errno = EINVAL;
        return -1;
    }
    if (self->psk_configured) {
        errno = EALREADY;
        return -1;
    }
    if (!CRYPTO_THREAD_run_once(&psk_index_once, ssl_psk_init_index) || psk_index < 0) {
        errno = EIO;
        return -1;
    }

    struct ssl_psk *psk = OPENSSL_malloc(sizeof(*psk) + identity_len);
    if (!psk) {
        errno = ENOMEM;
        return -1;
    }
    psk->identity_len = identity_len;
    psk->key_len = key_len;
    memcpy(psk->identity, identity, identity_len);
    memcpy(psk->key, key, key_len);

    SSL_CTX *ctx = self->ctx;
    if (!SSL_CTX_set_ex_data(ctx, psk_index, psk)) {
        ssl_psk_free(psk);
        errno = EIO;
        return -1;
    }

    int previous_min_version = SSL_CTX_get_min_proto_version(ctx);
    if (!SSL_CTX_set_min_proto_version(ctx, TLS1_3_VERSION) ||
        !SSL_CTX_set_ciphersuites(ctx, "TLS_AES_128_GCM_SHA256")) {
        SSL_CTX_set_min_proto_version(ctx, previous_min_version);
        SSL_CTX_set_ex_data(ctx, psk_index, NULL);
        ssl_psk_free(psk);
        errno = EIO;
        return -1;
    }

    SSL_CTX_set_session_id_context(ctx, psk_session_context, sizeof(psk_session_context) - 1);
    SSL_CTX_set_psk_use_session_callback(ctx, ssl_psk_use_session);
    SSL_CTX_set_psk_find_session_callback(ctx, ssl_psk_find_session);
    SSL_CTX_set_cert_cb(ctx, ssl_psk_reject_local_certificate, NULL);
    SSL_CTX_set_cert_verify_callback(ctx, ssl_psk_reject_peer_certificate, NULL);
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);
    self->psk_configured = true;
    return 0;
#else
    UNUSED(self);
    UNUSED(identity);
    UNUSED(key);
    UNUSED(key_len);
    errno = ENOTSUP;
    return -1;
#endif
}
