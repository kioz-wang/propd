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
#include "infra/thread_pool.h"
#include "logger/logger.h"
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define logFmtHead "[server::io] "

static int cred_check(const void *credbook, const struct ucred *cred, io_type_t type, const char *key) {
    int ret = 0;
    /* TODO */
    return ret;
}

extern void unix_stream_discard(int connfd);

struct worker_arg {
    const void     *credbook;
    const io_ctx_t *io_ctx;
    int             connfd; /* own */
    struct ucred    cred;   /* own */
};
typedef struct worker_arg worker_arg_t;

static int get(const worker_arg_t *arg, int connfd, const char *key) {
    int            ret = 0;
    ssize_t        n;
    const value_t *value    = NULL;
    timestamp_t    duration = 0;

    ret = cred_check(arg->credbook, &arg->cred, _io_get, key);

    if (!ret) {
        ret = io_get(arg->io_ctx, key, &value, &duration);
    }

    n = send(connfd, &duration, sizeof(duration), MSG_NOSIGNAL);
    if (n != sizeof(duration)) goto exit;
    logfD(logFmtHead logFmtKey " >>>%d send duration %ld", key, connfd, duration);

    if (!ret) {
        n = send(connfd, value, sizeof(value_t) + value->length, MSG_NOSIGNAL);
        if (n != sizeof(value_t) + value->length) goto exit;
        logfD(logFmtHead logFmtKey " >>>%d send value with type %d length %d", key, connfd, value->type, value->length);
    } else {
        value_t undef = {.type = _value_undef, .length = 0};
        n             = send(connfd, &undef, sizeof(undef), MSG_NOSIGNAL);
        if (n != sizeof(undef)) goto exit;
        logfD(logFmtHead logFmtKey " >>>%d send undef value", key, connfd);
    }

    free((void *)value);
    return ret;

exit:
    free((void *)value);
    return EIO;
}

static int set(const worker_arg_t *arg, int connfd, const char *key, const value_t *value_head) {
    int      ret = 0;
    ssize_t  n;
    value_t *value = NULL;

    value = malloc(sizeof(value_t) + value_head->length);
    if (!value) {
        logfE(logFmtHead logFmtKey " <<<%d no memory to recv data of value, discard it", key, connfd);
        unix_stream_discard(connfd);
        return ENOMEM;
    }

    memcpy(value, value_head, sizeof(value_t));
    n = recv(connfd, value->data, value->length, 0);
    if (n != value->length) goto exit;
    logfD(logFmtHead logFmtKey " <<<%d recv data of value with length %d", key, connfd, value->length);

    ret = cred_check(arg->credbook, &arg->cred, _io_set, key);

    if (!ret) {
        ret = io_set(arg->io_ctx, key, value);
    }

    free((void *)value);
    return ret;

exit:
    free((void *)value);
    return EIO;
}

static int del(const worker_arg_t *arg, const char *key) {
    int ret = 0;

    ret = cred_check(arg->credbook, &arg->cred, _io_del, key);

    if (!ret) {
        ret = io_del(arg->io_ctx, key);
    }
    return ret;
}

static int worker(worker_arg_t *arg) {
    int ret    = 0;
    int connfd = arg->connfd;

    for (;;) {
        io_package_t pkg_head;
        ssize_t      n;
        int          result = 0;

        n = recv(connfd, &pkg_head, sizeof(pkg_head), 0);
        if (n == -1) {
            logfE(logFmtHead "<<<%d fail to recv header of package" logFmtErrno, connfd, logArgErrno);
            ret = errno;
            break;
        } else if (n == 0) {
            logfV(logFmtHead "<<<%d disconnect", connfd);
            break;
        }
        logfD(logFmtHead logFmtKey " <<<%d recv header of package with type %d, created at %lxms", pkg_head.key, connfd,
              pkg_head.type, timestamp_to_ms(pkg_head.created));

        switch (pkg_head.type) {
        case _io_get:
            result = get(arg, connfd, pkg_head.key);
            break;
        case _io_set:
            result = set(arg, connfd, pkg_head.key, &pkg_head.value);
            break;
        case _io_del:
            result = del(arg, pkg_head.key);
            break;
        }

        n = send(connfd, &result, sizeof(result), MSG_NOSIGNAL);
        if (n != sizeof(result)) {
            ret = EIO;
            break;
        }
        logfD(logFmtHead logFmtKey " >>>%d send result" logFmtRet, pkg_head.key, connfd, result);
    }

    close(arg->connfd);
    free(arg);
    return ret;
}

