
#include "mx/listener.h"

#include "mx/memory.h"
#include "mx/string.h"
#include "mx/stream.h"
#include "mx/socket.h"
#include "mx/net.h"
#include "mx/misc.h"
#include "mx/log.h"
#include "mx/url.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <netinet/in.h>
#include <arpa/inet.h>



void listener_init(struct listener *self, const char *address, unsigned int port)
{
    self->address = address ? xstrdup(address) : NULL;
    self->port = port;

    self->fd = -1;
    self->stream = NULL;

    self->ssl = false;
    self->websocket = false;

    self->bridge = false;
    self->keep_alive = 0;
    self->max_connections = 0;
}


void listener_clean(struct listener *self)
{
    listener_stop(self);

    if (self->address)
        self->address = xfree(self->address);

    if (self->stream)
        self->stream = stream_delete(self->stream);
}


struct listener* listener_new(const char *address, unsigned int port)
{
    struct listener *self = xmalloc(sizeof(struct listener));
    listener_init(self, address, port);
    return self;
}


struct listener* listener_new_from_url(const char *url)
{
    struct url_parser parser = {0};
    struct listener *self = NULL;
    char *buffer = NULL;

    do {
        unsigned int port = 0;
        bool ssl = false;
        bool websocket = false;
        if (!url_parse(&parser, url)) {
            ERROR("Could not create listener from '%s'", url);
            break;
        }
        if (parser.scheme) {
            buffer = xstrndupex(buffer, parser.scheme, parser.scheme_len);
            bool success = url_parse_scheme(buffer, &port, &ssl);
            if (!success) {
                ERROR("Unknown listener url scheme '%s'", parser.scheme);
                break;
            }
            websocket = xstrstarts(parser.scheme, "ws");
        }
        if (parser.port) {
            long parsed_port;
            buffer = xstrndupex(buffer, parser.port, parser.port_len);
            bool success = xstrtol(buffer, &parsed_port, 10);
            if (!success || parsed_port < 1 || parsed_port > 65535) {
                ERROR("Invalid listener url port '%s'", parser.port);
                break;
            }
            port = (unsigned int)parsed_port;
        }
        if (parser.host)
            buffer = xstrndupex(buffer, parser.host, parser.host_len);
        else
            buffer = xstrndupex(buffer, parser.path, parser.path_len);

        self = listener_new(buffer, port);
        self->ssl = ssl;
        self->websocket = websocket;

    } while (0);

    if (buffer)
        xfree(buffer);

    return self;
}


struct listener* listener_delete(struct listener *self)
{
    listener_clean(self);
    return xfree(self);
}


bool listener_is_unix(const struct listener *self)
{
    return self->port == 0;
}


int listener_start(struct listener *self)
{
    int fd = -1;

    if (listener_is_unix(self)) {
        fd = socket_create_unix_stream();
        if (fd == -1)
            return -1;
        if (socket_listen_unix(fd, self->address) == -1) {
            socket_close(fd);
            return -1;
        }
        INFO("Listening on %s", self->address);
    }
    else {
        fd = strchr(self->address, ':') ? socket_create_inet6_stream()
                                        : socket_create_inet_stream();
        if (fd == -1)
            return -1;
        socket_set_reuse_addr(fd, 1);
        socket_set_tcp_no_delay(fd, 1);
        if (socket_listen_inet(fd, self->address, self->port) == -1) {
            socket_close(fd);
            return -1;
        }
        INFO("Listening on %s:%u", self->address, self->port);
    }

    self->fd = fd;
    self->stream = stream_new(fd);

    return fd;
}


void listener_stop(struct listener *self)
{
    if (self->fd >= 0) {
        self->fd = socket_close(self->fd);
        if (listener_is_unix(self))
            unlink(self->address);
    }

    if (self->stream)
        self->stream = stream_delete(self->stream);
}


int listener_accept(struct listener *self)
{
    struct sockaddr_storage addr;
    socklen_t addrlen = sizeof(addr);

    int fd = accept(self->fd, (struct sockaddr *)&addr, &addrlen);
    if (fd == -1) {
        ERROR("Cannot accept new client");
        return -1;
    }

    if (listener_is_unix(self)) {
        INFO("New local client with %d fd connected", fd);
    }
    else {
        char addrstr[INET6_ADDRSTRLEN] = {0};
        if (!net_addr_to_str((struct sockaddr *)&addr, addrstr, sizeof(addrstr)))
            strcpy(addrstr, "unknown");
        INFO("New client with %d fd connected from %s", fd, addrstr);
    }

    socket_set_non_blocking(fd, 1);

    return fd;
}





void listener_list_init(struct listener_list *self)
{
    LIST_INIT(&self->items);
}


void listener_list_clean(struct listener_list *self)
{
    struct listener *l, *tmp;
    LIST_FOREACH_SAFE(l, &self->items, _entry_, tmp) {
        LIST_REMOVE(l, _entry_);
        listener_delete(l);
    }
}


struct listener_list* listener_list_new(void)
{
    struct listener_list *self = xmalloc(sizeof(struct listener_list));
    listener_list_init(self);
    return self;
}


struct listener_list* listener_list_delete(struct listener_list *self)
{
    listener_list_clean(self);
    return xfree(self);
}


void listener_list_add(struct listener_list *self, struct listener *l)
{
    LIST_INSERT_HEAD(&self->items, l, _entry_);
}


void listener_list_rm(struct listener_list *self, struct listener *l)
{
    UNUSED(self);

    LIST_REMOVE(l, _entry_);
}


struct listener* listener_list_find(struct listener_list *self, const char *address, unsigned int port)
{
    struct listener *l, *tmp;
    LIST_FOREACH_SAFE(l, &self->items, _entry_, tmp) {
        if (l->address && !strcmp(address, l->address) && (port == l->port))
            return l;
    }

    return NULL;
}


bool listener_list_is_empty(struct listener_list *self)
{
    return LIST_EMPTY(&self->items) ? true : false;
}
