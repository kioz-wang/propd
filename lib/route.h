/**
 * @file route.h
 * @author kioz.wang (never.had@outlook.com)
 * @brief
 * @version 0.1
 * @date 2025-11-18
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

#ifndef __PROPD_ROUTE_H
#define __PROPD_ROUTE_H

#include "io.h"
#include <pthread.h>
#include <stdint.h>
#include <sys/queue.h>

struct route_item {
    io_ctx_t    *io_ctx;
    const char **prefix;
    LIST_ENTRY(route_item) entry;
};
typedef struct route_item route_item_t;

/**
 * @brief Allocate and initialize an item of route
 *
 * @param io_ctx
 * @param num_prefix ref. length of arraydup_cstring
 * @param prefix ref. array of arraydup_cstring
 * @return route_item_t*
 */
route_item_t *route_item_create(io_ctx_t *io_ctx, uint32_t num_prefix, const char *prefix[]);
/**
 * @brief Release an item of route
 *
 * @param item （可以传入NULL）
 */
void route_item_destroy(route_item_t *item);

struct route {
    LIST_HEAD(route_list, route_item) list;
    pthread_rwlock_t rwlock;
};
typedef struct route route_t;

/**
 * @brief Allocate and initialize a route
 *
 * @return route_t* 路由表对象
 */
route_t *route_create(void);
/**
 * @brief Release a route
 *
 * @param route 路由表对象（可以传入NULL）
 */
void route_destroy(route_t *route);

/**
 * @brief Register a route item
 *
 * @param route 路由表对象
 * @param io_ctx
 * @param num_prefix ref. length of arraydup_cstring
 * @param prefix ref. array of arraydup_cstring
 * @return int ENOMEM EEXIST
 */
int route_register(route_t *route, io_ctx_t *io_ctx, uint32_t num_prefix, const char *prefix[]);
/**
 * @brief Unregister a route item by name
 *
 * @param route 路由表对象
 * @param name 路由表项的名称。传入NULL时，注销首个表项
 * @return int ENOENT
 */
int route_unregister(route_t *route, const char *name);
/**
 * @brief Get io of the route item that matches key
 *
 * @param route 路由表对象
 * @param key
 * @param io_ctx
 * @return int ENOENT
 */
int route_match(route_t *route, const char *key, io_ctx_t *io_ctx);

#endif /* __PROPD_ROUTE_H */
