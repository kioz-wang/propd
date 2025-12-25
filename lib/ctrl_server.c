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
#include "cache.h"
#include "global.h"
#include "io.h"
#include "logger/logger.h"
#include "misc.h"
#include "named_mutex.h"
#include "route.h"
#include "thread_pool.h"
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

#define logFmtHead "[server@ctrl] "

static int register_child(const ctrl_package_register_child_t *child) {
    int       ret = 0;
    io_ctx_t *io;

    if (!child->num_cache_now && !child->num_prefix) {
        logfE(logFmtHead "deny to register empty child");
        return -1;
    }

    io = io_constructor_unix(child->name, child->name);
    if (!io) {
        return -1;
    }
    pthread_cleanup_push((void (*)(void *))io_destructor, io);

    for (uint32_t i = 0; i < child->num_cache_now; i++) {
        const value_t *value;
        timestamp_t    duration;
        const char    *key = child->cache_now_then_prefix[i];

        ret = named_mutex_lock(g_nmtx_ns, key);
        if (ret) {
            logfE(logFmtHead "fail to lock name" logFmtRet, ret);
            break;
        }
        ret = io_get(io, key, &value, &duration);
        if (ret) {
            logfE(logFmtHead "fail to get %s" logFmtRet, key, ret);
        } else {
            ret = cache_set(g_cache, key, value, duration);
            if (ret) {
                logfE(logFmtHead "fail to cache %s" logFmtRet, key, ret);
            }
        }
        named_mutex_unlock(g_nmtx_ns, key);
        if (ret) break;
    }

    if (!ret && child->num_prefix) {
        const char **prefix = (const char **)calloc(child->num_prefix, sizeof(char *));
        if (prefix) {
            for (uint32_t i = 0; i < child->num_prefix; i++)
                prefix[i] = child->cache_now_then_prefix[child->num_cache_now + i];
            ret = route_register(g_route, io, child->num_prefix, prefix);
            free(prefix);
        } else {
            ret = -1;
        }
    }

    pthread_cleanup_pop(ret);
    return ret;
}

static int unregister_child(const char *name) {
    int ret = 0;

    if (name[0]) {
        ret = route_unregister(g_route, name);
    } else {
        while (!route_unregister(g_route, NULL))
            ;
    }
    return ret;
}

static int dump_db_route(int sockfd, struct sockaddr_un *cliaddr) {
    int ret = -1;
    /* TODO */
    return ret;
}

static int dump_db_cache(int sockfd, struct sockaddr_un *cliaddr) {
    int ret = -1;
    /* TODO */
    return ret;
}

struct ctrl_worker_arg {
    const char           *name;
    const char          **cache_now;
    const char          **prefix;
    const ctrl_package_t *package;
    int                   sockfd;
    struct sockaddr_un    cliaddr;
};
typedef struct ctrl_worker_arg ctrl_worker_arg_t;

typedef ctrl_worker_arg_t ctrl_worker_cleanup_ctx_t;
static void               ctrl_worker_cleanup(ctrl_worker_cleanup_ctx_t *ctx) {
    free((void *)ctx->package);
    free(ctx);
}

