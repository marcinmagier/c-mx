
#ifndef __MX_TIMER_H_
#define __MX_TIMER_H_



#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>



void clock_update(time_t ms, time_t chrono);
void clock_update_sys(void);

time_t clock_get_milis();
time_t clock_get_seconds();
time_t clock_get_chrono();



#define TIMER_MS        0                   // Timer milliseconds
#define TIMER_SEC       (0x1 << 0)          // Timer seconds
#define TIMER_CHRONO    (0x1 << 1)          // Timer seconds respecting hibernation

#define TIMER_UPDATE    (0x1 << 7)          // Timer automatic update on check



struct timer
{
    time_t start;
    time_t interval;
    uint8_t flags;
};





static inline time_t timer_tstamp(uint8_t flags)
{
    if (flags & TIMER_UPDATE)
        clock_update_sys();

    if (flags & TIMER_SEC)
        return clock_get_seconds();
    if (flags & TIMER_CHRONO)
        return clock_get_chrono();
    return clock_get_milis();
}


static inline void timer_start(struct timer *self, uint8_t flags, time_t interval)
{
    self->flags = flags;
    self->start = timer_tstamp(self->flags);
    self->interval = interval;
}


static inline void timer_stop(struct timer *self)
{
    self->start = 0;
    self->flags = 0;
    self->interval = 0;
}


static inline void timer_restart(struct timer *self)
{
    self->start = timer_tstamp(self->flags);
}


static inline bool timer_running(struct timer *self)
{
    return self->interval ? true : false;
}


static inline time_t timer_value(struct timer *self)
{
    return timer_tstamp(self->flags) - self->start;
}


static inline time_t timer_remaining(struct timer *self)
{
    time_t value = timer_value(self);
    return (value < self->interval) ? self->interval - value : 0;
}


static inline bool timer_expired(struct timer *self)
{
    if (self->interval == 0)
        return false;
    if (timer_value(self) < self->interval)
        return false;
    return true;
}



#endif /* __MX_TIMER_H_ */
