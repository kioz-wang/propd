/**
 * @file tcp.c
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
#include <errno.h>

#define logFmtHead "[storage::(tcp)] "

struct priv {
    /* TODO */;
};
typedef struct priv priv_t;

int prop_tcp_storage(storage_t *ctx, const char *name, const char *ip, unsigned short port) {
    if (!(ctx->name = strdup(name))) {
        logfE(logFmtHead "fail to allocate name" logFmtErrno, logArgErrno);
        return errno;
    }

    priv_t *priv = (priv_t *)malloc(sizeof(priv_t));
    if (!priv) {
        logfE(logFmtHead "fail to allocate priv" logFmtErrno, logArgErrno);
        return errno;
    }

    ctx->priv       = priv;
    ctx->destructor = free;
    return EOPNOTSUPP;
}

static int parse(storage_t *ctx, const char *name, const char **args) {
    unsigned short port = strtoul(args[1], NULL, 0);
    return prop_tcp_storage(ctx, name, args[0], port);
}

storage_parseConfig_t prop_tcp_parseConfig = {
    .name    = "tcp",
    .argName = "<IP>,<PORT>,",
    .note    = "注册类型为tcp的本地IO。IP，PORT是tcp IO需要连接的目标",
    .argNum  = 2,
    .parse   = parse,
};
