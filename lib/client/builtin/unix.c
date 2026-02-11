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

#include "builtin.h"
#include "global.h"
#include "misc.h"
#include "server/io.h"
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#define logFmtHead "[storage::(unix)] "

struct priv {
    bool shared;
    union {
        const char *target; /* not shared */
        struct {            /* shared */
            int             connfd;
            pthread_mutex_t mutex;
        };
    };
};
typedef struct priv priv_t;

static int __connect(const char *target, int *__connfd) {
    int                ret     = 0;
    struct sockaddr_un cliaddr = {0}, servaddr = {0};
    int                connfd = socket(AF_LOCAL, SOCK_STREAM, 0);
    if (connfd == -1) {
        logfE(logFmtHead "fail to get socket" logFmtErrno, logArgErrno);
        return errno;
    }

    cliaddr.sun_family  = AF_LOCAL;
    cliaddr.sun_path[0] = '\0';
    random_alnum(&cliaddr.sun_path[1], sizeof(cliaddr.sun_path) - 2);
    cliaddr.sun_path[sizeof(cliaddr.sun_path) - 1] = 'X';

    ret = bind(connfd, (const struct sockaddr *)&cliaddr, sizeof(cliaddr));
    if (ret) {
        logfE(logFmtHead "fail to bind" logFmtErrno, logArgErrno);
        close(connfd);
        return errno;
    }

    servaddr.sun_family = AF_LOCAL;
    snprintf(servaddr.sun_path, sizeof(servaddr.sun_path), PathFmt_IOServer, g_at, target);

    ret = connect(connfd, (const struct sockaddr *)&servaddr, sizeof(servaddr));
    if (ret) {
        logfE(logFmtHead "fail to connect %s" logFmtErrno, target, logArgErrno);
        close(connfd);
        return ENXIO;
    }

    logfI(logFmtHead "connect %s as %d", target, connfd);
    *__connfd = connfd;
    return 0;
}

static void __disconnect(int connfd) {
    shutdown(connfd, SHUT_WR);
    close(connfd);
    logfI(logFmtHead "disconnect %d", connfd);
}

static int unix_connect(priv_t *priv, int *connfd) {
    if (priv->shared) {
        *connfd = priv->connfd;
        pthread_mutex_lock(&priv->mutex);
    } else {
        return __connect(priv->target, connfd);
    }
    return 0;
}

static void unix_disconnect(priv_t *priv, int connfd) {
    if (priv->shared) {
        pthread_mutex_unlock(&priv->mutex);
    } else {
        __disconnect(connfd);
    }
}

void unix_stream_discard(int connfd) {
    ssize_t n;
    char    discard[16];
    int     fl = fcntl(connfd, F_GETFL);
    fcntl(connfd, F_SETFL, fl | O_NONBLOCK);
    do {
        n = recv(connfd, discard, sizeof(discard), 0);
    } while (n > 0 && n == sizeof(discard));
    fcntl(connfd, F_SETFL, fl);
}

int unix_recv_cstring(int connfd, char **__cstring) {
    const int step    = 32;
    int       count   = 0;
    char     *cstring = NULL;
    do {
        cstring = realloc(cstring, step * (count + 1));
        if (!cstring) return -1;
        char *p = &cstring[step * count];

        for (int i = 0; i < step; i++) {
            if (1 != recv(connfd, p, 1, 0)) {
                free(cstring);
                return -1;
            }
            if (*p++ == '\0') {
                *__cstring = cstring;
                return step * count + i;
            }
        }
    } while (true);
}

static void io_begin(int connfd, io_type_t type, const char *key, const value_t *value) {
    ssize_t      n __attribute__((unused));
    io_package_t pkg_head = {.type = type, .created = timestamp(true)};

    strncpy(pkg_head.key, key, sizeof(pkg_head.key));
    pkg_head.value.type   = value ? value->type : _value_undef;
    pkg_head.value.length = value ? value->length : 0;

    n = send(connfd, &pkg_head, sizeof(pkg_head), 0);
    assert(n == sizeof(pkg_head));
    logfD(logFmtHead logFmtKey " >>>%d send header of package with type %d", key, connfd, type);

    if (value) {
        n = send(connfd, value->data, value->length, 0);
        assert(n == value->length);
        logfD(logFmtHead logFmtKey " >>>%d send data of value with length %d", key, connfd, value->length);
    }
}

static int io_end(int connfd, const char *key) {
    int result = 0;

    if (sizeof(result) != recv(connfd, &result, sizeof(result), 0)) {
        return EIO;
    }
    logfD(logFmtHead logFmtKey " <<<%d recv result" logFmtRet, key, connfd, result);
    return result;
}

static int get(priv_t *priv, const char *key, const value_t **value, timestamp_t *duration) {
    int         ret    = 0;
    int         connfd = -1;
    timestamp_t _duration;
    value_t     value_head;
    value_t    *_value = NULL;

    ret = unix_connect(priv, &connfd);
    if (ret) return ret;

    io_begin(connfd, _io_get, key, NULL);

    // 1. recv duration
    if (sizeof(_duration) != recv(connfd, &_duration, sizeof(_duration), 0)) {
        ret = EIO;
        goto exit;
    }
    logfD(logFmtHead logFmtKey " <<<%d recv duration %ld", key, connfd, _duration);

    // 2. recv value head
    if (sizeof(value_head) != recv(connfd, &value_head, sizeof(value_head), 0)) {
        ret = EIO;
        goto exit;
    }
    logfD(logFmtHead logFmtKey " <<<%d recv header of value with type %d", key, connfd, value_head.type);

    _value = malloc(sizeof(value_t) + value_head.length);
    if (!_value) {
        ret = errno;
        goto exit;
    }
    memcpy(_value, &value_head, sizeof(value_t));

    // 3. recv value data
    if (_value->length != recv(connfd, _value->data, _value->length, 0)) {
        ret = EIO;
        goto exit;
    }
    logfD(logFmtHead logFmtKey " <<<%d recv data of value with length %d", key, connfd, _value->length);

    ret = io_end(connfd, key);
    if (ret) {
        goto exit;
    }

    unix_disconnect(priv, connfd);
    *value    = _value;
    *duration = _duration;
    return 0;

exit:
    unix_stream_discard(connfd);
    unix_disconnect(priv, connfd);
    free(_value);
    return ret;
}

