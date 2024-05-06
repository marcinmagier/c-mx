
#include "mx/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/reboot.h>
#include <stdarg.h>
#include <stdbool.h>
#include <time.h>
#include <sys/time.h>

#include <syslog.h>


#ifndef LOGGING_FILE
  #define LOGGING_FILE      1
#endif
#ifndef LOGGING_SYSLOG
  #define LOGGING_SYSLOG    1
#endif

#define LOG_DATE_PATTERN        "[%y-%m-%d %H:%M:%S"
#define LOG_MS_PATTERN          ".%03u] "

#define LOG_BUFFER_DATE_LEN     (sizeof(LOG_DATE_PATTERN)+sizeof(LOG_MS_PATTERN)+1)
#define LOG_BUFFER_SYSLOG_LEN   256

#define LOG_COLOR_RESET     LOG_COLOR_RED_BOLD
#define LOG_COLOR_ERROR     LOG_COLOR_RED_BOLD
#define LOG_COLOR_WARN      LOG_COLOR_RED
#define LOG_COLOR_INFO      ""
#define LOG_COLOR_DEBUG     LOG_COLOR_GREEN



typedef struct
{
    const char *color;
    const char *stamp;
    const char *file;
    unsigned int line;
    int syslog_type;
    bool details;
} log_data_t;


logger_t liblogger = {
    .conf.cfgmask = LOG_DEFAULT_VALUE,
    .name = "mxc",
    .file = "/tmp/cmx-lib.log",
    .terminal = true
};

logger_t *deflogger = &liblogger;




/**
 *  Calculate current date
 *
 */
static bool calculate_datetime(char *buffer, int size)
{
    time_t raw_time;
    struct tm *local_time;

    time(&raw_time);
    local_time = localtime(&raw_time);
    if (strftime(buffer, size, LOG_DATE_PATTERN, local_time) == 0) {
        fprintf(stderr, LOG_COLOR_WARN "(WRN) Cannot convert timestamp" LOG_COLOR_OFF "\n");
        return false;
    }

    struct timeval tv;
    gettimeofday(&tv, NULL);
    buffer += sizeof(LOG_DATE_PATTERN)-1;
    size -= sizeof(LOG_DATE_PATTERN);
    snprintf(buffer, size, LOG_MS_PATTERN, (unsigned int)(tv.tv_usec/1000));

    return true;
}

#if LOGGING_FILE

/**
 *  Open log file
 *
 */
static FILE* logfile_open(const char *path)
{
    FILE *logfile = fopen(path, "a");
    if (!logfile) {
        fprintf(stderr, LOG_COLOR_WARN "(WRN) Cannot open log file %s" LOG_COLOR_OFF "\n", path);
        return 0;
    }
    return logfile;
}


/**
 *  Close log file
 *
 */
static void logfile_close(FILE *logfile)
{
    if (fflush(logfile) == EOF) {
        fprintf(stderr, LOG_COLOR_WARN "(WRN) Cannot flush log file" LOG_COLOR_OFF "\n");
        return;
    }
    fclose(logfile);
}

/**
 *  Log message to file
 *
 */
static void log_file(const logger_t *logger, const log_data_t *logdata, const char *fmt, va_list arg)
{
    char curr_time[LOG_BUFFER_DATE_LEN];

    if (calculate_datetime(curr_time, sizeof(curr_time))) {
        FILE *logfile = logfile_open(logger->file);
        if (!logfile)
            return;
        fprintf(logfile, "%s%s %s: ", curr_time, logdata->stamp, logger->name);
        vfprintf(logfile, fmt, arg);
        if (logdata->details) {
            fprintf(logfile, "\n" "%s%s %s:     ", curr_time, logdata->stamp, logger->name);
            fprintf(logfile, "%s : %u" "\n", logdata->file, logdata->line);
        }
        else {
            fprintf(logfile, "\n");
        }
        logfile_close(logfile);
    }
}
#endif // LOGGING_FILE


#if LOGGING_SYSLOG

/**
 *  Log message to syslog
 *
 */
