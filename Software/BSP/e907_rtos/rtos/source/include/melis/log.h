/*
 * ===========================================================================================
 *
 *       Filename:  log.h
 *
 *    Description:  log message system definition.
 *
 *        Version:  Melis3.0
 *         Create:  2018-04-27 15:18:50
 *       Revision:  none
 *       Compiler:  GCC:version 7.2.1 20170904 (release),ARM/embedded-7-branch revision 255204
 *
 *         Author:  caozilong@allwinnertech.com
 *   Organization:  BU1-PSW
 *  Last Modified:  2020-06-02 14:20:12
 *
 * ===========================================================================================
 */

#ifndef __MELIS_LOG_H__
#define __MELIS_LOG_H__

#include <kconfig.h>

#ifdef __cplusplus
extern "C" {
#endif
/* ----------------------------------------------------------------------------*/
#define XPOSTO(x)   "\033[" #x "D\033[" #x "C"
/* ----------------------------------------------------------------------------*/
/* ----------------------------------------------------------------------------*/
/**
 * @brief  printk depend on system implemtion.
 *
 * @param[in] fmt format strings.
 * @param[in] ... varidic parameter list.
 */
/* ----------------------------------------------------------------------------*/
extern int      printk(const char *fmt, ...);
extern int      get_log_level(void);
extern void     set_log_level(int level);

#define SYS_LOG_LEVEL   CONFIG_LOG_DEFAULT_LEVEL

#define KERN_EMERG "<1>"
#define KERN_ALERT "<2>"
#define KERN_CRIT "<2>"
#define KERN_ERR "<2>"
#define KERN_WARNING "<3>"
#define KERN_NOTICE "<3>"
#define KERN_INFO "<4>"
#define KERN_DEBUG "<4>"

#if defined(CONFIG_LOG_RELEASE)
#define LOG_LAYOUT      "%s"
#ifdef CONFIG_DYNAMIC_LOG_LEVEL_SUPPORT
#define LOG_BACKEND_CALL(log_level, log_prefix, log_color, log_format, color_off, ...) \
    do{                                                                 \
       printk("<%d>" LOG_LAYOUT log_format "%s""\n\r",             \
       log_level, log_color, ##__VA_ARGS__, color_off);            \
    } while(0);
#else
#define LOG_BACKEND_CALL(log_level, log_prefix, log_color, log_format, color_off, ...) \
    printk(LOG_LAYOUT log_format "%s""\n\r",                            \
        log_color, ##__VA_ARGS__, color_off);
#endif
#else
#define LOG_LAYOUT      "%s%s%s: [%s:%04u]: %s%s"
#ifdef CONFIG_DYNAMIC_LOG_LEVEL_SUPPORT
#define LOG_BACKEND_CALL(log_level, log_prefix, log_color, log_format, color_off, ...) \
    do{                                                                 \
       printk("<%d>" LOG_LAYOUT log_format "%s""\n\r",              \
       log_level, log_color, log_prefix, color_off, __func__, __LINE__, XPOSTO(60),\
       log_color, ##__VA_ARGS__, color_off);                        \
    } while(0);
#else
#define LOG_BACKEND_CALL(log_level, log_prefix, log_color, log_format, color_off, ...) \
    printk(LOG_LAYOUT log_format "%s""\n\r",                            \
           log_color, log_prefix, color_off, __func__, __LINE__, XPOSTO(60),           \
           log_color, ##__VA_ARGS__, color_off);
#endif
#endif

#define LOG_NO_COLOR(log_level, log_prefix, log_format, ...)                           \
    LOG_BACKEND_CALL(log_level, log_prefix, "", log_format, "", ##__VA_ARGS__)

#define LOG_COLOR(log_level, log_prefix, log_color, log_format, ...)    \
    LOG_BACKEND_CALL(log_level, log_prefix, log_color, log_format,      \
                     SYS_LOG_COLOR_OFF, ##__VA_ARGS__)

#undef OPTION_LOG_LEVEL_ERROR
#undef OPTION_LOG_LEVEL_WARNING

#define OPTION_LOG_LEVEL_CLOSE             0
#define OPTION_LOG_LEVEL_LOG               1
#define OPTION_LOG_LEVEL_ERROR             2
#define OPTION_LOG_LEVEL_WARNING           3
#define OPTION_LOG_LEVEL_INFO              4
#define OPTION_LOG_LEVEL_MESSAGE           5

#define SYS_LOG_COLOR_OFF                 "\033[0m"
#define SYS_LOG_COLOR_RED                 "\033[1;40;31m"
#define SYS_LOG_COLOR_YELLOW              "\033[1;40;33m"
#define SYS_LOG_COLOR_BLUE                "\033[1;40;34m"
#define SYS_LOG_COLOR_PURPLE              "\033[1;40;35m"

#define LOG_LEVEL_ERROR_PREFIX            "[ERR]"
#define LOG_LEVEL_WARNING_PREFIX          "[WRN]"
#define LOG_LEVEL_INFO_PREFIX             "[INF]"
#define LOG_LEVEL_VERBOSE_PREFIX          "[VBS]"
#define LOG_LEVEL_DEBUG_PREFIX            "[DBG]"

#ifdef CONFIG_DISABLE_ALL_DEBUGLOG
#define __err(...)    do { } while(0)
#define __wrn(...)    do { } while(0)
#define __inf(...)    do { } while(0)
#define __msg(...)    do { } while(0)
#define __log(...)    do { } while(0)
#else
#if SYS_LOG_LEVEL == OPTION_LOG_LEVEL_CLOSE
#define __err(...)    do { } while(0)
#define __wrn(...)    do { } while(0)
#define __inf(...)    do { } while(0)
#define __msg(...)    do { } while(0)
#define __log(...)    do { } while(0)
#elif SYS_LOG_LEVEL == OPTION_LOG_LEVEL_LOG
#define __log(...)    do { LOG_COLOR(OPTION_LOG_LEVEL_LOG, LOG_LEVEL_DEBUG_PREFIX, SYS_LOG_COLOR_YELLOW, ##__VA_ARGS__); } while(0)
#define __err(...)    do { } while(0)
#define __wrn(...)    do { } while(0)
#define __inf(...)    do { } while(0)
#define __msg(...)    do { } while(0)
#elif SYS_LOG_LEVEL == OPTION_LOG_LEVEL_ERROR
#define __log(...)    do { LOG_COLOR(OPTION_LOG_LEVEL_LOG, LOG_LEVEL_DEBUG_PREFIX, SYS_LOG_COLOR_YELLOW, ##__VA_ARGS__); } while(0)
#define __err(...)    do { LOG_COLOR(OPTION_LOG_LEVEL_ERROR, LOG_LEVEL_ERROR_PREFIX, SYS_LOG_COLOR_RED, ##__VA_ARGS__); } while(0)
#define __wrn(...)    do { } while(0)
#define __inf(...)    do { } while(0)
#define __msg(...)    do { } while(0)
#elif SYS_LOG_LEVEL == OPTION_LOG_LEVEL_WARNING
#define __log(...)    do { LOG_COLOR(OPTION_LOG_LEVEL_LOG, LOG_LEVEL_DEBUG_PREFIX, SYS_LOG_COLOR_YELLOW, ##__VA_ARGS__); } while(0)
#define __err(...)    do { LOG_COLOR(OPTION_LOG_LEVEL_ERROR, LOG_LEVEL_ERROR_PREFIX, SYS_LOG_COLOR_RED, ##__VA_ARGS__); } while(0)
#define __wrn(...)    do { LOG_COLOR(OPTION_LOG_LEVEL_WARNING, LOG_LEVEL_WARNING_PREFIX, SYS_LOG_COLOR_PURPLE, ##__VA_ARGS__); } while(0)
#define __inf(...)    do { } while(0)
#define __msg(...)    do { } while(0)
#elif SYS_LOG_LEVEL == OPTION_LOG_LEVEL_INFO
#define __log(...)    do { LOG_COLOR(OPTION_LOG_LEVEL_LOG, LOG_LEVEL_DEBUG_PREFIX, SYS_LOG_COLOR_YELLOW, ##__VA_ARGS__); } while(0)
#define __err(...)    do { LOG_COLOR(OPTION_LOG_LEVEL_ERROR, LOG_LEVEL_ERROR_PREFIX, SYS_LOG_COLOR_RED, ##__VA_ARGS__); } while(0)
#define __wrn(...)    do { LOG_COLOR(OPTION_LOG_LEVEL_WARNING, LOG_LEVEL_WARNING_PREFIX, SYS_LOG_COLOR_PURPLE, ##__VA_ARGS__); } while(0)
#define __inf(...)    do { LOG_COLOR(OPTION_LOG_LEVEL_INFO, LOG_LEVEL_INFO_PREFIX, SYS_LOG_COLOR_BLUE, ##__VA_ARGS__); } while(0)
#define __msg(...)    do { } while(0)
#elif SYS_LOG_LEVEL == OPTION_LOG_LEVEL_MESSAGE
#define __log(...)    do { LOG_COLOR(OPTION_LOG_LEVEL_LOG, LOG_LEVEL_DEBUG_PREFIX, SYS_LOG_COLOR_YELLOW, ##__VA_ARGS__); } while(0)
#define __err(...)    do { LOG_COLOR(OPTION_LOG_LEVEL_ERROR, LOG_LEVEL_ERROR_PREFIX, SYS_LOG_COLOR_RED, ##__VA_ARGS__); } while(0)
#define __wrn(...)    do { LOG_COLOR(OPTION_LOG_LEVEL_WARNING, LOG_LEVEL_WARNING_PREFIX, SYS_LOG_COLOR_PURPLE, ##__VA_ARGS__); } while(0)
#define __inf(...)    do { LOG_COLOR(OPTION_LOG_LEVEL_INFO, LOG_LEVEL_INFO_PREFIX, SYS_LOG_COLOR_BLUE, ##__VA_ARGS__); } while(0)
#define __msg(...)    do { LOG_NO_COLOR(OPTION_LOG_LEVEL_MESSAGE, LOG_LEVEL_VERBOSE_PREFIX, ##__VA_ARGS__); } while(0)
#else
#error "invalid configuration of debug level."
#endif
#endif

#ifndef pr_fmt
#define pr_fmt(fmt) fmt
#endif

#define pr_emerg(fmt, ...) \
    __err(pr_fmt(fmt), ##__VA_ARGS__)
#define pr_alert(fmt, ...) \
    __err(pr_fmt(fmt), ##__VA_ARGS__)
#define pr_crit(fmt, ...) \
    __err(pr_fmt(fmt), ##__VA_ARGS__)
#define pr_err(fmt, ...) \
    __err(pr_fmt(fmt), ##__VA_ARGS__)
#define pr_warning(fmt, ...) \
    __wrn(pr_fmt(fmt), ##__VA_ARGS__)
#define pr_warn pr_warning
#define pr_notice(fmt, ...) \
    __log(pr_fmt(fmt), ##__VA_ARGS__)
#define pr_info(fmt, ...) \
    __inf(pr_fmt(fmt), ##__VA_ARGS__)
#define pr_debug(fmt, ...) \
    __msg(pr_fmt(fmt), ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif
