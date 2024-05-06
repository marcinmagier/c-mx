
#ifndef __MX_OBSERVER_H_
#define __MX_OBSERVER_H_


#include "mx/queue.h"

#include <stdbool.h>



typedef void (*observer_notify_fn)(void);



struct observer
{
    void *object;
    int notify_type;
    observer_notify_fn notify_handler;

    LIST_ENTRY(observer) _entry_;
};


void observer_init(struct observer *self);
void observer_clean(struct observer *self);

struct observer* observer_new(void);
struct observer* observer_create(void *object, int type, observer_notify_fn handler);
struct observer* observer_delete(struct observer *self);

bool observer_is_available(struct observer *self);




struct observer_list
{
    LIST_HEAD(oserver_head, observer) items;
};


void observer_list_init(struct observer_list *self);
void observer_list_clean(struct observer_list *self);

struct observer_list* observer_list_new(void);
struct observer_list* observer_list_delete(struct observer_list *self);

void observer_list_add_observer(struct observer_list *self, struct observer *observer);
void observer_list_remove_observer(struct observer_list *self, struct observer *observer);

struct observer* observer_list_find_observer(struct observer_list *self, void *object, int notify_type, observer_notify_fn handler);



#endif /* __MX_OBSERVER_H_ */
