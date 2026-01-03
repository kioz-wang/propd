/**
 * @file cache.c
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

#include "cache.h"
#include "global.h"
#include "infra/tree.h"
#include "logger/logger.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

struct cache_item {
    const char    *key;
    const value_t *value;
    timestamp_t    modified;
    timestamp_t    duration; /* 值为DURATION_INF时，表示永不过期*/
    RB_ENTRY(cache_item) entry;
};
typedef struct cache_item cache_item_t;

struct cache {
    RB_HEAD(cache_tree, cache_item) tree;
    timestamp_t      min_interval;
    timestamp_t      max_interval;
    timestamp_t      default_duration;
    timestamp_t      min_duration;
    pthread_rwlock_t rwlock;
    sem_t            clean_notice;
    pthread_t        cleaner;
};
typedef struct cache cache_t;

#if !defined(__uintptr_t_defined) && !defined(__uintptr_t)
#if defined(__LP64__) || defined(_LP64)
typedef unsigned long __uintptr_t;
#else
typedef unsigned int __uintptr_t;
#endif
#define __uintptr_t_defined 1
#endif

#define __unused __attribute__((unused))

static int cmp(cache_item_t *a, cache_item_t *b) { return strcmp(a->key, b->key); }

RB_GENERATE_STATIC(cache_tree, cache_item, entry, cmp);

static void item_destroy(cache_item_t *item) {
    if (item) {
        free((void *)item->key);
        free((void *)item->value);
        free(item);
    }
}

static cache_item_t *item_create(const char *key, const value_t *value, timestamp_t duration) {
    cache_item_t *item = (cache_item_t *)malloc(sizeof(cache_item_t));
    if (!item) goto exit;
    if (!(item->key = strdup(key))) goto exit;
    if (!(item->value = value_dup(value))) goto exit;
    item->modified = timestamp(true);
    item->duration = duration;
    return item;
exit:
    item_destroy(item);
    return NULL;
}

#define duration_is_outdate(item, now) (item)->duration != DURATION_INF && (item)->modified + (item)->duration <= (now)

const char *duration_fmt(char *buffer, size_t length, timestamp_t duration) {
    if (DURATION_INF == duration) snprintf(buffer, length, "inf");
    else snprintf(buffer, length, "%ldms", timestamp_to_ms(duration));
    return buffer;
}

static void *cache_cleaner(void *_arg) {
    cache_t    *cache = (cache_t *)_arg;
    timestamp_t last  = 0;

    logfI("[cache::cleaner] start with interval [%ld,%ld], unit: ms", timestamp_to_ms(cache->min_interval),
          timestamp_to_ms(cache->max_interval));

    for (;;) {
        struct timespec ts  = timestamp2spec(feature(false, timestamp_to_ms(cache->max_interval)));
        int             ret = sem_timedwait(&cache->clean_notice, &ts);
        if (!ret) {
            if (timestamp(true) - last < cache->min_interval) {
                logfD("[cache::cleaner] ignore notice");
                continue;
            }
        }

        pthread_rwlock_wrlock(&cache->rwlock);
        cache_item_t *item, *temp;
        last = timestamp(true);
        RB_FOREACH_SAFE(item, cache_tree, &cache->tree, temp) {
            if (duration_is_outdate(item, last)) {
                logfV("[cache::cleaner] clean " logFmtKey, item->key);
                RB_REMOVE(cache_tree, &cache->tree, item);
                item_destroy(item);
            }
        }
        pthread_rwlock_unlock(&cache->rwlock);
    }
    return NULL;
}

void *cache_create(timestamp_t min_interval, timestamp_t max_interval, timestamp_t default_duration,
                   timestamp_t min_duration) {
    cache_t *cache = (cache_t *)calloc(1, sizeof(cache_t));
    if (!cache) return NULL;
    int ret = 0;

    RB_INIT(&cache->tree);
    cache->min_interval     = min_interval;
    cache->max_interval     = max_interval;
    cache->default_duration = default_duration;
    cache->min_duration     = min_duration;

    ret = pthread_rwlock_init(&cache->rwlock, NULL);
    if (ret) {
        logfE("[cache] fail to pthread_rwlock_init" logFmtRet, ret);
        free(cache);
        errno = ret;
        return NULL;
    }
    sem_init(&cache->clean_notice, 0, 0);
    ret = pthread_create(&cache->cleaner, NULL, cache_cleaner, (void *)cache);
    if (ret) {
        logfE("[cache] fail to pthread_create" logFmtRet, ret);
        sem_destroy(&cache->clean_notice);
        pthread_rwlock_destroy(&cache->rwlock);
        free(cache);
        errno = ret;
        return NULL;
    }
    logfI("[cache] created");
    return cache;
}

