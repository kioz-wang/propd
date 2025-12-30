/**
 * @file ctrl_server.c
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

#include "ctrl_server.h"
#include "builtin/builtin.h"
#include "global.h"
#include "infra/thread_pool.h"
#include "io.h"
#include "logger/logger.h"
#include "misc.h"
#include "route.h"
#include "storage.h"
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#define logFmtHead "[server::ctrl] "

struct worker_arg {
    const io_ctx_t       *io_ctx;
    const char           *name;
    const char          **cache_now;
    const char          **prefix;
    const ctrl_package_t *package; /* own */
    int                   sockfd;
    struct sockaddr_un    cliaddr;
};
typedef struct worker_arg worker_arg_t;

/**
 * @brief
 *
 * @param io_ctx
 * @param child
 * @return int errno
 */
static int register_child(const io_ctx_t *io_ctx, const ctrl_package_register_child_t *child) {
    int           ret     = 0;
    storage_ctx_t storage = {0};

    if (!child->num_cache_now && !child->num_prefix) {
        logfE(logFmtHead "deny to register empty child %s", child->name);
        return EINVAL;
    }

    if (constructor_unix(&storage, child->name, false)) {
        logfE(logFmtHead "register child %s but" logFmtErrno, child->name, logArgErrno);
        return errno;
    }
    pthread_cleanup_push((void (*)(void *))storage_destructor, &storage);

    for (uint32_t i = 0; i < child->num_cache_now; i++) {
        ret = io_update(io_ctx, child->cache_now_then_prefix[i], &storage);
        if (ret) break;
    }

    if (!ret && child->num_prefix) {
        const char **prefix = (const char **)calloc(child->num_prefix, sizeof(char *));
        if (!prefix) ret = errno;
        else {
            for (uint32_t i = 0; i < child->num_prefix; i++)
                prefix[i] = child->cache_now_then_prefix[child->num_cache_now + i];
            ret = route_register(io_ctx->route, &storage, child->num_prefix, prefix);
            free(prefix);
        }
    }

    pthread_cleanup_pop(ret);
    return ret;
}

/**
 * @brief
 *
 * @param io_ctx
 * @param name
 * @return int errno
 */
static int unregister_child(const io_ctx_t *io_ctx, const char *name) {
    return route_unregister(io_ctx->route, name[0] ? name : NULL);
}

static int dump_db_route(int sockfd, const struct sockaddr_un *cliaddr) {
    int ret = -1;
    /* TODO */
    return ret;
}

static int dump_db_cache(int sockfd, const struct sockaddr_un *cliaddr) {
    int ret = -1;
    /* TODO */
    return ret;
}

static void worker_cleanup(worker_arg_t *arg) {
    logfD(logFmtHead "cleanup worker");
    free((void *)arg->package);
    free(arg);
}

static int worker(worker_arg_t *arg) {
    int ret = 0;

    pthread_cleanup_push((void (*)(void *))worker_cleanup, arg);

    switch (arg->package->type) {
    case _ctrl_register_child: {
        ret = register_child(arg->io_ctx, &arg->package->child);
    } break;
    case _ctrl_register_parent: {
        ret = ctrl_register_child(arg->package->name, arg->name, arg->cache_now, arg->prefix);
    } break;
    case _ctrl_unregister_child: {
        ret = unregister_child(arg->io_ctx, arg->package->name);
    } break;
    case _ctrl_unregister_parent: {
        ret = ctrl_unregister_child(arg->package->name, arg->name);
    } break;
    case _ctrl_dump_db_route: {
        ret = dump_db_route(arg->sockfd, &arg->cliaddr);
    } break;
    case _ctrl_dump_db_cache: {
        ret = dump_db_cache(arg->sockfd, &arg->cliaddr);
    } break;
    default:
        logfD(logFmtHead "unknown package type %d", arg->package->type);
        goto exit;
    }

    ssize_t n = sendto(arg->sockfd, &ret, sizeof(ret), 0, (const struct sockaddr *)&arg->cliaddr, sizeof(arg->cliaddr));
    assert(n == sizeof(ret));
    logfD(logFmtHead "send result" logFmtRet, ret);

exit:
    pthread_cleanup_pop(true);
    return ret;
}

