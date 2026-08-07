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

#include <prop/io.h>
#include "global.h"
#include <errno.h>

#define logFmtHead "[io::%s] "
#define logArgHead io->name

int prop_storage_get(const prop_io_t *io, const char *key, const value_t **value, timestamp_t *duration) {
    assert(key);
    assert(value);
    if (!io->get) return EOPNOTSUPP;

    timestamp_t _duration;

    int ret = io->get(io->priv, key, value, &_duration);
    if (ret) {
        logfE(logFmtHead "fail to get " logFmtKey logFmtErrno, logArgHead, key, logArgErrno_(ret));
        return ret;
    }

    if (duration) *duration = _duration;
    char buffer[256] = {0};
    char buffer1[32] = {0};
    pd_value_fmt(buffer, sizeof(buffer), *value, false);
    duration_fmt(buffer1, sizeof(buffer1), _duration);
    logfI(logFmtHead "get " logFmtKey " is " logFmtValue " with duration %s", logArgHead, key, buffer, buffer1);
    return 0;
}

int prop_storage_info(const prop_io_t *io, const char *key, range_t *range, char **help_message, char ***chain) {
    assert(key);

    return 0;
}

int prop_storage_set(const prop_io_t *io, const char *key, const value_t *value) {
    assert(key);
    assert(value);
    if (!io->set) return EOPNOTSUPP;

    char buffer[256] = {0};
    pd_value_fmt(buffer, sizeof(buffer), value, false);

    int ret = io->set(io->priv, key, value);
    if (ret) {
        logfE(logFmtHead "fail to set " logFmtKey " as " logFmtValue logFmtErrno, logArgHead, key, buffer,
              logArgErrno_(ret));
        return ret;
    }

    logfI(logFmtHead "set " logFmtKey " as " logFmtValue, logArgHead, key, buffer);
    return 0;
}

int prop_storage_del(const prop_io_t *io, const char *key) {
    assert(key);
    if (!io->set) return EOPNOTSUPP;

    int ret = io->del(io->priv, key);
    if (ret) {
        logfE(logFmtHead "fail to del " logFmtKey logFmtErrno, logArgHead, key, logArgErrno_(ret));
        return ret;
    }

    logfI(logFmtHead "del " logFmtKey, logArgHead, key);
    return ret;
}

void prop_storage_destructor(const prop_io_t *io) {
    if (io->destructor) io->destructor(io->priv);
    free((void *)io->name);
}
