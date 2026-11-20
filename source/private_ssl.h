
#ifndef __MX_PRIVATE_SSL_H_
#define __MX_PRIVATE_SSL_H_


#include <stdbool.h>

#include <openssl/ssl.h>
#include <openssl/bio.h>

#include "private_openssl_compat.h"


// BIO_TYPE_STREAM_SSL is similar to BIO_TYPE_SOCKET
// # define BIO_TYPE_SOCKET         ( 5|BIO_TYPE_SOURCE_SINK|BIO_TYPE_DESCRIPTOR)
#define BIO_TYPE_STREAM_SSL         ( 55|BIO_TYPE_SOURCE_SINK|BIO_TYPE_DESCRIPTOR)



#if OPENSSL_VERSION_NUMBER >= 0x10101000L && defined(TLS1_3_VERSION) && \
    !defined(OPENSSL_NO_TLS1_3) && !defined(OPENSSL_NO_PSK) && !defined(LIBRESSL_VERSION_NUMBER)
#define MX_SSL_HAS_TLS13_PSK 1
#endif


struct ssl
{
    SSL_CTX *ctx;
    bool psk_configured;
};



const BIO_METHOD *BIO_meth_new_stream_ssl(void);
void BIO_meth_free_stream_ssl(void);


#endif /* __MX_PRIVATE_SSL_H_ */
