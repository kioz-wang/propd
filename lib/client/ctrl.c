/**
 * @file ctrl_client.c
 * @author kioz.wang (never.had@outlook.com)
 * @brief
 * @version 0.1
 * @date 2025-12-04
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
#include "prop.h"
#include "server/ctrl.h"
#include "global.h"
#include "misc.h"
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define logFmtHead "[client::ctrl] "

struct ctrl_context {
    int                sockfd;
    const char        *server;
    struct sockaddr_un servaddr;
    size_t             package_length;
    ctrl_package_t    *package;
};
typedef struct ctrl_context ctrl_context_t;

static int ctrl_init(ctrl_context_t *ctx, const char *server, size_t delta_length) {
    struct sockaddr_un cliaddr = {0};
    ctx->sockfd                = socket(AF_LOCAL, SOCK_DGRAM, 0);
    if (ctx->sockfd == -1) {
        logfE(logFmtHead "fail to get socket" logFmtErrno, logArgErrno);
        return errno;
    }

    cliaddr.sun_family  = AF_LOCAL;
    cliaddr.sun_path[0] = '\0';
    random_alnum(&cliaddr.sun_path[1], sizeof(cliaddr.sun_path) - 2);
    cliaddr.sun_path[sizeof(cliaddr.sun_path) - 1] = 'X';

    if (bind(ctx->sockfd, (const struct sockaddr *)&cliaddr, sizeof(struct sockaddr_un))) {
        logfE(logFmtHead "fail to bind" logFmtErrno, logArgErrno);
        close(ctx->sockfd);
        return errno;
    }

    ctx->server = server;

    bzero(&ctx->servaddr, sizeof(struct sockaddr_un));
    ctx->servaddr.sun_family = AF_LOCAL;
    snprintf(ctx->servaddr.sun_path, sizeof(ctx->servaddr.sun_path), PathFmt_CtrlServer, g_at, server);

    ctx->package_length = sizeof(ctrl_package_t) + delta_length;
    ctx->package        = (ctrl_package_t *)malloc(ctx->package_length);
    if (!ctx->package) {
        logfE(logFmtHead "fail to allocate package" logFmtErrno, logArgErrno);
        return errno;
    }
    return 0;
}

static int ctrl_update(ctrl_context_t *ctx) {
    int ret = 0;
    if ((ssize_t)ctx->package_length != sendto(ctx->sockfd, ctx->package, ctx->package_length, 0,
                                               (const struct sockaddr *)&ctx->servaddr, sizeof(struct sockaddr_un)))
        ret = EIO;
    else
        logfD(logFmtHead ">>>%s send packge with type %d length %ld", ctx->server, ctx->package->type,
              ctx->package_length);
    free(ctx->package);
    return ret;
}

static int ctrl_final0(ctrl_context_t *ctx, void **data, int *data_length) {
    int     ret                       = 0;
    ssize_t n __attribute__((unused)) = 0;

    switch (ctx->package->type) {
    case _ctrl_dump_db_route:
    case _ctrl_dump_db_cache: {
        assert(data && data_length);

        int length = 0;
        n          = recvfrom(ctx->sockfd, &length, sizeof(length), 0, NULL, NULL);
        assert(n == sizeof(length));

        *data        = NULL;
        *data_length = length;
        if (length) {
            void *_data = malloc(length);
            *data       = _data;
            if (!_data) {
                logfE(logFmtHead "fail to allocate data" logFmtErrno, logArgErrno);
                int discard;
                recvfrom(ctx->sockfd, &discard, sizeof(discard), 0, NULL, NULL); /* discard data */
                recvfrom(ctx->sockfd, &discard, sizeof(discard), 0, NULL, NULL); /* discard result */
                close(ctx->sockfd);
                return errno;
            } else {
                n = recvfrom(ctx->sockfd, _data, length, 0, NULL, NULL);
                assert(n == length);
                logfD(logFmtHead "<<<%s recv data with length %d", ctx->server, length);
            }
        }
    }
    }

    if (sizeof(ret) != recvfrom(ctx->sockfd, &ret, sizeof(ret), 0, NULL, NULL)) ret = EIO;
    else logfD(logFmtHead "<<<%s recv result" logFmtRet, ctx->server, ret);

    close(ctx->sockfd);
    return ret;
}

