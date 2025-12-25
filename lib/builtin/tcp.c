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
#include "logger/logger.h"
#include <errno.h>

#define logFmtHead  "[bridge][tcp] "
#define logFmtKey   "<%s> "
#define logFmtValue "\"%s\""

struct priv {
    /* TODO */;
};
typedef struct priv priv_t;

io_ctx_t *io_constructor_tcp(const char *name, const char *ip, unsigned short port) {
    io_ctx_t *ctx = (io_ctx_t *)calloc(1, sizeof(io_ctx_t));
    if (!ctx) return NULL;

    if (!(ctx->name = strdup(name))) {
        logfE(logFmtHead "fail to allocate name" logFmtErrno, logArgErrno);
        return NULL;
    }

    priv_t *priv = (priv_t *)malloc(sizeof(priv_t));
    if (!priv) {
        logfE(logFmtHead "fail to allocate priv" logFmtErrno, logArgErrno);
        return NULL;
    }

    ctx->priv       = priv;
    ctx->destructor = free;
    return ctx;
}

static io_ctx_t *parse(const char *name, const char **args) {
    unsigned short port = strtoul(args[1], NULL, 0);
    return io_constructor_tcp(name, args[0], port);
}

io_parseConfig_t tcp_parseConfig = {
    .name    = "tcp",
    .argName = "<IP>,<PORT>",
    .note    = "注册类型为tcp的本地IO。IP，PORT是tcp IO需要连接的目标",
    .argNum  = 2,
    .parse   = parse,
};