static void log_syslog(const logger_t *logger, const log_data_t *logdata, const char *fmt, va_list arg)
{
    char buffer[LOG_BUFFER_SYSLOG_LEN];

    vsnprintf(buffer, sizeof(buffer), fmt, arg);
    openlog(logger->name, 0, LOG_DAEMON);
    syslog(logdata->syslog_type, "%s %s", logdata->stamp, buffer);
    if (logdata->details)
        syslog(logdata->syslog_type, "%s   %s : %u", logdata->stamp, logdata->file, logdata->line);
    closelog();
}

#endif // LOGGING_SYSLOG


/**
 *  Log message to console
 *
 */
static void log_console(const logger_t *logger, const log_data_t *logdata, const char *fmt, va_list arg)
{
    char curr_time[LOG_BUFFER_DATE_LEN];
    curr_time[0]='\0';

    if (logger->conf.bits.timestamp)
        calculate_datetime(curr_time, sizeof(curr_time));

    if (logger->terminal)
        fprintf(stderr, "%s", logdata->color);
    fprintf(stderr, "%s%s %s: ", curr_time, logdata->stamp, logger->name);
    vfprintf(stderr, fmt, arg);
    if (logdata->details) {
        fprintf(stderr, "\n" "%s%s %s:     ", curr_time, logdata->stamp, logger->name);
        fprintf(stderr, "%s : %u", logdata->file, logdata->line);
    }
    if (logger->terminal)
        fprintf(stderr, LOG_COLOR_OFF);
    fprintf(stderr, "\n");
}


/**
 * Initialize logger
 *
 * Name must be persistent when using logger.
 *
 */
void log_init(logger_t *logger, const char *name)
{
    logger->name = name;
    logger->conf.bits.verbosity = INFO_LEVEL;
    logger->conf.bits.console = 1;
    logger->conf.bits.debugtype = 0xFF;

    if (isatty(fileno(stderr)) != 1)
        logger->terminal = false;

    logger->reset_result = 1;
}


/**
 *  Log RESET message
 *
 */
void log_reset(const logger_t *logger, const char *file, unsigned int line, const char *fmt, ...)
{
    va_list arg;
    log_data_t logdata;

    logdata.color = LOG_COLOR_RESET;
    logdata.stamp = "(RST)";
    logdata.file = file;
    logdata.line = line;
    logdata.syslog_type = LOG_EMERG;
    logdata.details = true;

#if LOGGING_FILE
    {
        va_start(arg, fmt);
        log_file(logger, &logdata, fmt, arg);
        va_end(arg);
    }
#endif

#if LOGGING_SYSLOG
    {
        va_start(arg, fmt);
        log_syslog(logger, &logdata, fmt, arg);
        va_end(arg);
    }
#endif

    {
        va_start(arg, fmt);
        log_console(logger, &logdata, fmt, arg);
        va_end(arg);
    }

    if (logger->reset_result == 1) {
        // Exit application
        exit(EXIT_FAILURE);
    }
    else if (logger->reset_result == 2) {
        // Reboot system
        sync();
        reboot(RB_AUTOBOOT);
    }
}


/**
 *  Log ERROR message
 *
 */
void log_error(const logger_t *logger, const char *file, unsigned int line, const char *fmt, ...)
{
    va_list arg;
    log_data_t logdata;

    if (logger->conf.bits.verbosity < ERROR_LEVEL)
        return;

    logdata.color = LOG_COLOR_ERROR;
    logdata.stamp = "(ERR)";
    logdata.file = file;
    logdata.line = line;
    logdata.syslog_type = LOG_ERR;
    logdata.details = true;

#if LOGGING_FILE
    if (logger->conf.bits.file) {
        va_start(arg, fmt);
        log_file(logger, &logdata, fmt, arg);
        va_end(arg);
    }
#endif

#if LOGGING_SYSLOG
    if (logger->conf.bits.syslog) {
        va_start(arg, fmt);
        log_syslog(logger, &logdata, fmt, arg);
        va_end(arg);
    }
#endif

    if (logger->conf.bits.console) {
        va_start(arg, fmt);
        log_console(logger, &logdata, fmt, arg);
        va_end(arg);
    }
}