struct ctx {
    void              *thread_pool;
    const io_ctx_t    *io_ctx;
    const char        *name;           /* own */
    const char       **cache_now;      /* own, as child */
    const char       **prefix;         /* own, as child */
    uint32_t           num_prefix_max; /* own, as parent, limits recv buffer */
    int                sockfd;         /* own */
    struct sockaddr_un servaddr;       /* own */
};
typedef struct ctx ctx_t;

static void server_cleanup(ctx_t *ctx) {
    logfD(logFmtHead "cleanup server");
    unlink(ctx->servaddr.sun_path);
    close(ctx->sockfd);
    arrayfree_cstring(ctx->prefix);
    arrayfree_cstring(ctx->cache_now);
    free((void *)ctx->name);
    free(ctx);
}

static void *server(ctx_t *ctx) {
    pthread_cleanup_push((void (*)(void *))server_cleanup, ctx);

    size_t pkg_length = sizeof(ctrl_package_t) + ctx->num_prefix_max * NAME_MAX;
    do {
        ctrl_package_t *pkg = malloc(pkg_length);
        if (!pkg) {
            logfE(logFmtHead "fail to allocate package" logFmtErrno, logArgErrno);
            break;
        }
        struct sockaddr_un cliaddr = {0};

        worker_arg_t *arg = malloc(sizeof(worker_arg_t));
        if (!arg) {
            logfE(logFmtHead "fail to allocate ctrl_worker_arg" logFmtErrno, logArgErrno);
            free(pkg);
            break;
        }

        pthread_cleanup_push(free, pkg);
        pthread_cleanup_push(free, arg);
        ssize_t n =
            recvfrom(ctx->sockfd, pkg, pkg_length, 0, (struct sockaddr *)&cliaddr, &(socklen_t){sizeof(cliaddr)});
        if (n == -1) {
            logfE(logFmtHead "fail to recv package" logFmtErrno, logArgErrno);
            free(arg);
            free(pkg);
            break;
        }
        logfD(logFmtHead "recv package with length %ld", n);
        pthread_cleanup_pop(false);
        pthread_cleanup_pop(false);

        arg->io_ctx    = ctx->io_ctx;
        arg->name      = ctx->name;
        arg->cache_now = ctx->cache_now;
        arg->prefix    = ctx->prefix;
        arg->package   = pkg; /* take ownership */
        arg->sockfd    = ctx->sockfd;
        arg->cliaddr   = cliaddr;

        thread_pool_submit(ctx->thread_pool, (int (*)(void *))worker, arg, false);
    } while (true);

    pthread_cleanup_pop(true);
    return NULL;
}

int start_ctrl_server(const char *name, void *thread_pool, const io_ctx_t *io_ctx, const char *cache_now[],
                      const char *prefix[], uint32_t num_prefix_max, pthread_t *tid) {
    int    ret = 0;
    ctx_t *ctx = calloc(1, sizeof(ctx_t));
    if (!ctx) return errno;

    ctx->thread_pool = thread_pool;
    ctx->io_ctx      = io_ctx;

    ctx->name = strdup(name);
    if (!ctx->name) goto exit_ctx;
    ctx->cache_now = arraydup_cstring(cache_now, 0);
    if (!ctx->cache_now) goto exit_ctx;
    ctx->prefix = arraydup_cstring(prefix, 0);
    if (!ctx->prefix) goto exit_ctx;
    ctx->num_prefix_max = num_prefix_max;

    ctx->sockfd = socket(AF_LOCAL, SOCK_DGRAM, 0);
    if (ctx->sockfd == -1) {
        logfE(logFmtHead "fail to get socket" logFmtErrno, logArgErrno);
        goto exit_ctx;
    }

    struct sockaddr_un *servaddr = &ctx->servaddr;
    bzero(servaddr, sizeof(struct sockaddr_un));
    servaddr->sun_family = AF_LOCAL;
    snprintf(servaddr->sun_path, sizeof(servaddr->sun_path), PathFmt_CtrlServer, g_at, name);
    ret = bind(ctx->sockfd, (const struct sockaddr *)servaddr, sizeof(*servaddr));
    if (ret) {
        logfE(logFmtHead "fail to bind %s" logFmtErrno, servaddr->sun_path, logArgErrno);
        goto exit_listenfd;
    }
    logfI(logFmtHead "bind %s", servaddr->sun_path);

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
    arrayfree_cstring(ctx->prefix);
    arrayfree_cstring(ctx->cache_now);
    free((void *)ctx->name);
    free(ctx);
    return errno;
}
