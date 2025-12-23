/**
 * @file unix.c
 * @author kioz.wang (never.had@outlook.com)
 * @brief
 * @version 0.1
 * @date 2025-12-15
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

#include "global.h"
#include "io/bridge.h"
#include "io_server.h"
#include "logger/logger.h"
#include "misc.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#define logFmtHead  "[bridge][unix] "
#define logFmtKey   "<%s> "
#define logFmtValue "\"%s\""

static int io_connect(const char *target, int *connfd) {
    int                ret     = 0;
    struct sockaddr_un cliaddr = {0}, servaddr = {0};
    int                _connfd = socket(AF_LOCAL, SOCK_STREAM, 0);
    if (_connfd == -1) {
        logfE(logFmtHead "fail to get socket" logFmtErrno, logArgErrno);
        return -1;
    }

    cliaddr.sun_family  = AF_LOCAL;
    cliaddr.sun_path[0] = '\0';
    random_alphabet(&cliaddr.sun_path[1], sizeof(cliaddr.sun_path) - 2, false);
    cliaddr.sun_path[sizeof(cliaddr.sun_path) - 1] = 'X';

    ret = bind(_connfd, (const struct sockaddr *)&cliaddr, sizeof(cliaddr));
    if (ret) {
        logfE(logFmtHead "fail to bind" logFmtErrno, logArgErrno);
        close(_connfd);
        return ret;
    }

    servaddr.sun_family = AF_LOCAL;
    snprintf(servaddr.sun_path, sizeof(servaddr.sun_path), PathFmt_IOServer, g_at, target);

    ret = connect(_connfd, (const struct sockaddr *)&servaddr, sizeof(servaddr));
    if (ret) {
        logfE(logFmtHead "fail to connect %s" logFmtErrno, target, logArgErrno);
        close(_connfd);
        return ret;
    }

    logfI(logFmtHead "connect %s as %d", target, _connfd);
    *connfd = _connfd;
    return ret;
}

static void io_disconnect(int connfd) {
    shutdown(connfd, SHUT_WR);
    close(connfd);
    logfI(logFmtHead "disconnect %d", connfd);
}

static void io_begin(int connfd, io_type_t type, const char *key, const value_t *value) {
    ssize_t      n;
    io_package_t pkg_head = {.type = type, .created = timestamp(true)};

    strncpy(pkg_head.key, key, sizeof(pkg_head.key));
    pkg_head.value.type   = value ? value->type : _value_undef;
    pkg_head.value.length = value ? value->length : 0;

    n = send(connfd, &pkg_head, sizeof(pkg_head), 0);
    assert(n == sizeof(pkg_head));
    logfD(logFmtHead logFmtKey ">>>%d send header of package with type %d", key, connfd, type);

    if (value) {
        n = send(connfd, value->data, value->length, 0);
        assert(n == value->length);
        logfD(logFmtHead logFmtKey ">>>%d send data of value with length %d", key, connfd, value->length);
    }
}

static int io_end(int connfd, const char *key) {
    int result = 0;

    if (sizeof(result) != recv(connfd, &result, sizeof(result), 0)) {
        return EIO;
    }
    logfD(logFmtHead logFmtKey "<<<%d recv result" logFmtRet, key, connfd, result);
    return result;
}

void unix_stream_discard(int connfd) {
    ssize_t n;
    char    discard[32];
    int     fl = fcntl(connfd, F_GETFL);
    fcntl(connfd, F_SETFL, fl | O_NONBLOCK);
    do {
        n = recv(connfd, discard, sizeof(discard), 0);
    } while (n > 0 && n == sizeof(discard));
    fcntl(connfd, F_SETFL, fl);
}

static int get(const char *target, const char *key, const value_t **value, timestamp_t *duration) {
    int         ret    = 0;
    int         connfd = -1;
    timestamp_t _duration;
    value_t     value_head;
    value_t    *_value = NULL;

    ret = io_connect(target, &connfd);
    if (ret) return ret;

    io_begin(connfd, _io_get, key, NULL);

    if (sizeof(_duration) != recv(connfd, &_duration, sizeof(_duration), 0)) {
        ret = EIO;
        goto exit;
    }
    logfD(logFmtHead logFmtKey "<<<%d recv duration %ld", key, connfd, _duration);

    if (sizeof(value_head) != recv(connfd, &value_head, sizeof(value_head), 0)) {
        ret = EIO;
        goto exit;
    }
    logfD(logFmtHead logFmtKey "<<<%d recv header of value with type %d", key, connfd, value_head.type);

    _value = (value_t *)malloc(sizeof(value_t) + value_head.length);
    if (!_value) {
        ret = errno;
        goto exit;
    }
    memcpy(_value, &value_head, sizeof(value_t));

    if (_value->length != recv(connfd, _value->data, _value->length, 0)) {
        ret = EIO;
        goto exit;
    }
    logfD(logFmtHead logFmtKey "<<<%d recv data of value with length %d", key, connfd, _value->length);

    ret = io_end(connfd, key);
    if (ret) {
        goto exit;
    }

    io_disconnect(connfd);

    *value    = _value;
    *duration = _duration;

    char buffer[256];
    logfI(logFmtHead logFmtKey "get " logFmtValue " with duration %ldms", key,
          value_fmt(buffer, sizeof(buffer), _value, false), timestamp_to_ms(_duration));
    return 0;

exit:
    unix_stream_discard(connfd);
    free(_value);
    return ret;
}

static int set(const char *target, const char *key, const value_t *value) {
    int ret    = 0;
    int connfd = -1;

    ret = io_connect(target, &connfd);
    if (ret) return ret;

    io_begin(connfd, _io_set, key, value);
    ret = io_end(connfd, key);
    if (!ret) {
        char buffer[256];
        logfI(logFmtHead logFmtKey "set " logFmtValue, key, value_fmt(buffer, sizeof(buffer), value, false));
    }

    io_disconnect(connfd);
    return ret;
}

static int del(const char *target, const char *key) {
    int ret    = 0;
    int connfd = -1;

    ret = io_connect(target, &connfd);
    if (ret) return ret;

    io_begin(connfd, _io_del, key, NULL);
    ret = io_end(connfd, key);
    if (!ret) {
        logfI(logFmtHead logFmtKey "del", key);
    }

    io_disconnect(connfd);
    return ret;
}

io_t bridge_unix(const char *target) {
    io_t io = BRIDGE_INITIALIZER;

    if (!(io.priv = strdup(target))) {
        logfE(logFmtHead "fail to allocate priv" logFmtErrno, logArgErrno);
        return io;
    }

    io.get    = (typeof(io.get))get;
    io.set    = (typeof(io.set))set;
    io.del    = (typeof(io.del))del;
    io.deinit = free;
    return io;
}
