
#ifndef __MX_OPENSSL_COMPAT_H_
#define __MX_OPENSSL_COMPAT_H_

#if (OPENSSL_VERSION_NUMBER < 0x10100000L)

static inline BIO_METHOD *BIO_meth_new(int type, const char *name)
{
	BIO_METHOD *biom = xcalloc(1, sizeof(BIO_METHOD));

	if (biom != NULL) {
		biom->type = type;
		biom->name = name;
	}
	return biom;
}

static inline BIO_METHOD *BIO_meth_free(BIO_METHOD *biom)
{
    return xfree(biom);
}

#define BIO_meth_set_write(b, f) (b)->bwrite = (f)
#define BIO_meth_set_read(b, f) (b)->bread = (f)
#define BIO_meth_set_puts(b, f) (b)->bputs = (f)
#define BIO_meth_set_ctrl(b, f) (b)->ctrl = (f)
#define BIO_meth_set_create(b, f) (b)->create = (f)
#define BIO_meth_set_destroy(b, f) (b)->destroy = (f)

#define BIO_set_init(b, val) (b)->init = (val)
#define BIO_set_data(b, val) (b)->ptr = (val)
#define BIO_set_shutdown(b, val) (b)->shutdown = (val)
#define BIO_get_init(b) (b)->init
#define BIO_get_data(b) (b)->ptr
#define BIO_get_shutdown(b) (b)->shutdown

#define TLS_method SSLv23_method

#define X509_getm_notBefore X509_get_notBefore
#define X509_getm_notAfter X509_get_notAfter


#define BIO_get_init(b) (b)->init

#endif

#endif /* __MX_OPENSSL_COMPAT_H_ */

