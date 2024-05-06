
#ifndef __MX_REPEATER_H_
#define __MX_REPEATER_H_

#include "mx/timer.h"

#include "stdbool.h"



struct repeater
{
    unsigned int counter;
    unsigned int limit;
    unsigned int timeout;
    struct timer timer;
};



void repeater_init(struct repeater *self);
void repeater_clean(struct repeater *self);
void repeater_reset(struct repeater *self);

struct repeater* repeater_new(void);
struct repeater* repeater_delete(struct repeater *self);

void repeater_set_limit(struct repeater *self, unsigned int limit);
void repeater_set_timeout(struct repeater *self, unsigned int timeout);


bool repeater_is_exhausted(struct repeater *self);
bool repeater_is_waiting(struct repeater *self);

bool repeater_mark_attempt(struct repeater *self);

void repeater_wait(struct repeater *self);
void repeater_postpone(struct repeater *self, unsigned int timeout);
void repeater_back_off(struct repeater *self, unsigned int max_timeout);


#endif /* __MX_REPEATER_H_ */
