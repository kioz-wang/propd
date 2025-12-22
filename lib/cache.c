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
#include "logger/logger.h"
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

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

static void *cache_cleaner(void *_arg) {
    cache_t    *cache = (cache_t *)_arg;
    timestamp_t last  = 0;

    for (;;) {
        struct timespec ts  = timestamp2spec(feature(false, timestamp_to_ms(cache->max_interval)));
        int             ret = sem_timedwait(&cache->clean_notice, &ts);
        if (!ret) {
            if (timestamp(true) - last < cache->min_interval) {
                logfD("[cache][cleaner] ignore notice");
                continue;
            }
        }

        pthread_rwlock_wrlock(&cache->rwlock);
        cache_item_t *item, *temp;
        last = timestamp(true);
        RB_FOREACH_SAFE(item, cache_tree, &cache->tree, temp) {
            if (duration_is_outdate(item, last)) {
                logfV("[cache][cleaner] clean <%s>", item->key);
                RB_REMOVE(cache_tree, &cache->tree, item);
                item_destroy(item);
            }
        }
        pthread_rwlock_unlock(&cache->rwlock);
    }
    return NULL;
}

cache_t *cache_create(timestamp_t min_interval, timestamp_t max_interval, timestamp_t default_duration,
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
        return NULL;
    }
    sem_init(&cache->clean_notice, 0, 0);
    ret = pthread_create(&cache->cleaner, NULL, cache_cleaner, (void *)cache);
    if (ret) {
        logfE("[cache] fail to pthread_create" logFmtRet, ret);
        sem_destroy(&cache->clean_notice);
        pthread_rwlock_destroy(&cache->rwlock);
        free(cache);
        return NULL;
    }
    pthread_detach(cache->cleaner);
    logfI("[cache][cleaner] start with interval [%ld,%ld], unit: ms", timestamp_to_ms(min_interval),
          timestamp_to_ms(max_interval));
    logfI("[cache] created");
    return cache;
}

void cache_destroy(cache_t *cache) {
    if (!cache) return;

    pthread_cancel(cache->cleaner);

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

int cache_get(cache_t *cache, const char *key, const value_t **value, timestamp_t *duration) {
    int           ret    = 0;
    cache_item_t *item   = NULL;
    cache_item_t  shadow = {.key = key};

    pthread_rwlock_rdlock(&cache->rwlock);

    item = RB_FIND(cache_tree, &cache->tree, &shadow);
    if (!item) {
        logfD("[cache][get] <%s> not found", key);
        ret = ENOENT;
        goto exit;
    }
    timestamp_t now = timestamp(true);
    if (duration_is_outdate(item, now)) {
        logfD("[cache][get] <%s> out of date, notice cleaner", key);
        sem_post(&cache->clean_notice);
        ret = ENOENT;
        goto exit;
    }

    *value = value_dup(item->value);
    if (!*value) {
        logfE("[cache][get] <%s> fail to allocate value" logFmtErrno, key, logArgErrno);
        ret = ENOMEM;
        goto exit;
    }

    timestamp_t remain = item->duration;
    if (remain != DURATION_INF) {
        timestamp_t _remain = item->duration - (now - item->modified);
        remain              = _remain < cache->min_duration ? cache->min_duration : _remain;
    }
    *duration = remain;

    char buffer[512] = {0};
    logfV("[cache] get <%s> is \"%s\" with duration %ldms", key, value_fmt(buffer, sizeof(buffer), *value, false),
          timestamp_to_ms(remain));

exit:
    pthread_rwlock_unlock(&cache->rwlock);
    return ret;
}

int cache_set(cache_t *cache, const char *key, const value_t *value, timestamp_t duration) {
    timestamp_t _duration =
        duration ? (duration < cache->min_duration ? cache->min_duration : duration) : cache->default_duration;
    cache_item_t *item = item_create(key, value, _duration);
    if (!item) {
        logfE("[cache][set] <%s> fail to allocate item" logFmtErrno, key, logArgErrno);
        return ENOMEM;
    }
    int ret = 0;

    pthread_rwlock_wrlock(&cache->rwlock);

    cache_item_t *old_item = RB_INSERT(cache_tree, &cache->tree, item);
    if (old_item) {
        item_destroy(item);
        free((void *)old_item->value);
        old_item->value = value_dup(value);
        if (!old_item->value) {
            logfE("[cache][set] <%s> fail to allocate value" logFmtErrno, key, logArgErrno);
            ret = ENOMEM;
            goto exit;
        }
        old_item->modified = timestamp(true);
        old_item->duration = _duration;
    }

    char buffer[512] = {0};
    logfV("[cache] set <%s> to \"%s\" with duration %ldms", key, value_fmt(buffer, sizeof(buffer), value, false),
          timestamp_to_ms(_duration));

exit:
    pthread_rwlock_unlock(&cache->rwlock);
    return ret;
}

int cache_del(cache_t *cache, const char *key) {
    int           ret    = 0;
    cache_item_t *item   = NULL;
    cache_item_t  shadow = {.key = key};

    pthread_rwlock_wrlock(&cache->rwlock);

    item = RB_FIND(cache_tree, &cache->tree, &shadow);
    if (!item) {
        logfD("[cache][del] <%s> not found", key);
        ret = ENOENT;
        goto exit;
    }
    RB_REMOVE(cache_tree, &cache->tree, item);
    item_destroy(item);

    logfV("[cache] del <%s>", key);

exit:
    pthread_rwlock_unlock(&cache->rwlock);
    return ret;
}
