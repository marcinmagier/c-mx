
#include "mx/stream_ssl.h"
#include "mx/stream.h"

#include "mx/misc.h"
#include "mx/log.h"
#include "mx/memory.h"
#include "mx/queue.h"
#include "mx/misc.h"
#include "mx/ssl.h"

#include "private_stream.h"
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





static BIO_METHOD *bio_stream_ssl_method = NULL;





int stream_ssl_do_handshake(struct stream_ssl *self);



static void* stream_ssl_destructor_impl(struct stream *stream);
static ssize_t stream_ssl_read_impl(struct stream *stream, void *buffer, size_t length);
static ssize_t stream_ssl_write_impl(struct stream *stream, const void *buffer, size_t length);
static int     stream_ssl_flush_impl(struct stream *stream);
static int     stream_ssl_time_impl(struct stream *stream);


static const struct stream_vtable stream_ssl_vtable = {
        .destructor_fn = stream_ssl_destructor_impl,
        .read_fn = stream_ssl_read_impl,
        .write_fn = stream_ssl_write_impl,
        .flush_fn = stream_ssl_flush_impl,
        .time_fn = stream_ssl_time_impl,
};





struct stream_ssl
{
    struct stream stream;

    SSL *ssl;
    BIO *bio;
};



/**
 * SSL stream initializer
 *
 */
void stream_ssl_init(struct stream_ssl *self, struct ssl *ssl)
{
    self->ssl = SSL_new(ssl->ctx);
    self->bio = BIO_new(BIO_meth_new_stream_ssl());

    // Do not close stream from BIO
    BIO_set_shutdown(self->bio, 0);

    BIO_set_data(self->bio, self);
    BIO_set_init(self->bio, 1);

    SSL_set_bio(self->ssl, self->bio, self->bio);
}


/**
 * SSL stream cleaner
 */
void stream_ssl_clean(struct stream_ssl *self)
{
    if (self->ssl)
        SSL_free(self->ssl);

    // bio objects are freed by SSL_free()
}






/**
 * SSL stream constructor
 *
 */
struct stream_ssl* stream_ssl_new(struct ssl *ssl, struct stream *decorated)
{
    struct stream_ssl *self = xmalloc(sizeof(struct stream_ssl));
    self->stream.rtti = STREAM_SSL_CLS;
    self->stream.vtable = &stream_ssl_vtable;

    stream_init(&self->stream, -1, decorated);
    stream_ssl_init(self, ssl);
    return self;
}


/**
 * SSL stream destructor
 *
 */
struct stream_ssl* stream_ssl_delete(struct stream_ssl *self)
{
    // Call virtual destructor
    return self->stream.vtable->destructor_fn(&self->stream);
}





/**
 * Cast SSL stream to base class
 *
 */
struct stream* stream_ssl_to_stream(struct stream_ssl *self)
{
    return &self->stream;
}


/**
 * Cast SSL stream from base class
 *
 */
struct stream_ssl* stream_ssl_from_stream(struct stream *self)
{
    if (self->rtti == STREAM_SSL_CLS)
        return (struct stream_ssl*)self;

    return NULL;
}


/**
 * Return decorated stream
 *
 */
struct stream* stream_ssl_get_decorated(struct stream_ssl *self)
{
    return self->stream.decorated;
}



/**
 * SSL server mode
 *
 */
void stream_ssl_accept(struct stream_ssl *self)
{
    SSL_set_accept_state(self->ssl);
}


/**
 * SSL client mode
 *
 */
void stream_ssl_connect(struct stream_ssl *self)
{
    SSL_set_connect_state(self->ssl);
    stream_ssl_do_handshake(self);
}


/**
 * Renegotiate new handshake
 *
 * !!! TLS 1.3 does not support renegotiation !!!
 *
 */
void stream_ssl_renegotiate(struct stream_ssl *self)
{
    SSL_renegotiate(self->ssl);
}


















/**
 * BIO printer
 *
 */
static int print_bio_error(const char *str, size_t len, void *bp)
{
    UNUSED(len);
    UNUSED(bp);

    WARN("%s", str);
    return 0;
}


/**
 * SSL handshake handler
 *
 */
