/**
 * @file io_server.c
 * @author kioz.wang (never.had@outlook.com)
 * @brief
 * @version 0.1
 * @date 2025-12-03
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

#define _GNU_SOURCE
#include "io_server.h"
#include "global.h"
#include "logger/logger.h"
#include "named_mutex.h"
#include "thread_pool.h"
#include "value.h"
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#define logFmtHead  "[server@io] "
#define logFmtKey   "<%s> "
#define logFmtValue "\"%s\""

static int cred_check(void *credbook, const struct ucred *cred, io_type_t type, const char *key) {
    int ret = 0;
    /* TODO */
    return ret;
}

static int local_get(const char *key, const value_t **value, timestamp_t *duration) {
    int  ret = 0;
    io_t io;

    if (g_cache) {
        ret = cache_get(g_cache, key, value, duration);
        if (ret != ENOENT) return ret;
    }

    ret = route_match(g_route, key, &io);
    if (ret) return ret;

    ret = named_mutex_lock(g_nmtx_ns, key);
    if (ret) {
        logfE(logFmtHead logFmtKey "fail to lock key" logFmtRet, key, ret);
        return ret;
    }
    ret = io_get(&io, key, value, duration);
    if (!ret) {
        if (g_cache) cache_set(g_cache, key, *value, *duration);
    }
    named_mutex_unlock(g_nmtx_ns, key);

    return ret;
}

static int local_set(const char *key, const value_t *value) {
    int  ret = 0;
    io_t io;

    ret = route_match(g_route, key, &io);
    if (ret) return ret;

    ret = named_mutex_lock(g_nmtx_ns, key);
    if (ret) {
        logfE(logFmtHead logFmtKey "fail to lock key" logFmtRet, key, ret);
        return ret;
    }
    ret = io_set(&io, key, value);
    if (!ret) {
        if (g_cache) cache_set(g_cache, key, value, 0);
    }
    named_mutex_unlock(g_nmtx_ns, key);

    return ret;
}

static int local_del(const char *key) {
    int  ret = 0;
    io_t io;

    ret = route_match(g_route, key, &io);
    if (ret) return ret;

    ret = named_mutex_lock(g_nmtx_ns, key);
    if (ret) {
        logfE(logFmtHead logFmtKey "fail to lock key" logFmtRet, key, ret);
        return ret;
    }
    ret = io_del(&io, key);
    if (!ret) {
        if (g_cache) cache_del(g_cache, key);
    }
    named_mutex_unlock(g_nmtx_ns, key);

    return ret;
}

extern void unix_stream_discard(int connfd);

static int get(int connfd, const struct ucred *cred, const char *key) {
    int            ret = 0;
    ssize_t        n;
    const value_t *value    = NULL;
    timestamp_t    duration = 0;

    ret = cred_check(g_credbook, cred, _io_get, key);

    if (ret == 0) {
        ret = local_get(key, &value, &duration);
    }

    n = send(connfd, &duration, sizeof(duration), 0);
    assert(n == sizeof(duration)); /* TODO pthread_exit */
    logfD(logFmtHead logFmtKey ">>>%d send duration %ld", key, connfd, duration);
    if (ret == 0) {
        n = send(connfd, value, sizeof(value_t) + value->length, 0);
        assert(n == sizeof(value_t) + value->length); /* TODO pthread_exit */
        logfD(logFmtHead logFmtKey ">>>%d send value with type %d length %d", key, connfd, value->type, value->length);
    } else {
        value_t undef = {.type = _value_undef, .length = 0};
        n             = send(connfd, &undef, sizeof(undef), 0);
        assert(n == sizeof(undef)); /* TODO pthread_exit */
        logfD(logFmtHead logFmtKey ">>>%d send undef value", key, connfd);
    }
    return ret;
}

static int set(int connfd, const struct ucred *cred, const char *key, const value_t *value_head) {
    int      ret = 0;
    ssize_t  n;
    value_t *value = (value_t *)malloc(sizeof(value_t) + value_head->length);
    if (!value) {
        logfE(logFmtHead logFmtKey "<<<%d no memory to recv data of value, discard it", key, connfd);
        unix_stream_discard(connfd);
        return ENOMEM;
    }

    memcpy(value, value_head, sizeof(value_t));
    n = recv(connfd, value->data, value->length, 0);
    assert(n == value->length); /* TODO pthread_exit */
    logfD(logFmtHead logFmtKey "<<<%d recv data of value with length %d", key, connfd, value->length);

    ret = cred_check(g_credbook, cred, _io_set, key);

    if (ret == 0) {
        ret = local_set(key, value);
    }
    return ret;
}

static int del(const struct ucred *cred, const char *key) {
    int ret = 0;

    ret = cred_check(g_credbook, cred, _io_del, key);

    if (ret == 0) {
        ret = local_del(key);
    }
    return ret;
}

struct io_conn_arg {
    int          connfd;
    struct ucred cred;
};
typedef struct io_conn_arg io_conn_arg_t;

typedef io_conn_arg_t io_conn_cleanup_ctx_t;
static void           io_conn_cleanup(io_conn_cleanup_ctx_t *ctx) {
    close(ctx->connfd);
    free(ctx);
}

