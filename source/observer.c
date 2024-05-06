
#include "mx/observer.h"
#include "mx/memory.h"
#include "mx/misc.h"



/**
 * Initialize object
 *
 */
void observer_init(struct observer *self)
{
    self->object = NULL;
    self->notify_type = 0;
    self->notify_handler = NULL;
}


/**
 * Clean object
 *
 */
void observer_clean(struct observer *self)
{
    UNUSED(self);
}


/**
 * Constructor
 *
 */
struct observer* observer_new(void)
{
    struct observer *self = xmalloc(sizeof(struct observer));
    observer_init(self);
    return self;
}


/**
 *  Constructor with arguments
 *
 */
struct observer* observer_create(void *object, int type, observer_notify_fn handler)
{
    struct observer *self = observer_new();
    self->object = object;
    self->notify_type = type;
    self->notify_handler = handler;
    return self;
}


/**
 * Destructor
 *
 */
struct observer* observer_delete(struct observer *self)
{
    observer_clean(self);
    return xfree(self);
}


/**
 * Check if observer is available and ready
 */
bool observer_is_available(struct observer *self)
{
    if (self && self->notify_handler)
        return true;

    return false;
}





/**
 * Initialize object
 *
 */
void observer_list_init(struct observer_list *self)
{
    LIST_INIT(&self->items);
}


/**
 * Clean object
 *
 */
void observer_list_clean(struct observer_list *self)
{
    struct observer *observer, *tmp;
    LIST_FOREACH_SAFE(observer, &self->items, _entry_, tmp) {
        LIST_REMOVE(observer, _entry_);
        observer_delete(observer);
    }
}


/**
 * Constructor
 *
 */
struct observer_list* observer_list_new(void)
{
    struct observer_list *self = xmalloc(sizeof(struct observer_list));
    observer_list_init(self);
    return self;
}


/**
 * Destructor
 *
 */
struct observer_list* observer_list_delete(struct observer_list *self)
{
    observer_list_clean(self);
    return xfree(self);
}


/**
 * Add observer
 *
 */
void observer_list_add_observer(struct observer_list *self, struct observer *observer)
{
    LIST_INSERT_HEAD(&self->items, observer, _entry_);
}


/**
 * Remove observer
 *
 */
void observer_list_remove_observer(struct observer_list *self, struct observer *observer)
{
    UNUSED(self);
    LIST_REMOVE(observer, _entry_);
}


/**
 * Find matching observer
 *
 */
struct observer* observer_list_find_observer(struct observer_list *self, void *object, int notify_type, observer_notify_fn handler)
{
    struct observer *tmp;
    LIST_FOREACH(tmp, &self->items, _entry_) {
        if (tmp->object != object)
            continue;
        if ((notify_type > 0) && (tmp->notify_type != notify_type))
            continue;
        if (handler && (tmp->notify_handler != handler))
            continue;

        return tmp;
    }

    return NULL;
}
