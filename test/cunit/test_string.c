
#include "test.h"

#include "mx/string.h"
#include "mx/memory.h"

#include <CUnit/Basic.h>

#include <string.h>



static void test_string_move(void);
static void test_string_copy(void);
static void test_string_duplicate(void);
static void test_string_conversion(void);
static void test_string_join(void);
static void test_string_trim(void);
static void test_string_starts_ends(void);
static void test_string_tokenize(void);



CU_ErrorCode cu_test_string()
{
    // Test logging to terminal
    CU_pSuite suite = CU_add_suite("Suite string", NULL, NULL);
    if ( !suite ) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    CU_add_test(suite, "Test string move",                          test_string_move);
    CU_add_test(suite, "Test string copy",                          test_string_copy);
    CU_add_test(suite, "Test string duplicate",                     test_string_duplicate);
    CU_add_test(suite, "Test string conversion",                    test_string_conversion);
    CU_add_test(suite, "Test string join",                          test_string_join);
    CU_add_test(suite, "Test string trim",                          test_string_trim);
    CU_add_test(suite, "Test string starts/ends",                   test_string_starts_ends);
    CU_add_test(suite, "Test string tokenize",                      test_string_tokenize);

    return CU_get_error();
}



static const char *hello = "hello";
static const size_t hello_len = 5;

static const char *world = "world";
static const size_t world_len = 5;

static const char *helloworld = "helloworld";
static const size_t helloworld_len = 10;



/**
 *  Test string move
 *
 */
void test_string_move(void)
{
    char buffer[128];
    char *first, *second;

    // Move string pinter
    first = buffer;
    second = NULL;
    second = xstrmove(&first);
    CU_ASSERT_PTR_NOT_NULL(second);
    CU_ASSERT_PTR_NULL(first);
}


/**
 *  Test string copy
 *
 */
void test_string_copy(void)
{
    char buffer[128];
    char *tmp;

    tmp = xstrcpy(buffer, hello);
    CU_ASSERT_STRING_EQUAL(buffer, hello);
    CU_ASSERT_PTR_EQUAL(tmp, buffer + hello_len);

    tmp = xstrncpy(buffer, world, 3);
    tmp = xstrncpy(tmp, world+3, 33);    // maxlen is greater than string length
    CU_ASSERT_NSTRING_EQUAL(buffer, world, world_len);
    CU_ASSERT_PTR_EQUAL(tmp, buffer + world_len);

    xstrlcpy(buffer, helloworld, hello_len+1);
    CU_ASSERT_STRING_EQUAL(buffer, hello);
}


/**
 *  Test string duplicate
 *
 */
void test_string_duplicate(void)
{
    char *tmp;

    tmp = xstrdup(hello);
    CU_ASSERT_PTR_NOT_NULL(tmp);
    CU_ASSERT_STRING_EQUAL(tmp, hello);
    xfree(tmp);

    tmp = xstrndup(helloworld, hello_len);
    CU_ASSERT_PTR_NOT_NULL(tmp);
    CU_ASSERT_STRING_EQUAL(tmp, hello);
    xfree(tmp);
}


/**
 *  Test string conversion
 *
 */
void test_string_conversion(void)
{
    long value;
    bool success;

    success = xstrtol("_", &value, 10);
    CU_ASSERT_FALSE(success);
    success = xstrtol("_100", &value, 10);
    CU_ASSERT_FALSE(success);
    success = xstrtol("100_", &value, 10);
    CU_ASSERT_FALSE(success);

    success = xstrtol("100", &value, 10);
    CU_ASSERT_TRUE(success);
    CU_ASSERT_EQUAL(value, 100);

    success = xstrtol("0x10", &value, 16);
    CU_ASSERT_TRUE(success);
    CU_ASSERT_EQUAL(value, 16);
    success = xstrtol("0x10", &value, 0);
    CU_ASSERT_TRUE(success);
    CU_ASSERT_EQUAL(value, 16);
}


/**
 *  Test string joining
 *
 */
void test_string_join(void)
{
    char *tmp;

    tmp = xstrjoin(hello, world, NULL);
    CU_ASSERT_PTR_NOT_NULL(tmp);
    CU_ASSERT_STRING_EQUAL(tmp, helloworld);
    xfree(tmp);

    tmp = xmalloc(1);
    tmp = xstrjoinex(tmp, "hello", " ", "world");
    CU_ASSERT_PTR_NOT_NULL(tmp);
    CU_ASSERT_STRING_EQUAL(tmp, "hello world");
    xfree(tmp);
}


/**
 *  Test string joining
 *
 */
void test_string_trim(void)
{
    char test[] = " \n\t\r helloworld \n\t\r ";
    char *tmp;

    tmp = (char *)xstrltrim(test);
    CU_ASSERT_PTR_NOT_NULL(tmp);
    CU_ASSERT_NSTRING_EQUAL(tmp, helloworld, helloworld_len)

    tmp = xstrrtrim(tmp);
    CU_ASSERT_PTR_NOT_NULL(tmp);
    CU_ASSERT_STRING_EQUAL(tmp, helloworld);
}


/**
 *  Test string starts/ends
 *
 */
void test_string_starts_ends(void)
{
    char test[] = "prefix_hello_world_postfix";
    bool status;

    status = xstrstarts(test, "prefix");
    CU_ASSERT_TRUE(status);
    status = xstrends(test, "postfix");
    CU_ASSERT_TRUE(status);

    status = xstrstarts(test, "invalid");
    CU_ASSERT_FALSE(status);
    status = xstrends(test, "invalid");
    CU_ASSERT_FALSE(status);
}


/**
 *  Test string tokenize
 *
 */
void test_string_tokenize(void)
{
    char test[] = "first;second;third";
    const char *tmp;

    struct xstrtok *tokener = xstrtok_new(test);
    CU_ASSERT_PTR_NOT_NULL(tokener);

    tmp = xstrtok_next(tokener, "/;");
    CU_ASSERT_PTR_NOT_NULL(tmp);
    CU_ASSERT_STRING_EQUAL(tmp, "first");

    tmp = xstrtok_next(tokener, "h;");
    CU_ASSERT_PTR_NOT_NULL(tmp);
    CU_ASSERT_STRING_EQUAL(tmp, "second");

    tmp = xstrtok_next(tokener, ";");
    CU_ASSERT_PTR_NOT_NULL(tmp);
    CU_ASSERT_STRING_EQUAL(tmp, "third");

    tokener = xstrtok_delete(tokener);
    CU_ASSERT_PTR_NULL(tokener);
}
