
#ifndef __MX_LOG_H_
#define __MX_LOG_H_


#include <stdbool.h>
#include <stddef.h>


#define LOG_COLOR_RED           "\x1B[0;31m"
#define LOG_COLOR_RED_BOLD      "\x1B[1;31m"
#define LOG_COLOR_GREEN         "\x1B[0;32m"
#define LOG_COLOR_GREEN_BOLD    "\x1B[1;32m"
#define LOG_COLOR_YELLOW        "\x1B[0;33m"
#define LOG_COLOR_YELLOW_BOLD   "\x1B[1;33m"
#define LOG_COLOR_BLUE          "\x1B[0;34m"
#define LOG_COLOR_BLUE_BOLD     "\x1B[1;34m"
#define LOG_COLOR_PURPLE        "\x1B[0;35m"
#define LOG_COLOR_PURPLE_BOLD   "\x1B[1;35m"
#define LOG_COLOR_CYAN          "\x1B[0;36m"
#define LOG_COLOR_CYAN_BOLD     "\x1B[1;36m"
#define LOG_COLOR_WHITE         "\x1B[0;37m"
#define LOG_COLOR_WHITE_BOLD    "\x1B[1;37m"
#define LOG_COLOR_OFF           "\x1B[00m"



typedef enum
{
    RESET_LEVEL = 0,
    ERROR_LEVEL,
    WARNING_LEVEL,
    INFO_LEVEL,
    DEBUG_LEVEL
} log_verbosity_t;


typedef union
{
    unsigned int cfgmask;
    struct {
        unsigned int verbosity : 4;
        unsigned int console : 1;
        unsigned int syslog : 1;
        unsigned int file : 1;
        unsigned int timestamp : 1;
        unsigned int debugtype : 8;
    } bits;
} log_conf_t;

typedef struct
{
    log_conf_t conf;
    const char *name;
    const char *file;
    bool terminal;
    int reset_result;
} logger_t;


#define LOG_VERBOSITY_MASK      (0xF)
#define LOG_CONSOLE_MASK        (0x1 << 4)
#define LOG_SYSLOG_MASK         (0x1 << 5)
#define LOG_FILE_MASK           (0x1 << 6)
#define LOG_TIME_STAMP          (0x1 << 7)
#define LOG_DEBUGTYPE_MASK      (0xFF << 8)

#define LOG_DEFAULT_DEBUGTYPE   (0xFF)
#define LOG_DEFAULT_VALUE       (0xFF13)



extern logger_t *deflogger;

extern void log_init  (logger_t *logger, const char *name);

extern void log_reset (const logger_t *logger, const char *file, unsigned int line, const char *fmt, ...);
extern void log_error (const logger_t *logger, const char *file, unsigned int line, const char *fmt, ...);
extern void log_warn  (const logger_t *logger, const char *fmt, ...);
extern void log_info  (const logger_t *logger, const char *fmt, ...);
extern void log_debug (const logger_t *logger, unsigned short debugtype, const char *fmt, ...);

extern void log_log   (const char *color, const char *fmt, ...);
extern void log_data  (const char *color, const char *prefix, const void *data, size_t len);



#ifndef __FILENAME__
  #define __FILENAME__   __FILE__
#endif

#define _RESET_(logger, ...)            { log_reset (logger, __FILENAME__, __LINE__, __VA_ARGS__); }
#define _ERROR_(logger, ...)            { log_error (logger, __FILENAME__, __LINE__, __VA_ARGS__); }
#define _WARN_(logger, ...)             { log_warn  (logger,                         __VA_ARGS__); }
#define _INFO_(logger, ...)             { log_info  (logger,                         __VA_ARGS__); }
#define _DEBUG_(logger, debugtype, ...) { log_debug (logger, debugtype,              __VA_ARGS__); }

#define _LOG_(color, ...)                       { log_log(color, __VA_ARGS__);}
#define _LOG_DATA_(color, prefix, data, len)    { if (len > 0) log_data(color, prefix, data, len);}


#define RESET(...)  _RESET_(deflogger, __VA_ARGS__)
#define ERROR(...)  _ERROR_(deflogger, __VA_ARGS__)
#define WARN(...)   _WARN_(deflogger, __VA_ARGS__)
#define INFO(...)   _INFO_(deflogger, __VA_ARGS__)
#define DEBUG(...)  _DEBUG_(deflogger, LOG_DEFAULT_DEBUGTYPE, __VA_ARGS__)


#endif /* __MX_LOG_H_ */