struct ctx {
    void              *thread_pool;
    const void        *credbook;
    const io_ctx_t    *io_ctx;
    int                sockfd;   /* own */
    struct sockaddr_un servaddr; /* own */
};
typedef struct ctx ctx_t;

static void server_cleanup(ctx_t *ctx) {
    logfD(logFmtHead "cleanup server");
    unlink(ctx->servaddr.sun_path);
    close(ctx->sockfd);
    free(ctx);
}

static void *server(ctx_t *ctx) {
    pthread_cleanup_push((void (*)(void *))server_cleanup, ctx);

    for (;;) {
        struct sockaddr_un cliaddr = {0};

        worker_arg_t *arg = malloc(sizeof(worker_arg_t));
        if (!arg) {
            logfE(logFmtHead "fail to allocate worker_arg" logFmtErrno, logArgErrno);
            break;
        }

        arg->credbook = ctx->credbook;
        arg->io_ctx   = ctx->io_ctx;

        pthread_cleanup_push(free, arg);
        arg->connfd = accept(ctx->sockfd, (struct sockaddr *)&cliaddr, &(socklen_t){sizeof(cliaddr)});
        if (arg->connfd < 0) {
            logfE(logFmtHead "fail to accept" logFmtErrno, logArgErrno);
            free(arg);
            if (errno == EINTR) continue;
            break;
        }
        pthread_cleanup_pop(false);

        getsockopt(arg->connfd, SOL_SOCKET, SO_PEERCRED, &arg->cred, &(socklen_t){sizeof(arg->cred)});
        logfV(logFmtHead "accept p%d,u%d,g%d path %s as %d", arg->cred.pid, arg->cred.uid, arg->cred.gid,
              cliaddr.sun_path[0] ? cliaddr.sun_path : "?", arg->connfd);
        thread_pool_submit(ctx->thread_pool, (int (*)(void *))worker, arg, false);
    }

    pthread_cleanup_pop(true);
    return NULL;
}

int start_io_server(const char *name, void *thread_pool, const void *credbook, const io_ctx_t *io_ctx, pthread_t *tid) {
    int    ret = 0;
    ctx_t *ctx = calloc(1, sizeof(ctx_t));
    if (!ctx) return errno;

    ctx->thread_pool = thread_pool;
    ctx->credbook    = credbook;
    ctx->io_ctx      = io_ctx;

    ctx->sockfd = socket(AF_LOCAL, SOCK_STREAM, 0);
    if (ctx->sockfd == -1) {
        logfE(logFmtHead "fail to get socket" logFmtErrno, logArgErrno);
        goto exit_ctx;
    }

    struct sockaddr_un *servaddr = &ctx->servaddr;
    bzero(servaddr, sizeof(struct sockaddr_un));
    servaddr->sun_family = AF_LOCAL;
    snprintf(servaddr->sun_path, sizeof(servaddr->sun_path), PathFmt_IOServer, g_at, name);
    ret = bind(ctx->sockfd, (const struct sockaddr *)servaddr, sizeof(*servaddr));
    if (ret) {
        logfE(logFmtHead "fail to bind %s" logFmtErrno, servaddr->sun_path, logArgErrno);
        goto exit_listenfd;
    }
    ret = listen(ctx->sockfd, 5); /* TODO why 5? */
    if (ret) {
        logfE(logFmtHead "fail to listen at %s" logFmtErrno, servaddr->sun_path, logArgErrno);
        goto exit_sun_path;
    }
    logfI(logFmtHead "listen at %s", servaddr->sun_path);

    pthread_t _tid;
    ret = pthread_create(&_tid, NULL, (void *(*)(void *))server, ctx);
    if (ret) {
        logfE(logFmtHead "fail to pthread_create" logFmtRet, ret);
        errno = ret;
        goto exit_sun_path;
    }

    if (tid) {
        pthread_detach(_tid);
        *tid = _tid;
    } else {
        pthread_join(_tid, NULL);
    }
    return 0;

exit_sun_path:
    unlink(ctx->servaddr.sun_path);
exit_listenfd:
    close(ctx->sockfd);
exit_ctx:
    free(ctx);
    return errno;
}
