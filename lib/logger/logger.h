/**
 * @file logger.h
 * @author kioz.wang
 * @brief
 * @version 0.2
 * @date 2025-10-09
 *
 * @copyright MIT License
 *
 *  Copyright (c) 2025 kioz.wang
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 */

#ifndef __LOGGER_H__
#define __LOGGER_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum mlog_level {
    MLOG_ERRO,
    MLOG_ERROR = MLOG_ERRO,
    MLOG_WARN,
    MLOG_INFO,
    MLOG_VERB,
    MLOG_DEBG,
    MLOG_DEBUG = MLOG_DEBG,
};
typedef uint8_t mlog_level_t;

mlog_level_t mlog_level_parse(const char *s);

typedef void (*mlogger_f)(const char *msg);

#define MLOGGER_SYMBOL_PREFIX propd_

#define __SYMBOL_CONCAT__(a, b)        a##b
#define __MLOGGER_FUNC__(prefix, name) __SYMBOL_CONCAT__(prefix, name)
#define MLOGGER_FUNC(name)             __MLOGGER_FUNC__(MLOGGER_SYMBOL_PREFIX, name)

void MLOGGER_FUNC(logf)(mlog_level_t lvl, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
void MLOGGER_FUNC(set_logger)(mlog_level_t lvl, mlogger_f f);
void MLOGGER_FUNC(set_out_logger)(void (*f)(mlog_level_t, mlogger_f));

#ifdef MLOGGER_COLOR
#define MLOG_cRESET "\e[0m"
#define MLOG_cBOLD  "\e[1m"
#define MLOG_cERRO  "\e[31m"               /* 红 */
#define MLOG_cWARN  "\e[33m"               /* 黄 */
#define MLOG_cINFO  "\e[32m"               /* 绿 */
#define MLOG_cVERB  "\e[38;2;100;150;100m" /* 浅绿 */
#define MLOG_cDEBG  "\e[90m"               /* 灰 */
#else
#define MLOG_cRESET
#define MLOG_cBOLD
#define MLOG_cERRO
#define MLOG_cWARN
#define MLOG_cINFO
#define MLOG_cVERB
#define MLOG_cDEBG
#endif /* MLOGGER_COLOR */

#ifdef MLOGGER_TIMESTAMP
#include <stdbool.h>
#include <time.h>
static inline int64_t mlog_timestamp(bool monotonic) {
    struct timespec ts;
    clock_gettime(monotonic ? CLOCK_MONOTONIC : CLOCK_REALTIME, &ts);
    return ts.tv_sec * 1000000000L + ts.tv_nsec;
}
#define MLOG_tsFmt "[%lx]"
#define MLOG_tsArg , (mlog_timestamp(true) / 1000)
#else
#define MLOG_tsFmt
#define MLOG_tsArg
#endif /* MLOGGER_TIMESTAMP */

#ifdef MLOGGER_LEVEL
#define MLOG_lERRO "[E]"
#define MLOG_lWARN "[W]"
#define MLOG_lINFO "[I]"
#define MLOG_lVERB "[V]"
#define MLOG_lDEBG "[D]"
#else
#define MLOG_lERRO
#define MLOG_lWARN
#define MLOG_lINFO
#define MLOG_lVERB
#define MLOG_lDEBG
#endif /* MLOGGER_LEVEL */

#define logfE(fmt, ...)                                                                                                \
    MLOGGER_FUNC(logf)(MLOG_ERRO, MLOG_tsFmt MLOG_cERRO MLOG_lERRO MLOG_cBOLD fmt MLOG_cRESET "\n" MLOG_tsArg,         \
                       ##__VA_ARGS__)
#define logfW(fmt, ...)                                                                                                \
    MLOGGER_FUNC(logf)(MLOG_WARN, MLOG_tsFmt MLOG_cWARN MLOG_lWARN MLOG_cBOLD fmt MLOG_cRESET "\n" MLOG_tsArg,         \
                       ##__VA_ARGS__)
#define logfI(fmt, ...)                                                                                                \
    MLOGGER_FUNC(logf)(MLOG_INFO, MLOG_tsFmt MLOG_cINFO MLOG_lINFO fmt MLOG_cRESET "\n" MLOG_tsArg, ##__VA_ARGS__)
#define logfV(fmt, ...)                                                                                                \
    MLOGGER_FUNC(logf)(MLOG_VERB, MLOG_tsFmt MLOG_cVERB MLOG_lVERB fmt MLOG_cRESET "\n" MLOG_tsArg, ##__VA_ARGS__)
#define logfD(fmt, ...)                                                                                                \
    MLOGGER_FUNC(logf)(MLOG_DEBG, MLOG_tsFmt MLOG_cDEBG MLOG_lDEBG fmt MLOG_cRESET "\n" MLOG_tsArg, ##__VA_ARGS__)

#include <errno.h>
#include <string.h>
#define logFmtRet   " (%d)"
#define logFmtErrno " (%d:%s)"
#define logArgErrno errno, strerror(errno)

#ifdef __cplusplus
}
#endif

#endif /* __LOGGER_H__ */
