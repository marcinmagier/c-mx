
#include "mx/url.h"

#include <string.h>
#include <stdbool.h>


struct url_scheme
{
    const char *name;
    unsigned int port;
    bool encrypted;
};


const struct url_scheme schemes[] = {
        { "mqtts",  8883,   true},
        { "mqtt",   1883,   false},
        { "wss",    443,    true},
        { "ws",     80,     false},

        { NULL,     0,      false},     // End
};



/**
 * Convert scheme into port and endcryption
 *
 */
bool url_parse_scheme(const char *scheme, unsigned int *port, bool *encrypted)
{
    for (unsigned int i=0; ;i++) {
        const struct url_scheme *s = &schemes[i];
        if (s->name == NULL)
            break;

        if (!strncmp(scheme, s->name, strlen(s->name))) {
            if (port)
                *port = s->port;
            if (encrypted)
                *encrypted = s->encrypted;
            return true;
        }
    }

    return false;
}


/**
 * Parser url
 *
 * scheme://user@host:port/path?query
 *
 */
bool url_parse(struct url_parser *parser, const char *url)
{
    const char *tmp = url;
    const char *colon;
    const char *slash;

    // Find scheme
    colon = strstr(tmp, "://");
    if (!colon) {
        // No scheme found
        parser->scheme = NULL;
    }
    else {
        // Scheme available                         e.g. scheme://...
        parser->scheme = tmp;
        parser->scheme_len = colon - parser->scheme;
        tmp = colon + 3;    // Skip "://"
    }

    if (tmp[0] == '/') {
        // Its just a path                          e.g. /a/b/c... or scheme:/a/b/c...
        parser->userinfo = NULL;
        parser->host = NULL;
        parser->port = NULL;
        parser->path = tmp;
    }
    else {
        // Find userinfo
        const char *at = strchr(tmp, '@');
        if (!at) {
            // Userinfo not defined
            parser->userinfo = NULL;
        }
        else {
            // Userinfo available                   e.g. scheme://user@host...
            parser->userinfo = tmp;
            parser->userinfo_len = at - parser->userinfo;
            tmp = at + 1;       // Skip '@'
        }

        parser->host = tmp;
        // Find host and port
        colon = strchr(parser->host, ':');
        slash = strchr(parser->host, '/');
        if (!colon) {
            // Port not defined
            parser->port = NULL;
            if (slash) {
                parser->host_len = slash - parser->host;
                parser->path = slash;
            }
            else {
                // Path not defined                 e.g. scheme://user@host
                parser->host_len = strlen(parser->host);
                parser->path = NULL;
                parser->query = NULL;
                return true;
            }
        }
        else {
            // Port available                       e.g. scheme://user@host:port...
            parser->host_len = colon - parser->host;
            parser->port = colon + 1;   // Skip ':'
            if (slash) {
                parser->port_len = slash - parser->port;
                parser->path = slash;
            }
            else {
                // Path not defined                 e.g. scheme://user@host:port
                parser->port_len = strlen(parser->port);
                parser->path = NULL;
                parser->query = NULL;
                return true;
            }
        }
    }

    // Find query
    const char *question = strchr(parser->path, '?');
    if (!question) {
        // Query not defined                        e.g. scheme://user@host:port/a/b/c
        parser->path_len = strlen(parser->path);
        parser->query = NULL;
    }
    else {
        // Query available                          e.g. scheme://user@host:port/a/b/c?query
        parser->path_len = question - parser->path;
        parser->query = question + 1;       // Skip '?'
        parser->query_len = strlen(parser->query);
    }

    return true;
}
