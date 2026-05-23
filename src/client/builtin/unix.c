/**
 * @file unix.c
 * @author kioz.wang (never.had@outlook.com)
 * @brief  Unix-domain-socket storage backend — delegates to a remote propd node.
 * @version 0.2
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
#include "shared/io_package.h"
#include "unix_stream.h"
#include <errno.h>
#include <pthread.h>
#include <stdio.h>

#define logFmtHead "[storage::(unix)] "

struct priv {
    bool shared;
    union {
        const char *target; /* non-shared: server name, re-connected per operation */
        struct {
            us_t             us;    /* shared: persistent connection */
            pthread_mutex_t  mutex;
        };
    };
};
typedef struct priv priv_t;

/* ── helpers ──────────────────────────────────────────────────────────── */

static void io_begin(const us_t *us, io_type_t type, const char *key, const value_t *value) {
    io_package_t pkg_head = {.type = type, .created = timestamp(true)};

    strncpy(pkg_head.key, key, sizeof(pkg_head.key));
    pkg_head.value.type   = value ? value->type : _value_undef;
    pkg_head.value.length = value ? value->length : 0;

    us_write(us, &pkg_head, sizeof(pkg_head));
    logfD(logFmtHead logFmtKey " >>> send header of package with type %d", key, type);

    if (value && value->length) {
        us_write(us, value->data, value->length);
        logfD(logFmtHead logFmtKey " >>> send data of value with length %d", key, value->length);
    }
}

static int io_end(const us_t *us, const char *key) {
    int result = 0;
    if (us_read(us, &result, sizeof(result))) return EIO;
    logfD(logFmtHead logFmtKey " <<< recv result" logFmtRet, key, result);
    return result;
}

/* ── storage operations ────────────────────────────────────────────────── */

static int get(priv_t *priv, const char *key, const value_t **value, timestamp_t *duration) {
    int         ret     = 0;
    timestamp_t _duration;
    value_t    *_value  = NULL;
    us_t        us_local;
    const us_t *us;

    /* acquire stream */
    if (priv->shared) {
        pthread_mutex_lock(&priv->mutex);
        us = &priv->us;
    } else {
        if (us_open_at(&us_local, g_at, priv->target)) return errno;
        us = &us_local;
    }

    io_begin(us, _io_get, key, NULL);

    /* 1. recv duration */
    if (us_read(us, &_duration, sizeof(_duration))) { ret = EIO; goto exit; }
    logfD(logFmtHead logFmtKey " <<< recv duration %ld", key, _duration);

    /* 2. recv value (auto: head + variable-length data in one call) */
    _value = (value_t *)us_read_auto(us, sizeof(value_t));
    if (!_value) { ret = EIO; goto exit; }
    logfD(logFmtHead logFmtKey " <<< recv value with type %d length %d", key, _value->type, _value->length);

    ret = io_end(us, key);
    if (ret) goto exit;

    if (!priv->shared) us_close(&us_local);
    else pthread_mutex_unlock(&priv->mutex);

    *value    = _value;
    *duration = _duration;
    return 0;

exit:
    us_discard_remain(us);
    if (!priv->shared) us_close(&us_local);
    else pthread_mutex_unlock(&priv->mutex);
    free(_value);
    return ret;
}

static int info(priv_t *priv, const char *key, range_t *range, char **help_message, char ***__chain) {
    int    ret          = 0;
    int    chain_length = 0;
    char **chain        = NULL;
    us_t   us_local;
    const us_t *us;

    /* acquire stream */
    if (priv->shared) {
        pthread_mutex_lock(&priv->mutex);
        us = &priv->us;
    } else {
        if (us_open_at(&us_local, g_at, priv->target)) return errno;
        us = &us_local;
    }

    io_begin(us, _io_info, key, NULL);

    /* 1. recv range */
    if (us_read(us, range, sizeof(*range))) { ret = EIO; goto exit; }
    logfD(logFmtHead logFmtKey " <<< recv range", key);

    /* 2. recv cstring choices (if applicable) */
    if (range->type == _value_cstring) {
        char **choice = calloc(range->cstring.num + 1, sizeof(char *));
        if (!choice) { ret = errno; goto exit; }
        for (int i = 0; i < range->cstring.num; i++) {
            choice[i] = us_read_cstring(us);
            if (!choice[i]) { ret = EIO; goto exit; }
            logfD(logFmtHead logFmtKey " <<< recv choice \"%s\"", key, choice[i]);
        }
        range->cstring.choice = choice;
    }

    /* 3. recv help message */
    *help_message = us_read_cstring(us);
    if (!*help_message) { ret = EIO; goto exit; }
    logfD(logFmtHead logFmtKey " <<< recv help message \"%s\"", key, *help_message);

    /* 4. recv chain */
    if (us_read(us, &chain_length, sizeof(chain_length))) { ret = EIO; goto exit; }
    logfD(logFmtHead logFmtKey " <<< recv length of chain %d", key, chain_length);
    chain = calloc(chain_length + 1, sizeof(char *));
    if (!chain) { ret = errno; goto exit; }
    for (int i = 0; i < chain_length; i++) {
        chain[i] = us_read_cstring(us);
        if (!chain[i]) { ret = EIO; goto exit; }
        logfD(logFmtHead logFmtKey " <<< chain \"%s\"", key, chain[i]);
    }
    *__chain = chain;

    ret = io_end(us, key);

exit:
    if (!priv->shared) us_close(&us_local);
    else pthread_mutex_unlock(&priv->mutex);
    return ret;
}

static int set(priv_t *priv, const char *key, const value_t *value) {
    int    ret = 0;
    us_t   us_local;
    const us_t *us;

    /* acquire stream */
    if (priv->shared) {
        pthread_mutex_lock(&priv->mutex);
        us = &priv->us;
    } else {
        if (us_open_at(&us_local, g_at, priv->target)) return errno;
        us = &us_local;
    }

    io_begin(us, _io_set, key, value);
    ret = io_end(us, key);

    if (!priv->shared) us_close(&us_local);
    else pthread_mutex_unlock(&priv->mutex);
    return ret;
}

static int del(priv_t *priv, const char *key) {
    int    ret = 0;
    us_t   us_local;
    const us_t *us;

    /* acquire stream */
    if (priv->shared) {
        pthread_mutex_lock(&priv->mutex);
        us = &priv->us;
    } else {
        if (us_open_at(&us_local, g_at, priv->target)) return errno;
        us = &us_local;
    }

    io_begin(us, _io_del, key, NULL);
    ret = io_end(us, key);

    if (!priv->shared) us_close(&us_local);
    else pthread_mutex_unlock(&priv->mutex);
    return ret;
}

/* ── lifecycle ─────────────────────────────────────────────────────────── */

static void destructor(priv_t *priv) {
    if (priv->shared) {
        us_close(&priv->us);
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
        int ret = us_open_at(&priv->us, g_at, name);
        if (ret) {
            free(priv);
            free((void *)ctx->name);
            return errno;
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

/* ── config parser ─────────────────────────────────────────────────────── */

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
               "server，且不支持'立即缓存'）。TYPE取值temp,long，默认为temp",
    .argNum  = 1,
    .parse   = parse,
};
