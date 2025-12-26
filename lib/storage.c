/**
 * @file storage.c
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

#include "storage.h"
#include "global.h"
#include "logger/logger.h"

#define logFmtHead  "[storage@%s] <%s> "
#define logArgHead  storage->name, key
#define logFmtValue "\"%s\""

int storage_get(const storage_ctx_t *storage, const char *key, const value_t **value, timestamp_t *duration) {
    assert(key);
    assert(value);
    if (!storage->get) return -RPOPD_E_OPNOTSUPP;

    timestamp_t _duration;

    int ret = storage->get(storage->priv, key, value, &_duration);
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
int storage_set(const storage_ctx_t *storage, const char *key, const value_t *value) {
    assert(key);
    assert(value);
    if (!storage->set) return -RPOPD_E_OPNOTSUPP;

    char buffer[256];
    value_fmt(buffer, sizeof(buffer), value, false);

    int ret = storage->set(storage->priv, key, value);
    if (ret) {
        logfE(logFmtHead "fail to set " logFmtValue logFmtErrno, logArgHead, buffer, logArgErrno);
    } else {
        logfI(logFmtHead "set " logFmtValue, logArgHead, buffer);
    }
    return propd_errno_map();
}
int storage_del(const storage_ctx_t *storage, const char *key) {
    assert(key);
    if (!storage->del) return -RPOPD_E_OPNOTSUPP;

    int ret = storage->del(storage->priv, key);
    if (ret) {
        logfE(logFmtHead "fail to del" logFmtErrno, logArgHead, logArgErrno);
    } else {
        logfI(logFmtHead "del", logArgHead);
    }
    return propd_errno_map();
}
void storage_destructor(storage_ctx_t *storage) {
    if (storage->destructor) storage->destructor(storage->priv);
    free((void *)storage->name);
    free(storage);
}
