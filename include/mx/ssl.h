
#ifndef __MX_SSL_H_
#define __MX_SSL_H_


#include <stdbool.h>
#include <stddef.h>


struct ssl;


struct ssl* ssl_new(void);
struct ssl* ssl_delete(struct ssl *self);



void ssl_set_certfile(struct ssl *self, const char *certfile, const char *keyfile);
void ssl_set_cafile(struct ssl *self, const char *cafile);

void ssl_set_verify_peer(struct ssl *self, bool verify);

/*
 * Configure a context once, before creating streams, to require a TLS 1.3
 * external PSK with SHA-256. Certificate fallback is disabled.
 * The context copies the identity string (1-255 bytes) and binary key (16-48 bytes).
 * Returns 0 on success or -1 with errno set (ENOTSUP without TLS 1.3 PSK support).
 */
int ssl_set_psk(struct ssl *self, const char *identity, const unsigned char *key, size_t key_len);


#endif /* __MX_SSL_H_ */