int stream_ssl_do_handshake(struct stream_ssl *self)
{
    STREAM_LOG("-- ssl:handshake");
    int ret = SSL_do_handshake(self->ssl);
    if (ret <= 0) {
        int ssl_error = SSL_get_error(self->ssl, ret);
        switch (ssl_error) {
            case SSL_ERROR_ZERO_RETURN:
                break;
            case SSL_ERROR_WANT_WRITE:
            case SSL_ERROR_WANT_READ:
                errno = EAGAIN;
                break;
            case SSL_ERROR_WANT_X509_LOOKUP:
                break;
            case SSL_ERROR_SYSCALL:
                ERROR("SSL handshake syscal error %d", ret);
                return 0;   // The same as disconnection
            case SSL_ERROR_SSL:
                ERR_print_errors_cb(print_bio_error, NULL);
                break;
            default:
                ERROR("Unexpected SSL handshake error %d", ssl_error);
        }
    }

    if (SSL_is_init_finished(self->ssl))
        stream_set_status(&self->stream, STREAM_ST_READY);

    return ret;
}


/**
 * SSL stream class read operation
 *
 */
ssize_t stream_ssl_do_read(struct stream_ssl *self, void *buffer, size_t length)
{
    int ret;

    if (!SSL_is_init_finished(self->ssl)) {
        ret = stream_ssl_do_handshake(self);
        if (ret <= 0)
            return ret;
    }
    if (SSL_is_init_finished(self->ssl)) {
        ret = SSL_read(self->ssl, buffer, length);
        STREAM_LOG("-- ssl:read %d", ret);
        if (ret > 0)
            return ret;
        int ssl_error = SSL_get_error(self->ssl, ret);
        switch (ssl_error) {
            case SSL_ERROR_ZERO_RETURN:
                return 0;
            case SSL_ERROR_NONE:
            case SSL_ERROR_WANT_WRITE:
            case SSL_ERROR_WANT_READ:
                break;
            case SSL_ERROR_SYSCALL:
                if (ret < 0)
                    ERROR("SSL read syscal error %d", ret);
                return ret;
            case SSL_ERROR_SSL:
                ERR_print_errors_cb(print_bio_error, NULL);
                return -1;
            default:
                ERROR("Unexpected SSL read error %d", ssl_error);
                return -1;
        }
    }

    errno = EAGAIN;
    return -1;
}


/**
 * SSL stream class write operation
 *
 */
ssize_t stream_ssl_do_write(struct stream_ssl *self, const void *buffer, size_t length)
{
    int ret;

    if (!SSL_is_init_finished(self->ssl)) {
        ret = stream_ssl_do_handshake(self);
        if (ret <= 0)
            return ret;
    }
    if (SSL_is_init_finished(self->ssl)) {
        ret = SSL_write(self->ssl, buffer, length);
        STREAM_LOG("-- ssl:write %d", ret);
        if (ret > 0)
            return ret;
        int ssl_error = SSL_get_error(self->ssl, ret);
        switch (ssl_error) {
            case SSL_ERROR_ZERO_RETURN:
                return 0;
            case SSL_ERROR_NONE:
            case SSL_ERROR_WANT_WRITE:
            case SSL_ERROR_WANT_READ:
                errno = EAGAIN;
                break;
            case SSL_ERROR_SYSCALL:
                ERROR("SSL write syscal error %d", ret);
                return 0;   // The same as disconnection
            case SSL_ERROR_SSL:
                ERR_print_errors_cb(print_bio_error, NULL);
                return -1;
            default:
                ERROR("Unexpected SSL write error %d", ssl_error);
                return -1;
        }

        return ret;
    }

    errno = EAGAIN;
    return -1;
}







////// Virtual function definitions for stream class

/**
 * SSL stream class destructor
 *
 */
void* stream_ssl_destructor_impl(struct stream *stream)
{
    stream_ssl_clean((struct stream_ssl*)stream);
    stream_clean(stream);

    return xfree(stream);
}


/**
 * SSL stream virtual read implementation
 *
 */
ssize_t stream_ssl_read_impl(struct stream *stream, void *buffer, size_t length)
{
    return stream_ssl_do_read((struct stream_ssl *)stream, buffer, length);
}


/**
 * SSL stream virtual write implementation
 *
 */
ssize_t stream_ssl_write_impl(struct stream *stream, const void *buffer, size_t length)
{
    return stream_ssl_do_write((struct stream_ssl*)stream, buffer, length);
}


/**
 * SSL stream virtual flush implementation
 *
 */
int stream_ssl_flush_impl(struct stream *stream)
{
    return stream_do_flush(stream);
}