static int ctrl_worker(ctrl_worker_arg_t *arg) {
    int ret = 0;

    pthread_cleanup_push((void (*)(void *))ctrl_worker_cleanup, arg);

    switch (arg->package->type) {
    case _ctrl_register_child: {
        ret = register_child(&arg->package->child);
    } break;
    case _ctrl_register_parent: {
        ret = ctrl_register_child(arg->package->name, arg->name, arg->cache_now, arg->prefix);
    } break;
    case _ctrl_unregister_child: {
        ret = unregister_child(arg->package->name);
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

struct ctrl_server_arg {
    const char        *name;
    uint32_t           num_prefix_max; /* as parent, limits recv buffer */
    const char       **cache_now;      /* as child */
    const char       **prefix;         /* as child */
    struct sockaddr_un servaddr;
    int                listenfd;
};
typedef struct ctrl_server_arg ctrl_server_arg_t;

static void ctrl_server_cleanup(ctrl_server_arg_t *arg) {
    unlink(arg->servaddr.sun_path);
    close(arg->listenfd);
    arrayfree_cstring(arg->prefix);
    arrayfree_cstring(arg->cache_now);
    free((void *)arg->name);
    free(arg);
}

static void *ctrl_server(ctrl_server_arg_t *arg) {
    pthread_cleanup_push((void (*)(void *))ctrl_server_cleanup, arg);

    do {
        size_t          pkg_length = sizeof(ctrl_package_t) + arg->num_prefix_max * NAME_MAX;
        ctrl_package_t *pkg        = (ctrl_package_t *)malloc(pkg_length);
        if (!pkg) {
            logfE(logFmtHead "fail to allocate package" logFmtErrno, logArgErrno);
            break;
        }
        struct sockaddr_un cliaddr = {0};

        ssize_t n =
            recvfrom(arg->listenfd, pkg, pkg_length, 0, (struct sockaddr *)&cliaddr, &(socklen_t){sizeof(cliaddr)});
        if (n == -1) {
            logfE(logFmtHead "fail to recv package" logFmtErrno, logArgErrno);
            free(pkg);
            break;
        }
        logfD(logFmtHead "recv package with length %ld", n);

        ctrl_worker_arg_t *ctrl_worker_arg = (ctrl_worker_arg_t *)malloc(sizeof(ctrl_worker_arg_t));
        if (!ctrl_worker_arg) {
            logfE(logFmtHead "fail to allocate ctrl_worker_arg" logFmtErrno, logArgErrno);
            free(pkg);
            break;
        }
        ctrl_worker_arg->name      = arg->name;
        ctrl_worker_arg->cache_now = arg->cache_now;
        ctrl_worker_arg->prefix    = arg->prefix;
        ctrl_worker_arg->package   = pkg; /* take ownership */
        ctrl_worker_arg->sockfd    = arg->listenfd;
        ctrl_worker_arg->cliaddr   = cliaddr;

        thread_pool_submit(g_pool, (int (*)(void *))ctrl_worker, ctrl_worker_arg, false);
    } while (true);

    pthread_cleanup_pop(true);
    return NULL;
}

int ctrl_start_server(const char *name, uint32_t num_prefix_max, const char *cache_now[], const char *prefix[],
                      pthread_t *tid) {
    int                ret = 0;
    ctrl_server_arg_t *arg = (ctrl_server_arg_t *)calloc(1, sizeof(ctrl_server_arg_t));
    if (!arg) return errno;

    arg->name = strdup(name);
    if (!arg->name) goto exit_ctx;

    arg->num_prefix_max = num_prefix_max;

    arg->cache_now = arraydup_cstring(cache_now, 0);
    if (!arg->cache_now) goto exit_ctx;

    arg->prefix = arraydup_cstring(prefix, 0);
    if (!arg->prefix) goto exit_ctx;

    arg->listenfd = socket(AF_LOCAL, SOCK_DGRAM, 0);
    if (arg->listenfd == -1) {
        logfE(logFmtHead "fail to get socket" logFmtErrno, logArgErrno);
        goto exit_ctx;
    }

    struct sockaddr_un *servaddr = &arg->servaddr;
    bzero(servaddr, sizeof(struct sockaddr_un));
    servaddr->sun_family = AF_LOCAL;
    snprintf(servaddr->sun_path, sizeof(servaddr->sun_path), PathFmt_CtrlServer, g_at, name);
    ret = bind(arg->listenfd, (const struct sockaddr *)servaddr, sizeof(*servaddr));
    if (ret) {
        logfE(logFmtHead "fail to bind %s" logFmtErrno, servaddr->sun_path, logArgErrno);
        goto exit_listenfd;
    }
    logfI(logFmtHead "bind %s", servaddr->sun_path);

    pthread_t _tid;
    ret = pthread_create(&_tid, NULL, (void *(*)(void *))ctrl_server, arg);
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
    unlink(arg->servaddr.sun_path);
exit_listenfd:
    close(arg->listenfd);
exit_ctx:
    arrayfree_cstring(arg->prefix);
    arrayfree_cstring(arg->cache_now);
    free((void *)arg->name);
    free(arg);
    return errno;
}
