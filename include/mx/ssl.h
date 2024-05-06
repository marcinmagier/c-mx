
#ifndef __MX_SSL_H_
#define __MX_SSL_H_


#include <stdbool.h>


struct ssl;


struct ssl* ssl_new(void);
struct ssl* ssl_delete(struct ssl *self);



void ssl_set_certfile(struct ssl *self, const char *certfile, const char *keyfile);
void ssl_set_cafile(struct ssl *self, const char *cafile);

void ssl_set_verify_peer(struct ssl *self, bool verify);


#endif /* __MX_SSL_H_ */
