/**
 * @file logger.c
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

#include "logger.h"
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef MLOGGER_FMT_MAX
#define MLOGGER_FMT_MAX (1024)
#endif /* MLOGGER_FMT_MAX */

static mlog_level_t g_log_level = MLOG_DEBG;

static void default_logger(const char *msg) { fputs(msg, stdout); }

static mlogger_f g_logger = default_logger;

static const char *log_level_names[MLOG_DEBG + 1] = {
    "ERRO", "WARN", "INFO", "VERB", "DEBG",
};

mlog_level_t mlog_level_parse(const char *s) {
    for (int32_t i = 0; i < MLOG_DEBG + 1; i++) {
        if (!strcmp(s, log_level_names[i])) {
            return i;
        }
    }
    if (!strcmp(s, "ERROR")) return MLOG_ERROR;
    if (!strcmp(s, "DEBUG")) return MLOG_DEBUG;

    int32_t       value;
    char         *endptr = NULL;
    unsigned long parsed = strtoul(s, &endptr, 0);
    if (s == endptr || parsed == ULONG_MAX) {
        value = MLOG_DEBG;
    } else {
        value = (parsed > MLOG_DEBG) ? MLOG_DEBG : (int32_t)parsed;
    }
    return value;
}

static int32_t check_stderr_level() {
    static int32_t value   = -2;
    const char    *value_s = NULL;

    if (value == -2) {
#ifdef MLOGGER_ENV
        value_s = getenv(MLOGGER_ENV);
#endif /* MLOGGER_ENV */
        if (!value_s || value_s[0] == '\0') {
            value = -1;
        }
    }
    if (value != -2) return value;

    return value = mlog_level_parse(value_s);
}

static void logf__(mlog_level_t lvl, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    int32_t stderr_level = check_stderr_level();

    if ((stderr_level >= 0 && lvl <= stderr_level) || lvl <= g_log_level) {
        char line[MLOGGER_FMT_MAX] = {0};
        (void)vsnprintf(line, sizeof(line) - 1, fmt, args);

        if (stderr_level >= 0 && lvl <= stderr_level) fputs(line, stderr);
        if (lvl <= g_log_level) g_logger(line);
    }

    va_end(args);
}

static void set_logger(mlog_level_t lvl, mlogger_f f) {
    g_log_level = (lvl > MLOG_DEBG) ? MLOG_DEBG : lvl;
    if (f) g_logger = f;
}

static void set_out_logger(void (*f)(mlog_level_t, mlogger_f)) { f(g_log_level, g_logger); }

void MLOGGER_FUNC(logf)(mlog_level_t lvl, const char *fmt, ...) __attribute__((alias("logf__")));
void MLOGGER_FUNC(set_logger)(mlog_level_t lvl, mlogger_f f) __attribute__((alias("set_logger")));
void MLOGGER_FUNC(set_out_logger)(void (*f)(mlog_level_t, mlogger_f)) __attribute__((alias("set_out_logger")));

#ifdef __TEST_LOGGER__

int32_t main(int32_t argc, const char *argv[]) {
    set_logger(MLOG_INFO, NULL);
    for (int32_t i = 0; i < MLOG_DEBG + 1; i++) {
        logf__(i, "message level %s\n", log_level_names[i]);
    }
    logf__(MLOG_ERRO, "\n");
    logfE("%s", log_level_names[MLOG_ERRO]);
    logfW("%s", log_level_names[MLOG_WARN]);
    logfI("%s", log_level_names[MLOG_INFO]);
    logfV("%s", log_level_names[MLOG_VERB]);
    logfD("%s", log_level_names[MLOG_DEBG]);
}

#endif /* __TEST_LOGGER__ */
