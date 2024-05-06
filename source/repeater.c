
#include "mx/repeater.h"
#include "mx/memory.h"
#include "mx/rand.h"
#include "mx/misc.h"




void repeater_init(struct repeater *self)
{
    self->counter = 0;
    self->limit = 0;
    self->timeout = 1;      // One second
    timer_stop(&self->timer);
}


void repeater_clean(struct repeater *self)
{
    UNUSED(self);
}


void repeater_reset(struct repeater *self)
{
    self->counter = 0;
    timer_stop(&self->timer);
}


struct repeater* repeater_new(void)
{
    struct repeater *self = xmalloc(sizeof(struct repeater));
    repeater_init(self);
    return self;
}


struct repeater* repeater_delete(struct repeater *self)
{
    repeater_clean(self);
    return xfree(self);
}


void repeater_set_limit(struct repeater *self, unsigned int limit)
{
    self->limit = limit;
}


void repeater_set_timeout(struct repeater *self, unsigned int timeout)
{
    self->timeout = timeout;
}


bool repeater_is_exhausted(struct repeater *self)
{
    if ((self->limit > 0) && (self->counter >= self->limit))
        return true;

    return false;
}


bool repeater_is_waiting(struct repeater *self)
{
    if (timer_running(&self->timer) && !timer_expired(&self->timer))
        return true;

    return false;
}


bool repeater_mark_attempt(struct repeater *self)
{
    self->counter++;

    return repeater_is_exhausted(self);
}


void repeater_wait(struct repeater *self)
{
    timer_start(&self->timer, TIMER_SEC, self->timeout);
}


void repeater_postpone(struct repeater *self, unsigned int timeout)
{
    self->timeout = timeout;
    repeater_wait(self);
}


void repeater_back_off(struct repeater *self, unsigned int max_timeout)
{
    rand_time_increase(&self->timeout, max_timeout);
    repeater_wait(self);
}