static int io_conn(io_conn_arg_t *arg) {
    int connfd = arg->connfd;
    int result = 0;

    pthread_cleanup_push((void (*)(void *))io_conn_cleanup, arg);

    for (;;) {
        io_package_t pkg_head;
        ssize_t      n;

        n = recv(connfd, &pkg_head, sizeof(pkg_head), 0);
        if (n == -1) {
            logfE(logFmtHead "<<<%d fail to recv header of package" logFmtErrno, connfd, logArgErrno);
            break;
        } else if (n == 0) {
            logfV(logFmtHead "<<<%d disconnect", connfd);
            break;
        }
        const char *key = pkg_head.key;
        logfD(logFmtHead logFmtKey "<<<%d recv header of package with type %d", key, connfd, pkg_head.type);

        switch (pkg_head.type) {
        case _io_get:
            result = get(connfd, &arg->cred, key);
            break;
        case _io_set:
            result = set(connfd, &arg->cred, key, &pkg_head.value);
            break;
        case _io_del:
            result = del(&arg->cred, key);
            break;
        }

        n = send(connfd, &result, sizeof(result), 0);
        assert(n == sizeof(result)); /* TODO pthread_exit */
        logfD(logFmtHead logFmtKey ">>>%d send result" logFmtRet, key, connfd, result);
    }
    pthread_cleanup_pop(true);
    return result;
}

typedef struct {
    const int  *listenfd;
    const char *path;
} io_server_cleanup_ctx_t;
static void io_server_cleanup(io_server_cleanup_ctx_t *ctx) {
    if (*ctx->listenfd) close(*ctx->listenfd);
    if (ctx->path) unlink(ctx->path);
}

static void *io_server(io_server_t *ctx) {
    int ret = 0;

    io_server_cleanup_ctx_t cleanup_ctx = {0};
    pthread_cleanup_push((void (*)(void *))io_server_cleanup, &cleanup_ctx);

    struct sockaddr_un servaddr = {0};
    int                listenfd = socket(AF_LOCAL, SOCK_STREAM, 0);
    if (listenfd == -1) {
        logfE(logFmtHead "fail to get socket" logFmtErrno, logArgErrno);
        goto exit;
    }
    cleanup_ctx.listenfd = &listenfd;

    servaddr.sun_family = AF_LOCAL;
    snprintf(servaddr.sun_path, sizeof(servaddr.sun_path), PathFmt_IOServer, g_at, ctx->name);
    ret = bind(listenfd, (const struct sockaddr *)&servaddr, sizeof(servaddr));
    if (ret) {
        logfE(logFmtHead "fail to bind %s" logFmtErrno, servaddr.sun_path, logArgErrno);
        goto exit;
    }
    listen(listenfd, 5);
    logfI(logFmtHead "listen at %s", servaddr.sun_path);
    cleanup_ctx.path = servaddr.sun_path;

    for (;;) {
        struct sockaddr_un cliaddr = {0};
        int                connfd  = -1;

        io_conn_arg_t *arg = (io_conn_arg_t *)malloc(sizeof(io_conn_arg_t));
        if (!arg) {
            logfE(logFmtHead "fail to allocate io_conn_arg" logFmtErrno, logArgErrno);
            break;
        }

        if ((connfd = accept(listenfd, (struct sockaddr *)&cliaddr, &(socklen_t){sizeof(cliaddr)})) < 0) {
            logfE(logFmtHead "fail to accept" logFmtErrno, logArgErrno);
            free(arg);
            if (errno == EINTR) continue;
            break;
        }

        arg->connfd = connfd;
        getsockopt(connfd, SOL_SOCKET, SO_PEERCRED, &arg->cred, &(socklen_t){sizeof(arg->cred)});
        logfV(logFmtHead "accept p%d,u%d,g%d path %s as %d", arg->cred.pid, arg->cred.uid, arg->cred.gid,
              cliaddr.sun_path[0] ? cliaddr.sun_path : "?", connfd);
        thread_pool_submit(g_pool, (int (*)(void *))io_conn, arg, false);
    }

exit:
    pthread_cleanup_pop(true);
    return NULL;
}

io_server_t *io_start_server(const char *name, bool join) {
    int ret = 0;

    io_server_t *ctx = (io_server_t *)calloc(1, sizeof(io_server_t));
    if (!ctx) return NULL;

    ctx->name = strdup(name);
    if (!ctx->name) {
        free(ctx);
        return NULL;
    }

    ret = pthread_create(&ctx->tid, NULL, (void *(*)(void *))io_server, ctx);
    if (ret) {
        logfE(logFmtHead "fail to pthread_create" logFmtRet, ret);
        free((void *)ctx->name);
        free(ctx);
        return NULL;
    }
    if (join) pthread_join(ctx->tid, NULL);
    else pthread_detach(ctx->tid);
    return ctx;
}

void io_stop_server(io_server_t *ctx) {
    if (ctx) {
        pthread_cancel(ctx->tid);
        free((void *)ctx->name);
        free(ctx);
    }
}