int prop_register_child(const char *server, const char *name, const char *cache_now[], const char *prefix[]) {
    int            ret           = 0;
    uint32_t       num_cache_now = 0;
    uint32_t       num_prefix    = 0;
    ctrl_context_t ctx;

    if (cache_now)
        while (cache_now[num_cache_now])
            num_cache_now++;
    if (prefix)
        while (prefix[num_prefix])
            num_prefix++;
    ret = ctrl_init(&ctx, server, (num_prefix + num_cache_now) * NAME_MAX);
    if (ret) {
        goto exit;
    }

    ctx.package->type = _ctrl_register_child;

    ctrl_package_register_child_t *child = &ctx.package->child;
    strncpy(child->name, name, sizeof(child->name));
    child->num_cache_now = num_cache_now;
    child->num_prefix    = num_prefix;
    for (uint32_t i = 0; i < num_cache_now; i++) {
        strncpy(child->cache_now_then_prefix[i], cache_now[i], NAME_MAX);
    }
    for (uint32_t i = 0; i < num_prefix; i++) {
        strncpy(child->cache_now_then_prefix[i + num_cache_now], prefix[i], NAME_MAX);
    }

    ret = ctrl_update(&ctx);
    if (!ret) ret = ctrl_final0(&ctx, NULL, NULL);

exit:
    if (ret) logfE(logFmtHead "fail to register <%s> into <%s>" logFmtRet, name, server, ret);
    else logfI(logFmtHead "register <%s> into <%s>", name, server);
    return ret;
}

static int ctrl_generic0(const char *server, ctrl_type_t type, const char *name) {
    int            ret = 0;
    ctrl_context_t ctx;

    ret = ctrl_init(&ctx, server, 0);
    if (ret) {
        return ret;
    }
    ctx.package->type = type;
    strncpy(ctx.package->name, name, sizeof(ctx.package->name));
    ret = ctrl_update(&ctx);
    if (!ret) ret = ctrl_final0(&ctx, NULL, NULL);
    return ret;
}

int prop_register_parent(const char *server, const char *name) {
    int ret = ctrl_generic0(server, _ctrl_register_parent, name);
    if (ret) logfE(logFmtHead "fail to register self<%s> into <%s>" logFmtRet, server, name, ret);
    else logfI(logFmtHead "register self<%s> into <%s>", server, name);
    return ret;
}

int prop_unregister_child(const char *server, const char *name) {
    int ret = 0;
    ret     = ctrl_generic0(server, _ctrl_unregister_child, name);
    if (ret) logfE(logFmtHead "fail to unregister <%s> from <%s>" logFmtRet, name, server, ret);
    else logfI(logFmtHead "unregister <%s> from <%s>", name, server);
    return ret;
}

int prop_unregister_parent(const char *server, const char *name) {
    int ret = ctrl_generic0(server, _ctrl_unregister_parent, name);
    if (ret) logfE(logFmtHead "fail to unregister self<%s> from <%s>" logFmtRet, server, name, ret);
    else logfI(logFmtHead "unregister self<%s> from <%s>", server, name);
    return ret;
}

static int ctrl_generic1(const char *server, ctrl_type_t type, void **data, int *data_length) {
    int            ret = 0;
    ctrl_context_t ctx;

    ret = ctrl_init(&ctx, server, 0);
    if (ret) {
        return ret;
    }
    ctx.package->type = type;
    ret               = ctrl_update(&ctx);
    if (!ret) ret = ctrl_final0(&ctx, data, data_length);
    return ret;
}

int prop_dump_db_route(const char *server, void **db) {
    int db_length;
    return ctrl_generic1(server, _ctrl_dump_db_route, db, &db_length);
    /* TODO what's format of db? cstring or complex structure? */
}

int prop_dump_db_cache(const char *server, void **db) {
    int db_length;
    return ctrl_generic1(server, _ctrl_dump_db_cache, db, &db_length);
    /* TODO what's format of db? cstring or complex structure? */
}
