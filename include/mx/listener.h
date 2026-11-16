
#ifndef __MX_LISTENER_H_
#define __MX_LISTENER_H_



#include "mx/queue.h"

#include <stdbool.h>


struct stream;

struct listener
{
    char *address;
    unsigned int port;

    bool ssl;
    bool websocket;

    int fd;
    struct stream *stream;

    bool bridge;
    unsigned int keep_alive;
    unsigned int max_connections;

    LIST_ENTRY(listener) _entry_;
};


void listener_init(struct listener *self, const char *address, unsigned int port);
void listener_clean(struct listener *self);

struct listener* listener_new(const char *address, unsigned int port);
struct listener* listener_new_from_url(const char *url);
struct listener* listener_delete(struct listener *self);

bool listener_is_unix(const struct listener *self);


// Returns listening socket fd or -1 on error.
int listener_start(struct listener *self);
void listener_stop(struct listener *self);

// Returns accepted non-blocking socket fd or -1 on error.
int listener_accept(struct listener *self);



struct listener_list
{
    LIST_HEAD(listener_list_head, listener) items;
};


void listener_list_init(struct listener_list *self);
void listener_list_clean(struct listener_list *self);

struct listener_list* listener_list_new(void);
struct listener_list* listener_list_delete(struct listener_list *self);

void listener_list_add(struct listener_list *self, struct listener *l);
void listener_list_rm(struct listener_list *self, struct listener *l);
struct listener* listener_list_find(struct listener_list *self, const char *address, unsigned int port);
bool listener_list_is_empty(struct listener_list *self);


#define listener_list_for_each(var, head) LIST_FOREACH(var, &head->items, _entry_)
#define listener_list_for_each_safe(var, head, tvar) LIST_FOREACH_SAFE(var, &head->items, _entry_, tvar)



#endif /* __MX_LISTENER_H_ */
