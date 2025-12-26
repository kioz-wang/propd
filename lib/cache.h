/**
 * @file cache.h
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

#ifndef __PROPD_CACHE_H
#define __PROPD_CACHE_H

#include "infra/timestamp.h"
#include "value.h"
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>

#define DURATION_INF (INT64_MAX)

const char *duration_fmt(char *buffer, size_t length, timestamp_t duration);

/**
 * @brief Allocate and initialize a cache, and start a cleaner
 *
 * @param min_interval 主动触发过期回收时的最小间隔
 * @param max_interval 长时间未主动触发过期回收时，将自动执行一次
 * @param default_duration 以下情况中，调整为该值：set时，若传入duration为0
 * @param min_duration 以下情况中，调整为该值：set时，若传入duration不为0且小于；get时，若剩余duration小于
 * @return void* 缓存对象（On error, return NULL and set errno）
 */
void *cache_create(timestamp_t min_interval, timestamp_t max_interval, timestamp_t default_duration,
                   timestamp_t min_duration);
/**
 * @brief Release a cache
 *
 * @param cache 缓存对象（maybe NULL）
 */
void cache_destroy(void *cache);
/**
 * @brief Get value (allocated) and duration of a key
 *
 * @param cache 缓存对象
 * @param key
 * @param value
 * @param duration
 * @return int errno (ENOENT ENOMEM)
 */
int cache_get(void *cache, const char *key, const value_t **value, timestamp_t *duration);
/**
 * @brief Set a key with value (no ownership transfer) and duraion. Update if exist
 *
 * @param cache 缓存对象
 * @param key
 * @param value
 * @param duration
 * 传入0时，设置为dufault_duration；否则小于min_duration时，上调至min_duration；否则若等于DURATION_INF，则永不过期
 * @return int errno (ENOMEM)
 */
int cache_set(void *cache, const char *key, const value_t *value, timestamp_t duration);
/**
 * @brief Delete a key
 *
 * @param cache 缓存对象
 * @param key
 * @return int errno (ENOENT)
 */
int cache_del(void *cache, const char *key);

#endif /* __PROPD_CACHE_H */