static int info(priv_t *priv, const char *key, range_t *range, char **help_message, char ***__chain) {
    int    ret          = 0;
    int    connfd       = -1;
    int    chain_length = 0;
    char **chain        = NULL;

    ret = unix_connect(priv, &connfd);
    if (ret) return ret;

    io_begin(connfd, _io_info, key, NULL);

    // 1. recv range
    if (sizeof(*range) != recv(connfd,  , sizeof(*range), 0)) {
        ret = EIO;
        goto exit;
    }
    logfD(logFmtHead logFmtKey " <<<%d recv range", key, connfd);

    // 2. recv data
    if (range->type == _value_cstring) {
        char **choice = calloc(range->cstring.num + 1, sizeof(char *));
        if (!choice) {
            ret = errno;
            goto exit;
        }

        for (int i = 0; i < range->cstring.num; i++) {
            ret = unix_recv_cstring(connfd, &choice[i]);
            if (ret < 0) {
                goto exit;
            }
            logfD(logFmtHead logFmtKey " <<<%d recv choice \"%s\"", key, connfd, choice[i]);
        }
        range->cstring.choice = choice;
    }

    // 3. recv help massage
    ret = unix_recv_cstring(connfd, help_message);
    if (ret < 0) {
        goto exit;
    }
    logfD(logFmtHead logFmtKey " <<<%d recv help message \"%s\"", key, connfd, *help_message);

    // 4. recv chain
    if (sizeof(chain_length) != recv(connfd, &chain_length, sizeof(chain_length), 0)) {
        ret = EIO;
        goto exit;
    }
    logfD(logFmtHead logFmtKey " <<<%d recv length of chain %d", key, connfd, chain_length);
    chain = calloc(chain_length + 1, sizeof(char *));
    if (!chain) {
        ret = errno;
        goto exit;
    }

    for (int i = 0; i < chain_length; i++) {
        ret = unix_recv_cstring(connfd, &chain[i]);
        if (ret < 0) {
            goto exit;
        }
        logfD(logFmtHead logFmtKey " <<<%d chain \"%s\"", key, connfd, chain[i]);
    }
    *__chain = chain;

    ret = io_end(connfd, key);

exit:
    unix_disconnect(priv, connfd);
    return ret;
}

static int set(priv_t *priv, const char *key, const value_t *value) {
    int ret    = 0;
    int connfd = -1;

    ret = unix_connect(priv, &connfd);
    if (ret) return ret;

    io_begin(connfd, _io_set, key, value);
    ret = io_end(connfd, key);

    unix_disconnect(priv, connfd);
    return ret;
}

static int del(priv_t *priv, const char *key) {
    int ret    = 0;
    int connfd = -1;

    ret = unix_connect(priv, &connfd);
    if (ret) return ret;

    io_begin(connfd, _io_del, key, NULL);
    ret = io_end(connfd, key);

    unix_disconnect(priv, connfd);
    return ret;
}

static void destructor(priv_t *priv) {
    if (priv->shared) {
        __disconnect(priv->connfd);
        pthread_mutex_destroy(&priv->mutex);
    } else {
        free((void *)priv->target);
    }
    free(priv);
}

int prop_unix_storage(storage_t *ctx, const char *name, bool shared) {
    if (!(ctx->name = strdup(name))) {
        logfE(logFmtHead "fail to allocate name" logFmtErrno, logArgErrno);
        return errno;
    }
    priv_t *priv = malloc(sizeof(priv_t));
    if (!priv) {
        logfE(logFmtHead "fail to allocate priv" logFmtErrno, logArgErrno);
        free((void *)ctx->name);
        return errno;
    }
    priv->shared = shared;

    if (shared) {
        pthread_mutex_init(&priv->mutex, NULL);
        int ret = __connect(name, &priv->connfd);
        if (ret) {
            free(priv);
            free((void *)ctx->name);
            return ret;
        }
    } else {
        if (!(priv->target = strdup(name))) {
            logfE(logFmtHead "fail to allocate target" logFmtErrno, logArgErrno);
            free(priv);
            free((void *)ctx->name);
            return errno;
        }
    }

    ctx->priv       = priv;
    ctx->get        = (typeof(ctx->get))get;
    ctx->set        = (typeof(ctx->set))set;
    ctx->del        = (typeof(ctx->del))del;
    ctx->destructor = (typeof(ctx->destructor))destructor;
    return 0;
}

static int parse(storage_t *ctx, const char *name, const char **args) {
    const char *type_s = args[0];
    bool        shared = false;

    if (!type_s[0]) shared = false;
    else if (!strcmp(type_s, "temp")) shared = false;
    else if (!strcmp(type_s, "long")) shared = true;
    else return EINVAL;

    return prop_unix_storage(ctx, name, shared);
}

storage_parseConfig_t prop_unix_parseConfig = {
    .name    = "unix",
    .argName = "[<TYPE>],",
    .note    = "注册类型为unix的存储（与通过--children注册不同的是：不需要child具有ctrl "
               "server，且不支持“立即缓存”）。TYPE取值temp,long，默认为temp",
    .argNum  = 1,
    .parse   = parse,
};
