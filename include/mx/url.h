
#ifndef __MX_URL_H_
#define __MX_URL_H_


#include <stddef.h>
#include <stdbool.h>



struct url_parser
{
    const char *scheme;
    size_t scheme_len;

    const char *userinfo;
    size_t userinfo_len;

    const char *host;
    size_t host_len;

    const char *port;
    size_t port_len;

    const char *path;
    size_t path_len;

    const char *query;
    size_t query_len;
};




bool url_parse(struct url_parser *parser, const char *url);

bool url_parse_scheme(const char *scheme, unsigned int *port, bool *encrypted);


#endif /* __MX_URL_H_ */
