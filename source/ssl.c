

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



typedef struct verify_options_st {
    int depth;
    int quiet;
    int error;
    int return_error;
} VERIFY_CB_ARGS;

VERIFY_CB_ARGS verify_args = { -1, 0, X509_V_OK, 0 };



int verify_callback(int ok, X509_STORE_CTX *ctx)
{
    X509 *err_cert;
    int err, depth;

    err_cert = X509_STORE_CTX_get_current_cert(ctx);
    err = X509_STORE_CTX_get_error(ctx);
    depth = X509_STORE_CTX_get_error_depth(ctx);

    if (!verify_args.quiet || !ok) {
        printf("depth=%d ", depth);
        if (err_cert != NULL) {
            char *b = X509_NAME_oneline(X509_get_subject_name(err_cert), NULL, 0);
            if (!b)
                return 0;
            if (!*b) {
                OPENSSL_free(b);
                return 1;
            }
            printf("%s\n", b);
            OPENSSL_free(b);
        } else {
            printf("<no cert>\n");
        }
    }
    if (!ok) {
        printf("verify error:num=%d:%s\n", err, X509_verify_cert_error_string(err));
        if (verify_args.depth < 0 || verify_args.depth >= depth) {
            if (!verify_args.return_error)
                ok = 1;
            verify_args.error = err;
        } else {
            ok = 0;
            verify_args.error = X509_V_ERR_CERT_CHAIN_TOO_LONG;
        }
    }
    switch (err) {
    case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT:
        printf("issuer= ");
//        X509_NAME_print_ex(bio_err, X509_get_issuer_name(err_cert), 0, get_nameopt());
        break;
    case X509_V_ERR_CERT_NOT_YET_VALID:
    case X509_V_ERR_ERROR_IN_CERT_NOT_BEFORE_FIELD:
        printf("notBefore=");
//        ASN1_TIME_print(bio_err, X509_get0_notBefore(err_cert));
        break;
    case X509_V_ERR_CERT_HAS_EXPIRED:
    case X509_V_ERR_ERROR_IN_CERT_NOT_AFTER_FIELD:
//        BIO_printf(bio_err, "notAfter=");
//        ASN1_TIME_print(bio_err, X509_get0_notAfter(err_cert));
//        BIO_printf(bio_err, "\n");
        break;
    case X509_V_ERR_NO_EXPLICIT_POLICY:
//        if (!verify_args.quiet)
//            policies_print(ctx);
        break;
    }
//    if (err == X509_V_OK && ok == 2 && !verify_args.quiet)
//        policies_print(ctx);
    if (ok && !verify_args.quiet)
        printf("verify return:%d\n", ok);
    return ok;
}


const char *psk_identity = "PSK-IDENTITY";
const unsigned char psk_key[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1};


unsigned int psk_client_cb(SSL *ssl, const char *hint,
                                    char *identity, unsigned int max_identity_len,
                                    unsigned char *psk, unsigned int max_psk_len)
{
    UNUSED(ssl);

    int ret;

    if (hint)
        printf("PSK hint %s\n", hint);

    ret = snprintf(identity, max_identity_len, "%s", psk_identity);
    if (ret < 0 || (unsigned int)ret > max_identity_len)
        return 0;

    if (sizeof(psk_key) > max_psk_len)
        return 0;

    memcpy(psk, psk_key, sizeof(psk_key));

    return sizeof(psk_key);
}



unsigned int psk_server_cb(SSL *ssl, const char *identity,
                                     unsigned char *psk, unsigned int max_psk_len)
{
    UNUSED(ssl);

    if (identity)
        printf("PSK identity %s\n", identity);

    if (sizeof(psk_key) > max_psk_len)
        return 0;

    memcpy(psk, psk_key, sizeof(psk_key));

    return sizeof(psk_key);
}







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
    int verify_flags = SSL_VERIFY_NONE;
    if (verify)
        verify = SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT | SSL_VERIFY_CLIENT_ONCE;

    SSL_CTX_set_verify(self->ctx, verify_flags, verify_callback);
}


void ssl_set_psk(struct ssl *self, const char *identity, const char *psk)
{
    UNUSED(self);
    UNUSED(identity);
    UNUSED(psk);

    //    SSL_CTX_set_psk_client_callback(ctx, psk_client_cb);
    //    SSL_CTX_set_psk_server_callback(ctx, psk_server_cb);
}
