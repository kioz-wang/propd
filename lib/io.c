/**
 * @file io.c
 * @author kioz.wang (never.had@outlook.com)
 * @brief
 * @version 0.1
 * @date 2025-12-26
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

#include "io.h"
#include "cache.h"
#include "global.h"
#include "infra/named_mutex.h"
#include "logger/logger.h"
#include "route.h"
#include <errno.h>

struct cleanup_ctx {
    const io_ctx_t       *io;
    const storage_ctx_t **storage;
    const char          **key;
};
typedef struct cleanup_ctx cleanup_ctx_t;

static void cleanup(cleanup_ctx_t *ctx) {
    if (ctx->key) named_mutex_unlock(ctx->io->nmtx_ns, *ctx->key);
    if (ctx->storage) route_deref(*ctx->storage);
}

int io_get(const io_ctx_t *io, const char *key, const value_t **value, timestamp_t *duration) {
    int                  ret         = 0;
    const storage_ctx_t *storage     = NULL;
    cleanup_ctx_t        cleanup_ctx = {.io = io};

    if (io->cache) {
        ret = cache_get(io->cache, key, value, duration);
        if (ret != ENOENT) return ret;
    }

    pthread_cleanup_push((void (*)(void *))cleanup, &cleanup_ctx);

    ret = route_match(io->route, key, &storage);
    if (ret) goto exit;
    cleanup_ctx.storage = &storage;

    ret = named_mutex_lock(io->nmtx_ns, key);
    if (ret) {
        logfE("[server::?] fail to lock " logFmtKey " to get" logFmtRet, key, ret);
        goto exit;
    }
    cleanup_ctx.key = &key;

    ret = storage_get(storage, key, value, duration);
    if (!ret) {
        if (io->cache) cache_set(io->cache, key, *value, *duration);
    }

exit:
    pthread_cleanup_pop(true);
    return ret;
}

int io_update(const io_ctx_t *io, const char *key, const storage_ctx_t *storage) {
    int            ret   = 0;
    const value_t *value = NULL;
    timestamp_t    duration;
    cleanup_ctx_t  cleanup_ctx = {.io = io};

    if (!io->cache) return 0;

    pthread_cleanup_push((void (*)(void *))cleanup, &cleanup_ctx);

    ret = named_mutex_lock(io->nmtx_ns, key);
    if (ret) {
        logfE("[server::?] fail to lock " logFmtKey " to update" logFmtRet, key, ret);
        goto exit;
    }
    cleanup_ctx.key = &key;

    ret = storage_get(storage, key, &value, &duration);
    if (!ret) {
        cache_set(io->cache, key, value, duration);
    }

exit:
    pthread_cleanup_pop(true);
    return ret;
}

int io_set(const io_ctx_t *io, const char *key, const value_t *value) {
    int                  ret         = 0;
    const storage_ctx_t *storage     = NULL;
    cleanup_ctx_t        cleanup_ctx = {.io = io};

    pthread_cleanup_push((void (*)(void *))cleanup, &cleanup_ctx);

    ret = route_match(io->route, key, &storage);
    if (ret) goto exit;
    cleanup_ctx.storage = &storage;

    ret = named_mutex_lock(io->nmtx_ns, key);
    if (ret) {
        logfE("[server::?] fail to lock " logFmtKey " to set" logFmtRet, key, ret);
        goto exit;
    }
    cleanup_ctx.key = &key;

    ret = storage_set(storage, key, value);
    if (!ret) {
        if (io->cache) cache_set(io->cache, key, value, 0);
    }

exit:
    pthread_cleanup_pop(true);
    return ret;
}

int io_del(const io_ctx_t *io, const char *key) {
    int                  ret         = 0;
    const storage_ctx_t *storage     = NULL;
    cleanup_ctx_t        cleanup_ctx = {.io = io};

    pthread_cleanup_push((void (*)(void *))cleanup, &cleanup_ctx);

    ret = route_match(io->route, key, &storage);
    if (ret) goto exit;
    cleanup_ctx.storage = &storage;

    ret = named_mutex_lock(io->nmtx_ns, key);
    if (ret) {
        logfE("[server::?] fail to lock " logFmtKey " to del" logFmtRet, key, ret);
        goto exit;
    }
    cleanup_ctx.key = &key;

    ret = storage_del(storage, key);
    if (!ret) {
        if (io->cache) cache_del(io->cache, key);
    }

exit:
    pthread_cleanup_pop(true);
    return ret;
}
