/**
 * @file file.c
 * @author kioz.wang (never.had@outlook.com)
 * @brief
 * @version 0.1
 * @date 2025-12-08
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

#include "io/bridge.h"
#include "logger/logger.h"
#include <errno.h>
#include <linux/limits.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define logFmtHead  "[bridge][file] "
#define logFmtKey   "<%s> "
#define logFmtValue "\"%s\""

static int file_get(const char *root, const char *key, const value_t **value, timestamp_t *duration) {
    int      ret = 0;
    value_t  value_head;
    value_t *_value         = NULL;
    char     path[PATH_MAX] = {};
    snprintf(path, sizeof(path), "%s/%s", root, key);
    FILE *fp = fopen(path, "r");
    if (!fp) {
        logfE(logFmtHead logFmtKey "fail to open %s" logFmtErrno, key, path, logArgErrno);
        return errno;
    }
    if (1 != fread(&value_head, sizeof(value_t), 1, fp)) {
        logfE(logFmtHead logFmtKey "fail to read header of value", key);
        ret = EIO;
        goto exit;
    }
    _value = (value_t *)malloc(sizeof(value_t) + value_head.length);
    if (!_value) {
        logfE(logFmtHead logFmtKey "fail to allocate value with length %d" logFmtErrno, key, value_head.length,
              logArgErrno);
        ret = errno;
        goto exit;
    }
    memcpy(_value, &value_head, sizeof(value_t));
    if (1 != fread(_value->data, _value->length, 1, fp)) {
        logfE(logFmtHead logFmtKey "fail to read data of value", key);
        ret = EIO;
        goto exit;
    }
    fclose(fp);

    *value    = _value;
    *duration = 0;

    char buffer[256];
    logfI(logFmtHead logFmtKey "get " logFmtValue, key, value_fmt(buffer, sizeof(buffer), _value, false));
    return 0;

exit:
    fclose(fp);
    free(_value);
    return ret;
}

static int file_set(const char *root, const char *key, const value_t *value) {
    int  ret       = 0;
    char path[256] = {};
    snprintf(path, sizeof(path), "%s/%s", root, key);
    FILE *fp = fopen(path, "w");
    if (!fp) {
        logfE(logFmtHead logFmtKey "fail to open %s" logFmtErrno, key, path, logArgErrno);
        return errno;
    }
    if (1 != fwrite(value, sizeof(value_t) + value->length, 1, fp)) {
        logfE(logFmtHead logFmtKey "fail to write value", key);
        ret = EIO;
    }
    fclose(fp);

    if (!ret) {
        char buffer[256];
        logfI(logFmtHead logFmtKey "set " logFmtValue, key, value_fmt(buffer, sizeof(buffer), value, false));
    }
    return ret;
}

static int file_del(const char *root, const char *key) {
    char path[256] = {};
    snprintf(path, sizeof(path), "%s/%s", root, key);
    int ret = unlink(path);
    if (!ret) {
        logfI(logFmtHead logFmtKey "del", key);
    }
    return ret;
}

io_t bridge_file(const char *dir) {
    io_t io = BRIDGE_INITIALIZER;

    if (access(dir, F_OK) == -1) {
        int ret = mkdir(dir, 0755);
        if (ret) {
            logfE(logFmtHead "fail to create root path %s" logFmtErrno, dir, logArgErrno);
            return io;
        }
    }

    if (!(io.priv = strdup(dir))) {
        logfE(logFmtHead "fail to allocate priv" logFmtErrno, logArgErrno);
        return io;
    }

    io.get    = (typeof(io.get))file_get;
    io.set    = (typeof(io.set))file_set;
    io.del    = (typeof(io.del))file_del;
    io.deinit = free;
    return io;
}
