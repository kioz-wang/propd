/**
 * @file logger.h
 * @author kioz.wang (never.had@outlook.com)
 * @brief
 * @version 0.1
 * @date 2026-08-09
 *
 * @copyright MIT License
 *
 *  Copyright (c) 2026 kioz.wang
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

#ifndef __PROP_LOGGER_H
#define __PROP_LOGGER_H

#include <errno.h>
#include <mlogger/logger.h>
#include <string.h>

extern mlogger_t g_logger;

#define logfE(err, fmt, ...)     mlogf(&g_logger, MLOG_ERRO, fmt " (%d)", ##__VA_ARGS__, err)
#define logfE_std(err, fmt, ...) mlogf(&g_logger, MLOG_ERRO, fmt " (%d:%s)", ##__VA_ARGS__, err, strerror(err))
#define logfW(err, fmt, ...)     mlogf(&g_logger, MLOG_WARN, fmt " (%d)", ##__VA_ARGS__, err)
#define logfW_std(err, fmt, ...) mlogf(&g_logger, MLOG_WARN, fmt " (%d:%s)", ##__VA_ARGS__, err, strerror(err))
#define logfI(fmt, ...)          mlogf(&g_logger, MLOG_INFO, fmt, ##__VA_ARGS__)
#define logfV(fmt, ...)          mlogf(&g_logger, MLOG_VERB, fmt, ##__VA_ARGS__)
#define logfD(fmt, ...)          mlogf(&g_logger, MLOG_DEBG, fmt, ##__VA_ARGS__)

#endif /* __PROP_LOGGER_H */
