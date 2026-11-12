
#include "mx/timer.h"

#include <time.h>



static volatile time_t clock_ms = 0;
static volatile time_t clock_s = 0;
static volatile time_t clock_rtc = 0;



/**
 *  Update clock values
 *
 *  The millisecond counter wraps every ~50 days.
 *       ULONG   / 24 / 60 / 60 / 1000
 *  4294967295ms / 24 / 60 / 60 / 1000  =  49.71 days
 *
 *  After wrapping clock_s is delayed by 295ms. For high accuraccy use uint64_t type.
 *
 */
void clock_update(time_t ms, time_t rtc)
{
    if (ms) {
        clock_s += ((clock_ms % 1000) + ms)/1000;
        clock_ms += ms;
    }

    clock_rtc = rtc;
}


/**
 * Update clock values from system timers
 *
 */
void clock_update_sys(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    clock_s = ts.tv_sec;
    clock_ms = ts.tv_sec * 1000;
    clock_ms += ts.tv_nsec / 1000 / 1000;

    clock_rtc = time(NULL);
}


/**
 * Milliseconds getter
 *
 */
time_t clock_get_milis()
{
    return clock_ms;
}


/**
 * Seconds getter
 *
 */
time_t clock_get_seconds()
{
    return clock_s;
}


/**
 * Real time clock getter
 *
 */
time_t clock_get_rtc()
{
    return clock_rtc != 0 ? clock_rtc : clock_s;
}
