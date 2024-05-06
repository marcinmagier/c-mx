
#include "test.h"

#include <CUnit/Basic.h>

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>




logger_t testlogger = {
    .conf.cfgmask = LOG_DEFAULT_VALUE,
    .name = "cutest",
    .file = "/tmp/cmx-test.log"

};


#define TEST_RESET(...)    _RESET_(&testlogger, __VA_ARGS__)
#define TEST_ERROR(...)    _ERROR_(&testlogger, __VA_ARGS__)
#define TEST_WARN(...)     _WARN_(&testlogger, __VA_ARGS__)
#define TEST_INFO(...)     _INFO_(&testlogger, __VA_ARGS__)
#define TEST_DEBUG(...)    _DEBUG_(&testlogger, LOG_DEFAULT_DEBUGTYPE, __VA_ARGS__)





static int suite_init_default(void);
static int suite_clean_default(void);

static int suite_init_file(void);
static int suite_clean_file(void);

static int suite_init_syslog(void);
static int suite_clean_syslog(void);


static void test_default_debug(void);
static void test_default_info(void);
static void test_default_warning(void);
static void test_default_error(void);
static void test_default_reset(void);

static void test_private_debug(void);
static void test_private_info(void);
static void test_private_warning(void);
static void test_private_error(void);
static void test_private_reset(void);

static void test_internal_errors(void);



CU_ErrorCode cu_test_log()
{
    // Test logging to terminal
    CU_pSuite suite = CU_add_suite("Console suite", suite_init_default, suite_clean_default);
    if ( !suite ) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    CU_add_test(suite, "Test default console debug",       test_default_debug);
    CU_add_test(suite, "Test default console info",        test_default_info);
    CU_add_test(suite, "Test default console warn",        test_default_warning);
    CU_add_test(suite, "Test default console error",       test_default_error);
    CU_add_test(suite, "Test default console reset",       test_default_reset);

    CU_add_test(suite, "Test private console debug",       test_private_debug);
    CU_add_test(suite, "Test private console info",        test_private_info);
    CU_add_test(suite, "Test private console warn",        test_private_warning);
    CU_add_test(suite, "Test private console error",       test_private_error);
    CU_add_test(suite, "Test private console reset",       test_private_reset);


    // Test logging to file
    suite = CU_add_suite("File suite", suite_init_file, suite_clean_file);
    if ( !suite ) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    CU_add_test(suite, "Test default file debug",           test_default_debug);
    CU_add_test(suite, "Test default file info",            test_default_info);
    CU_add_test(suite, "Test default file warn",            test_default_warning);
    CU_add_test(suite, "Test default file error",           test_default_error);
    CU_add_test(suite, "Test default file reset",           test_default_reset);

    CU_add_test(suite, "Test private file debug",           test_private_debug);
    CU_add_test(suite, "Test private file info",            test_private_info);
    CU_add_test(suite, "Test private file warn",            test_private_warning);
    CU_add_test(suite, "Test private file error",           test_private_error);
    CU_add_test(suite, "Test private file reset",           test_private_reset);


    // Test logging to syslog
    suite = CU_add_suite("Syslog suite", suite_init_syslog, suite_clean_syslog);
    if ( !suite ) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    CU_add_test(suite, "Test default syslog debug",         test_default_debug);
    CU_add_test(suite, "Test default syslog info",          test_default_info);
    CU_add_test(suite, "Test default syslog warn",          test_default_warning);
    CU_add_test(suite, "Test default syslog error",         test_default_error);
    CU_add_test(suite, "Test default syslog reset",         test_default_reset);

    CU_add_test(suite, "Test private syslog debug",         test_private_debug);
    CU_add_test(suite, "Test private syslog info",          test_private_info);
    CU_add_test(suite, "Test private syslog warn",          test_private_warning);
    CU_add_test(suite, "Test private syslog error",         test_private_error);
    CU_add_test(suite, "Test private syslog reset",         test_private_reset);

    CU_add_test(suite, "Test logger internal errors",       test_internal_errors);

    return CU_get_error();
}



int suite_init_default(void)
{
    LOG("\n\n Console logger");
    log_init(deflogger, "cmx");
    log_init(&testlogger, "cmx");
    deflogger->reset_result = 0;    // Do not exit
    testlogger.conf.bits.timestamp = 1;
    testlogger.reset_result = 0;    // Do not exit
    return 0;
}

int suite_clean_default(void)
{
    LOG("\n Console logger done");
    // Restore default logger
    log_init(deflogger, "cmx");
    return 0;
}


int suite_init_file(void)
{
    LOG("\n\n File logger");

    log_init(deflogger, "cmx");
    deflogger->conf.bits.console = 0;
    deflogger->conf.bits.file = 1;
    deflogger->reset_result = 0;    // Do not exit

    log_init(&testlogger, "cmx");
    testlogger.conf.bits.console = 0;
    testlogger.conf.bits.file = 1;
    testlogger.conf.bits.timestamp = 1;
    testlogger.reset_result = 0;    // Do not exit

    return 0;
}

