/**
 * @file memory.c
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

#include "builtin.h"
#include "cache.h"
#include "global.h"
#include "logger/logger.h"
#include "memio/layout.h"
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#define logFmtHead "[storage::(memory)] "

struct priv {
    void        *base;
    const pos_t *layout;
};
typedef struct priv priv_t;

static void memory_deinit(priv_t *priv) {
    /* TODO */
    munmap(priv->base, layout_length(priv->layout));
    layout_destroy((pos_t *)priv->layout);
    free(priv);
}

static int memory_get(const priv_t *priv, const char *key, const value_t **value, timestamp_t *duration) {
    const pos_t *pos = pos_search_by_name(priv->layout, key);
    if (!pos) {
        return ENOENT;
    }

    value_t *_value = malloc(sizeof(value_t) + pos->length);
    if (!_value) {
        return errno;
    }

    pos_read(pos, priv->base, _value->data, pos->length);
    _value->length = pos->length;
    _value->type   = pos->length > sizeof(uint32_t) ? _value_data : _value_u32;
    *value         = _value;
    *duration      = DURATION_INF;
    return 0;
}

int constructor_memory(storage_ctx_t *ctx, const char *name, long phy, const void *layout) {
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

    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd == -1) {
        logfE(logFmtHead "fail to open /dev/mem" logFmtErrno, logArgErrno);
        free(priv);
        free((void *)ctx->name);
        return EIO;
    }
    /* TODO propd_mmap/munmap */
    uint32_t len = layout_length(layout);
    priv->base   = mmap(0, len, PROT_READ, MAP_SHARED, fd, phy);
    close(fd);
    if (!priv->base) {
        logfE(logFmtHead "fail to mmap(%lx,%x)" logFmtErrno, phy, len, logArgErrno);
        free(priv);
        free((void *)ctx->name);
        return errno;
    }
    priv->layout = layout;

    ctx->priv       = priv;
    ctx->get        = (typeof(ctx->get))memory_get;
    ctx->set        = NULL;
    ctx->del        = NULL;
    ctx->destructor = (typeof(ctx->destructor))memory_deinit;
    return 0;
}

static int parse(storage_ctx_t *ctx, const char *name, const char **args) {
    int    ret    = 0;
    long   phy    = strtoul(args[0], NULL, 16);
    pos_t *layout = layout_parse(args[1]);
    if (!layout) {
        return EINVAL;
    }
    ret = constructor_memory(ctx, name, phy, layout);
    if (ret) {
        layout_destroy(layout);
    }
    return ret;
}

storage_parseConfig_t memory_parseConfig = {
    .name    = "memory",
    .argName = "<PHY>,<LAYOUT>,",
    .note    = "注册类型为memory的存储。PHY是内存地址，LAYOUT是描述内存布局的json文件",
    .argNum  = 2,
    .parse   = parse,
};