void cache_destroy(void *_cache) {
    cache_t *cache = _cache;
    if (!cache) return;

    pthread_cancel(cache->cleaner);
    pthread_join(cache->cleaner, NULL);

    sem_destroy(&cache->clean_notice);

    pthread_rwlock_wrlock(&cache->rwlock);
    cache_item_t *item, *temp;
    RB_FOREACH_SAFE(item, cache_tree, &cache->tree, temp) {
        RB_REMOVE(cache_tree, &cache->tree, item);
        item_destroy(item);
    }
    pthread_rwlock_unlock(&cache->rwlock);

    pthread_rwlock_destroy(&cache->rwlock);
    free(cache);
    logfI("[cache] destroyed");
}

int cache_get(void *_cache, const char *key, const value_t **value, timestamp_t *duration) {
    cache_t      *cache  = _cache;
    int           ret    = 0;
    cache_item_t *item   = NULL;
    cache_item_t  shadow = {.key = key};

    pthread_rwlock_rdlock(&cache->rwlock);

    item = RB_FIND(cache_tree, &cache->tree, &shadow);
    if (!item) {
        logfD("[cache] get " logFmtKey " but not found", key);
        ret = ENOENT;
        goto exit;
    }
    timestamp_t now = timestamp(true);
    if (duration_is_outdate(item, now)) {
        logfD("[cache] get " logFmtKey " but out of date, notice cleaner", key);
        sem_post(&cache->clean_notice);
        ret = ENOENT;
        goto exit;
    }

    *value = value_dup(item->value);
    if (!*value) {
        logfE("[cache] get " logFmtKey " but fail to allocate value" logFmtErrno, key, logArgErrno);
        ret = errno;
        goto exit;
    }

    timestamp_t remain = item->duration;
    if (remain != DURATION_INF) {
        timestamp_t _remain = item->duration - (now - item->modified);
        remain              = _remain < cache->min_duration ? cache->min_duration : _remain;
    }
    *duration = remain;

    char buffer[256] = {0};
    char buffer1[32] = {0};
    logfV("[cache] get " logFmtKey " is " logFmtValue " with duration %s", key,
          value_fmt(buffer, sizeof(buffer), *value, false), duration_fmt(buffer1, sizeof(buffer1), remain));

exit:
    pthread_rwlock_unlock(&cache->rwlock);
    return ret;
}

int cache_set(void *_cache, const char *key, const value_t *value, timestamp_t duration) {
    cache_t    *cache = _cache;
    timestamp_t _duration =
        duration ? (duration < cache->min_duration ? cache->min_duration : duration) : cache->default_duration;
    cache_item_t *item = item_create(key, value, _duration);
    if (!item) {
        logfE("[cache] set " logFmtKey " but fail to allocate item" logFmtErrno, key, logArgErrno);
        return errno;
    }
    int ret = 0;

    pthread_rwlock_wrlock(&cache->rwlock);

    cache_item_t *old_item = RB_INSERT(cache_tree, &cache->tree, item);
    if (old_item) {
        item_destroy(item);
        free((void *)old_item->value);
        old_item->value = value_dup(value);
        if (!old_item->value) {
            logfE("[cache] set " logFmtKey " but fail to allocate value" logFmtErrno, key, logArgErrno);
            ret = errno;
            goto exit;
        }
        old_item->modified = timestamp(true);
        old_item->duration = _duration;
    }

    char buffer[256] = {0};
    char buffer1[32] = {0};
    logfV("[cache] set " logFmtKey " as " logFmtValue " with duration %s", key,
          value_fmt(buffer, sizeof(buffer), value, false), duration_fmt(buffer1, sizeof(buffer1), _duration));

exit:
    pthread_rwlock_unlock(&cache->rwlock);
    return ret;
}

int cache_del(void *_cache, const char *key) {
    cache_t      *cache  = _cache;
    int           ret    = 0;
    cache_item_t *item   = NULL;
    cache_item_t  shadow = {.key = key};

    pthread_rwlock_wrlock(&cache->rwlock);

    item = RB_FIND(cache_tree, &cache->tree, &shadow);
    if (!item) {
        logfD("[cache] del " logFmtKey " but not found", key);
        ret = ENOENT;
        goto exit;
    }
    RB_REMOVE(cache_tree, &cache->tree, item);
    item_destroy(item);

    logfV("[cache] del " logFmtKey, key);

exit:
    pthread_rwlock_unlock(&cache->rwlock);
    return ret;
}
