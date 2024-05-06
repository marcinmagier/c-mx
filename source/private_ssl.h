
#ifndef __MX_PRIVATE_SSL_H_
#define __MX_PRIVATE_SSL_H_


#include <openssl/ssl.h>
#include <openssl/bio.h>

#include "private_openssl_compat.h"


// BIO_TYPE_STREAM_SSL is similar to BIO_TYPE_SOCKET
// # define BIO_TYPE_SOCKET         ( 5|BIO_TYPE_SOURCE_SINK|BIO_TYPE_DESCRIPTOR)
#define BIO_TYPE_STREAM_SSL         ( 55|BIO_TYPE_SOURCE_SINK|BIO_TYPE_DESCRIPTOR)



struct ssl
{
    SSL_CTX *ctx;
};



const BIO_METHOD *BIO_meth_new_stream_ssl(void);
void BIO_meth_free_stream_ssl(void);


#endif /* __MX_PRIVATE_SSL_H_ */