int suite_clean_file(void)
{
    LOG("\n File logger done");
    // Restore default logger
    log_init(deflogger, "cmx");
    return 0;
}


int suite_init_syslog(void)
{
    LOG("\n\n Syslog logger");
    log_init(deflogger, "cmx");

    deflogger->conf.bits.console = 0;
    deflogger->conf.bits.file = 0;
    deflogger->conf.bits.syslog = 1;
    deflogger->reset_result = 0;    // Do not exit

    log_init(&testlogger, "cmx");
    testlogger.conf.bits.console = 0;
    testlogger.conf.bits.file = 0;
    testlogger.conf.bits.syslog = 1;
    testlogger.conf.bits.timestamp = 1;
    testlogger.reset_result = 0;    // Do not exit

    return 0;
}

int suite_clean_syslog(void)
{
    LOG("\n Syslog logger done");
    // Restore default logger
    log_init(deflogger, "cmx");
    return 0;
}



/*
 *  Test default logger
 *
 */

void test_default_debug(void)
{
    deflogger->conf.bits.verbosity = DEBUG_LEVEL;

    LOG("\n Test default DEBUG_LEVEL");

    DEBUG("Test default debug");
    INFO("Test default info");
    WARN("Test default warning");
    ERROR("Test default error");
    RESET("Test default reset");
}

void test_default_info(void)
{
    deflogger->conf.bits.verbosity = INFO_LEVEL;

    LOG("\n Test default INFO_LEVEL");

    DEBUG("Test default debug");
    INFO("Test default info");
    WARN("Test default warning");
    ERROR("Test default error");
    RESET("Test default reset");
}

void test_default_warning(void)
{
    deflogger->conf.bits.verbosity = WARNING_LEVEL;

    LOG("\n Test default WARNING_LEVEL");

    DEBUG("Test default debug");
    INFO("Test default info");
    WARN("Test default warning");
    ERROR("Test default error");
    RESET("Test default reset");
}

void test_default_error(void)
{
    deflogger->conf.bits.verbosity = ERROR_LEVEL;

    LOG("\n Test default ERROR_LEVEL");

    DEBUG("Test default debug");
    INFO("Test default info");
    WARN("Test default warning");
    ERROR("Test default error");
    RESET("Test default reset");
}

void test_default_reset(void)
{
    deflogger->conf.bits.verbosity = RESET_LEVEL;

    LOG("\n Test default RESET_LEVEL");

    DEBUG("Test default debug");
    INFO("Test default info");
    WARN("Test default warning");
    ERROR("Test default error");
    RESET("Test default reset");
}



/*
 *  Test private logger
 *
 */

void test_private_debug(void)
{
    testlogger.conf.bits.verbosity = DEBUG_LEVEL;

    LOG("\n Test private DEBUG_LEVEL");

    TEST_DEBUG("Test private debug");
    TEST_INFO("Test private info");
    TEST_WARN("Test private warning");
    TEST_ERROR("Test private error");
    TEST_RESET("Test private reset");
}

void test_private_info(void)
{
    testlogger.conf.bits.verbosity = INFO_LEVEL;
    testlogger.reset_result = 0;    // Do not exit

    LOG("\n Test private INFO_LEVEL");

    TEST_DEBUG("Test private debug");
    TEST_INFO("Test private info");
    TEST_WARN("Test private warning");
    TEST_ERROR("Test private error");
    TEST_RESET("Test private reset");
}

void test_private_warning(void)
{
    testlogger.conf.bits.verbosity = WARNING_LEVEL;
    testlogger.reset_result = 0;    // Do not exit

    LOG("\n Test private WARNING_LEVEL");

    TEST_DEBUG("Test private debug");
    TEST_INFO("Test private info");
    TEST_WARN("Test private warning");
    TEST_ERROR("Test private error");
    TEST_RESET("Test private reset");
}

void test_private_error(void)
{
    testlogger.conf.bits.verbosity = ERROR_LEVEL;
    testlogger.reset_result = 0;    // Do not exit

    LOG("\n Test private ERROR_LEVEL");

    TEST_DEBUG("Test private debug");
    TEST_INFO("Test private info");
    TEST_WARN("Test private warning");
    TEST_ERROR("Test private error");
    TEST_RESET("Test private reset");
}

void test_private_reset(void)
{
    testlogger.conf.bits.verbosity = RESET_LEVEL;
    testlogger.reset_result = 0;    // Do not exit

    LOG("\n Test private RESET_LEVEL");

    TEST_DEBUG("Test private debug");
    TEST_INFO("Test private info");
    TEST_WARN("Test private warning");
    TEST_ERROR("Test private error");
    TEST_RESET("Test private reset");
}



/**
 * Test internal logger errors
 *
 */
void test_internal_errors(void)
{
    log_init(&testlogger, "cmx");

    testlogger.file = "/invalid/log/file";
    testlogger.conf.bits.file = 1;

    TEST_ERROR("Test invalid log file")
}