/**
 * SSL stream virtual time handler implementation
 *
 */
int stream_ssl_time_impl(struct stream *stream)
{
    return stream_do_time(stream);
}







////// BIO stream ssl methods

/**
 *
 */
int bio_stream_ssl_new(BIO *b)
{
    BIO_set_init(b, 0);
    BIO_set_data(b, NULL);      // This will be set by stream_ssl constructor

    return 1;
}


/**
 *
 */
int bio_stream_ssl_free(BIO *b)
{
    if (!b)
        return 0;

    if (BIO_get_shutdown(b)) {
        if (BIO_get_init(b)) {
            struct stream_ssl *stream = BIO_get_data(b);
            if (stream) {
                stream_ssl_delete(stream);
                BIO_set_data(b, NULL);
            }
            BIO_set_init(b, 0);
        }
        BIO_free(b);
    }

    return 1;
}


/**
 *
 */
int bio_stream_ssl_read(BIO *b, char *out, int outl)
{
    if (out == NULL || outl == 0)
        return 0;

    struct stream_ssl *stream = BIO_get_data(b);
    if (stream == NULL)
        return 0;

    BIO_clear_retry_flags(b);

    int ret = stream_read(stream_ssl_get_decorated(stream), out, outl);
    if (ret > 0 && ret != outl) {
        BIO_set_retry_read(b);
    }
    else if (ret < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            BIO_set_retry_read(b);
    }

//    printf("RD: ");
//    for (int i=0; i<ret; i++)
//        printf("%02X ", (unsigned char)out[i]);
//    printf("\n");

    return ret;
}


/**
 *
 */
int bio_stream_ssl_write(BIO *b, const char *in, int inl)
{
    if (in == NULL || inl == 0)
        return 0;

    struct stream_ssl *stream = BIO_get_data(b);
    if (stream == NULL)
        return 0;

    int ret = stream_write(stream_ssl_get_decorated(stream), in, inl);

//    printf("WR: ");
//    for (int i=0; i<ret; i++)
//        printf("%02X ", (unsigned char)in[i]);
//    printf("\n");

    return ret;
}


/**
 *
 */
long bio_stream_ssl_ctrl(BIO *b, int cmd, long num, void *ptr)
{
    UNUSED(ptr);

    long ret = 1;

    struct stream_ssl *stream = BIO_get_data(b);
    if (stream == NULL)
        return 0;

    switch (cmd) {
    case BIO_CTRL_GET_CLOSE:
        ret = BIO_get_shutdown(b);
        break;
    case BIO_CTRL_SET_CLOSE:
        BIO_set_shutdown(b, (int)num);
        break;
    case BIO_CTRL_FLUSH:
        stream_flush(stream_ssl_to_stream(stream));
        ret = 1;    // Special treatment to avoid SSL_ERROR_SYSCALL
        break;
    case BIO_CTRL_DUP:
        ret = 1;    // Another special treatment
        break;
    default:
        ret = 0;
        break;
    }

    return ret;
}


/**
 *
 */
int bio_stream_ssl_puts(BIO *b, const char *str)
{
    return bio_stream_ssl_write(b, str, strlen(str));
}


/**
 *
 */
const BIO_METHOD *BIO_meth_new_stream_ssl(void)
{
    if (bio_stream_ssl_method == NULL) {
        bio_stream_ssl_method = BIO_meth_new(BIO_TYPE_STREAM_SSL, "stream_ssl");

        if (bio_stream_ssl_method) {
            BIO_meth_set_write(bio_stream_ssl_method, bio_stream_ssl_write);
            BIO_meth_set_read(bio_stream_ssl_method, bio_stream_ssl_read);
            BIO_meth_set_puts(bio_stream_ssl_method, bio_stream_ssl_puts);
            BIO_meth_set_ctrl(bio_stream_ssl_method, bio_stream_ssl_ctrl);
            BIO_meth_set_create(bio_stream_ssl_method, bio_stream_ssl_new);
            BIO_meth_set_destroy(bio_stream_ssl_method, bio_stream_ssl_free);
        }
    }

    return bio_stream_ssl_method;
}


/**
 *
 */
void BIO_meth_free_stream_ssl(void)
{
    if (bio_stream_ssl_method)
        BIO_meth_free(bio_stream_ssl_method);

    bio_stream_ssl_method = NULL;
}
