
#include <CUnit/Basic.h>
#include <CUnit/Automated.h>
#include <CUnit/Console.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>

#include "mx/log.h"
#include "mx/string.h"
#include "mx/misc.h"



static char cunit_result_file[] = "/tmp/mxc_test.cunit.log";



struct args
{
    bool automated;
    bool basic;
    bool console;
    char *modules;
    char *file;
    int verbose;
    unsigned int logger;
};

static void show_help(const char *name);
static bool args_parse(int argc, char *argv[], struct args *args);



struct test_module
{
    const char *name;
    CU_ErrorCode (*function)(void);
};


extern CU_ErrorCode cu_test_log();
extern CU_ErrorCode cu_test_http();
extern CU_ErrorCode cu_test_misc();
extern CU_ErrorCode cu_test_stream();
extern CU_ErrorCode cu_test_stream_mqtt();
extern CU_ErrorCode cu_test_stream_ws();
extern CU_ErrorCode cu_test_string();
extern CU_ErrorCode cu_test_timer();

const struct test_module test_modules[] = {
        {"log",             cu_test_log},
        {"http",            cu_test_http},
        {"misc",            cu_test_misc},
        {"stream",          cu_test_stream},
        {"stream_mqtt",     cu_test_stream_mqtt},
        {"stream_ws",       cu_test_stream_ws},
        {"string",          cu_test_string},
        {"timer",           cu_test_timer},
};


int main(int argc, char *argv[])
{
    struct args args;
    if (!args_parse(argc, argv, &args))
        return 1;

    log_init(deflogger, "cmx");
    deflogger->conf.cfgmask = args.logger;

    if ( CU_initialize_registry() != CUE_SUCCESS )
        return CU_get_error();

    for (unsigned int i=0; i<ARRAY_SIZE(test_modules); i++) {
        const struct test_module *module = &test_modules[i];
        if (!args.modules) {
            // Modules not defined run all test functions
            module->function();
        } else {
            // Modules defined, rull only requested modules
            struct xstrtok *tk = xstrtok_new(args.modules);
            const char *chunk;
            while (chunk = xstrtok_next(tk, ","), chunk) {
                if (xstrstarts(module->name, chunk))
                    module->function();
            }
            xstrtok_delete(tk);
        }
    }

    if (args.basic) {
        /* Run all tests using the CUnit Basic interface */
        CU_basic_set_mode(args.verbose);
        CU_basic_run_tests();
    }

    if (args.automated) {
        /* Run all tests using the CUnit Automated interface */
        if (args.file != NULL)
            CU_set_output_filename(args.file);
        else
            CU_set_output_filename(cunit_result_file);
        CU_list_tests_to_file();
        CU_automated_run_tests();
    }

    if (args.console) {
        /* Run all tests using the CUnit Console interface */
        CU_console_run_tests();
    }

    if (args.file != NULL)
        free(args.file);
    if (args.modules != NULL)
        free(args.modules);
    CU_cleanup_registry();
    return CU_get_error();
}



static struct option options[] =
{
    {"auto",    no_argument,        0,  'a'},
    {"basic",   no_argument,        0,  'b'},
    {"console", no_argument,        0,  'c'},
    {"verbose", required_argument,  0,  'v'},
    {"modules", required_argument,  0,  'm'},
    {"file",    required_argument,  0,  'f'},
    {"logger",  required_argument,  0,  'l'},
    {"help",    no_argument,        0,  'h'},
    {0,         0,                  0,   0}
};


bool args_parse(int argc, char *argv[], struct args *args)
{
    int c;
    int option_idx = 0;
    bool success = true;

    args->verbose = CU_BRM_NORMAL;
    args->automated = false;
    args->basic = false;
    args->console = false;
    args->modules = NULL;
    args->file = NULL;
    args->logger = LOG_DEFAULT_VALUE;

    while (1) {
        long val;
        char *end = NULL;
        c = getopt_long(argc, argv, "abchv:f:l:m:", options, &option_idx);
        if (c == -1)
            break;

        switch (c) {
        case 'a':
            args->automated = true;
            break;
        case 'b':
            args->basic = true;
            break;
        case 'c':
            args->console = true;
            break;

        case 'm':
            args->modules = xstrdup(optarg);
            break;

        case 'v':
            end = NULL;
            val = strtol(optarg, &end, 10);
            if (*end != '\0') {
                ERROR("Could not convert verbose value '%s'\n", optarg);
                success = false;
                break;
            }
            switch (val) {
            case 0:
                args->verbose = CU_BRM_SILENT;
                break;
            case 1:
                // args->verbose = CU_BRM_NORMAL;
                break;
            case 2:
                args->verbose = CU_BRM_VERBOSE;
                break;
            default:
                ERROR("Verbose value not supported, using default\n");
            }
            break;

        case 'f':
            args->file = strdup(optarg);
            break;

        case 'l':
            end = NULL;
            val = strtol(optarg, &end, 16);
            if (*end != '\0') {
                ERROR("Could not convert log value '%s'\n", optarg);
                success = false;
                break;
            }
            args->logger = (unsigned int)val;
            break;

        case 'h':
        case '?':
            show_help(argv[0]);
            success = false;
            break;

        default:
            ERROR("Could not parse arguments\n");
            success = false;
            break;
        }
    }

    if (!args->basic && !args->console)
        args->automated = true;

    if (!args->modules) {
        const char *modules = getenv("CU_TEST_MODULES");
        if (modules) {
            INFO("Test modules defined by env - %s", modules);
            args->modules = xstrdup(modules);
        }
    }

    if (!success && args->file != NULL) {
        free(args->file);
        args->file = NULL;
    }

    return success;
}


void show_help(const char *name)
{
    printf("\nUsage: %s [options] ...\n", name);

    printf("\nOptions:\n");
    printf("  -a  --auto        cunit automated mode, default\n");
    printf("  -b  --basic       cunit basic mode\n");
    printf("  -c  --console     cunit interactive console mode\n");
    printf("  -m  --modules     set of test modules to run");
    printf("  -v  --verbose     set verbose value [0-silent,1-normal,2-verbose]\n");
    printf("  -f  --file        output file when automated mode is set\n");
    printf("  -l  --logger      set logger value\n");
    printf("  -h  --help        help\n");

    printf("\nExamples:\n");
    printf("  %s --basic --verbos=2 --logger=0xFFFF\n", name);
    printf("  %s -b -a -v0 -f \"/tmp/cunit-result\"\n", name);
    printf("  %s --console\n", name);
    printf("\n");
}