/**
 *  Log WARNING message
 *
 */
void log_warn(const logger_t *logger, const char *fmt, ...)
{
    va_list arg;
    log_data_t logdata;

    if (logger->conf.bits.verbosity < WARNING_LEVEL)
        return;

    logdata.color = LOG_COLOR_WARN;
    logdata.stamp = "(WRN)";
    logdata.file = NULL;
    logdata.line = 0;
    logdata.syslog_type = LOG_WARNING;
    logdata.details = false;

#if LOGGING_FILE
    if (logger->conf.bits.file) {
        va_start(arg, fmt);
        log_file(logger, &logdata, fmt, arg);
        va_end(arg);
    }
#endif

#if LOGGING_SYSLOG
    if (logger->conf.bits.syslog) {
        va_start(arg, fmt);
        log_syslog(logger, &logdata, fmt, arg);
        va_end(arg);
    }
#endif

    if (logger->conf.bits.console) {
        va_start(arg, fmt);
        log_console(logger, &logdata, fmt, arg);
        va_end(arg);
    }
}


/**
 *  Log INFO message
 *
 */
void log_info(const logger_t *logger, const char *fmt, ...)
{
    va_list arg;
    log_data_t logdata;

    if (logger->conf.bits.verbosity < INFO_LEVEL)
        return;

    logdata.color = LOG_COLOR_INFO;
    logdata.stamp = "(LOG)";
    logdata.file = 0;
    logdata.line = 0;
    logdata.syslog_type = LOG_INFO;
    logdata.details = false;

#if LOGGING_FILE
    if (logger->conf.bits.file) {
        va_start(arg, fmt);
        log_file(logger, &logdata, fmt, arg);
        va_end(arg);
    }
#endif

#if LOGGING_SYSLOG
    if (logger->conf.bits.syslog) {
        va_start(arg, fmt);
        log_syslog(logger, &logdata, fmt, arg);
        va_end(arg);
    }
#endif

    if (logger->conf.bits.console) {
        va_start(arg, fmt);
        log_console(logger, &logdata, fmt, arg);
        va_end(arg);
    }
}


/**
 *  Log DEBUG message
 *
 */
void log_debug(const logger_t *logger, unsigned short debugtype, const char *fmt, ...)
{
    va_list arg;
    log_data_t logdata;

    if (logger->conf.bits.verbosity < DEBUG_LEVEL)
        return;
    if (!(logger->conf.bits.debugtype & debugtype))
        return;

    logdata.color = LOG_COLOR_DEBUG;
    logdata.stamp = "(DBG)";
    logdata.file = NULL;
    logdata.line = 0;
    logdata.syslog_type = LOG_DEBUG;
    logdata.details = false;

#if LOGGING_FILE
    if (logger->conf.bits.file) {
        va_start(arg, fmt);
        log_file(logger, &logdata, fmt, arg);
        va_end(arg);
    }
#endif

#if LOGGING_SYSLOG
    if (logger->conf.bits.syslog) {
        va_start(arg, fmt);
        log_syslog(logger, &logdata, fmt, arg);
        va_end(arg);
    }
#endif

    if (logger->conf.bits.console) {
        va_start(arg, fmt);
        log_console(logger, &logdata, fmt, arg);
        va_end(arg);
    }
}


/**
 *  Show LOG message
 *
 */
void log_log(const char *color, const char *fmt, ...)
{
    va_list arg;

    va_start(arg, fmt);

    fprintf(stderr, "%s", color);
    vfprintf(stderr, fmt, arg);
    fprintf(stderr, LOG_COLOR_OFF "\n");

    va_end(arg);
}


/**
 * Show data
 */
void log_data(const char *color, const char *prefix, const void *data, size_t len)
{
    fprintf(stderr, "%s", color);
    fprintf(stderr, "%s", prefix);
    for (size_t i=0; i<len; i++)
        fprintf(stderr, "%02X ", ((unsigned char*)data)[i]);
    fprintf(stderr, LOG_COLOR_OFF "\n");
}
