/**
 * @file io.c
 * @author kioz.wang (never.had@outlook.com)
 * @brief
 * @version 0.1
 * @date 2025-12-25
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

#include "io.h"
#include "global.h"
#include "logger/logger.h"

#define logFmtHead  "[io@%s] <%s> "
#define logArgHead  io->name, key
#define logFmtValue "\"%s\""

int io_get(const io_ctx_t *io, const char *key, const value_t **value, timestamp_t *duration) {
    assert(key);
    assert(value);
    if (!io->get) return -RPOPD_E_OPNOTSUPP;

    timestamp_t _duration;

    int ret = io->get(io->priv, key, value, &_duration);
    if (!ret && duration) *duration = _duration;
    if (ret) {
        logfE(logFmtHead "fail to get" logFmtErrno, logArgHead, logArgErrno);
    } else {
        char buffer[256];
        value_fmt(buffer, sizeof(buffer), *value, false);
        logfI(logFmtHead "get " logFmtValue " with duration %ldms", logArgHead, buffer, timestamp_to_ms(_duration));
    }
    return propd_errno_map();
}
int io_set(const io_ctx_t *io, const char *key, const value_t *value) {
    assert(key);
    assert(value);
    if (!io->set) return -RPOPD_E_OPNOTSUPP;

    char buffer[256];
    value_fmt(buffer, sizeof(buffer), value, false);

    int ret = io->set(io->priv, key, value);
    if (ret) {
        logfE(logFmtHead "fail to set " logFmtValue logFmtErrno, logArgHead, buffer, logArgErrno);
    } else {
        logfI(logFmtHead "set " logFmtValue, logArgHead, buffer);
    }
    return propd_errno_map();
}
int io_del(const io_ctx_t *io, const char *key) {
    assert(key);
    if (!io->del) return -RPOPD_E_OPNOTSUPP;

    int ret = io->del(io->priv, key);
    if (ret) {
        logfE(logFmtHead "fail to del" logFmtErrno, logArgHead, logArgErrno);
    } else {
        logfI(logFmtHead "del", logArgHead);
    }
    return propd_errno_map();
}
void io_destructor(io_ctx_t *io) {
    if (io->destructor) io->destructor(io->priv);
    free((void *)io->name);
    free(io);
}
