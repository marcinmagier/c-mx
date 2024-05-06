
#include "test.h"

#include "mx/timer.h"

#include <CUnit/Basic.h>

#include <unistd.h>



static void test_timer_ms(void);
static void test_timer_sec(void);
static void test_timer_chrono(void);
static void test_timer_auto_update(void);



CU_ErrorCode cu_test_timer()
{
    // Test logging to terminal
    CU_pSuite suite = CU_add_suite("Suite timer", NULL, NULL);
    if ( !suite ) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    CU_add_test(suite, "Test timer ms",                             test_timer_ms);
    CU_add_test(suite, "Test timer sec",                            test_timer_sec);
    CU_add_test(suite, "Test timer chrono",                         test_timer_chrono);
    CU_add_test(suite, "Test timer auto update",                    test_timer_auto_update);

    return CU_get_error();
}



/**
 *  Test timer ms
 *
 */
void test_timer_ms(void)
{
    unsigned int time = 100*1000;
    clock_update(time, 0);

    bool status;
    time_t value;
    struct timer timer;

    // Timer stop
    timer_stop(&timer);
    status = timer_running(&timer);
    CU_ASSERT_FALSE(status);
    status = timer_expired(&timer);
    CU_ASSERT_FALSE(status);

    // Timer start
    timer_start(&timer, TIMER_MS, 100);
    status = timer_running(&timer);
    CU_ASSERT_TRUE(status);
    status = timer_expired(&timer);
    CU_ASSERT_FALSE(status);
    //
    value = timer_value(&timer);
    CU_ASSERT_EQUAL(value, 0);
    value = timer_remaining(&timer);
    CU_ASSERT_EQUAL(value, 100);
    //
    clock_update(50, 0);
    status = timer_expired(&timer);
    CU_ASSERT_FALSE(status);
    value = timer_value(&timer);
    CU_ASSERT_EQUAL(value, 50);
    value = timer_remaining(&timer);
    CU_ASSERT_EQUAL(value, 50);
    // Expired
    clock_update(50, 0);
    status = timer_expired(&timer);
    CU_ASSERT_TRUE(status);
    value = timer_value(&timer);
    CU_ASSERT_EQUAL(value, 100);
    value = timer_remaining(&timer);
    CU_ASSERT_EQUAL(value, 0);
}


/**
 *  Test timer sec
 *
 */
void test_timer_sec(void)
{
    unsigned int time = 100*1000;
    clock_update(time, 0);

    bool status;
    time_t value;
    struct timer timer;

    // Timer start
    timer_start(&timer, TIMER_SEC, 100);
    status = timer_running(&timer);
    CU_ASSERT_TRUE(status);
    status = timer_expired(&timer);
    CU_ASSERT_FALSE(status);
    //
    value = timer_value(&timer);
    CU_ASSERT_EQUAL(value, 0);
    value = timer_remaining(&timer);
    CU_ASSERT_EQUAL(value, 100);
    //
    clock_update(50*1000, 0);
    status = timer_expired(&timer);
    CU_ASSERT_FALSE(status);
    value = timer_value(&timer);
    CU_ASSERT_EQUAL(value, 50);
    value = timer_remaining(&timer);
    CU_ASSERT_EQUAL(value, 50);
    // Expired
    clock_update(50*1000, 0);
    status = timer_expired(&timer);
    CU_ASSERT_TRUE(status);
    value = timer_value(&timer);
    CU_ASSERT_EQUAL(value, 100);
    value = timer_remaining(&timer);
    CU_ASSERT_EQUAL(value, 0);
}


/**
 *  Test timer chrono
 *
 */
void test_timer_chrono(void)
{
    unsigned int time = 100*1000;
    clock_update(time, 1000);

    bool status;
    time_t value;
    struct timer timer;

    // Timer start
    timer_start(&timer, TIMER_CHRONO, 100);
    status = timer_running(&timer);
    CU_ASSERT_TRUE(status);
    status = timer_expired(&timer);
    CU_ASSERT_FALSE(status);
    //
    value = timer_value(&timer);
    CU_ASSERT_EQUAL(value, 0);
    value = timer_remaining(&timer);
    CU_ASSERT_EQUAL(value, 100);
    //
    clock_update(0, 1050);
    status = timer_expired(&timer);
    CU_ASSERT_FALSE(status);
    value = timer_value(&timer);
    CU_ASSERT_EQUAL(value, 50);
    value = timer_remaining(&timer);
    CU_ASSERT_EQUAL(value, 50);
    // Expired
    clock_update(0, 1100);
    status = timer_expired(&timer);
    CU_ASSERT_TRUE(status);
    value = timer_value(&timer);
    CU_ASSERT_EQUAL(value, 100);
    value = timer_remaining(&timer);
    CU_ASSERT_EQUAL(value, 0);
}


/**
 *  Test timer auto update
 *
 */
void test_timer_auto_update(void)
{
    bool status;
    time_t value;
    struct timer timer;

    // Timer start
    timer_start(&timer, TIMER_MS | TIMER_UPDATE, 100);
    status = timer_running(&timer);
    CU_ASSERT_TRUE(status);
    status = timer_expired(&timer);
    CU_ASSERT_FALSE(status);
    //
    value = timer_value(&timer);
    CU_ASSERT_EQUAL(value, 0);
    value = timer_remaining(&timer);
    CU_ASSERT_EQUAL(value, 100);
    //
    usleep(50*1000);
    status = timer_expired(&timer);
    CU_ASSERT_FALSE(status);
    value = timer_value(&timer);
    CU_ASSERT_TRUE(49 <= value && value <= 51);
    value = timer_remaining(&timer);
    CU_ASSERT_TRUE(49 <= value && value <= 51);
    // Expired
    usleep(50*1000);
    status = timer_expired(&timer);
    CU_ASSERT_TRUE(status);
    value = timer_value(&timer);
    CU_ASSERT_TRUE(99 <= value && value <= 101);
    value = timer_remaining(&timer);
    CU_ASSERT_EQUAL(value, 0);
}
